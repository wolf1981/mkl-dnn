/*******************************************************************************
* Copyright 2016-2017 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "c_types_map.hpp"
#include "type_helpers.hpp"

#include "gemm_inner_product.hpp"

namespace mkldnn {
namespace impl {
namespace cpu {

// TODO: move BLAS wrappers to a separate header?
#ifdef USE_MKL
#include "mkl_cblas.h"
typedef MKL_INT cblas_int;
#endif

#ifdef USE_CBLAS
namespace {

template <data_type_t data_type>
using data_t = typename prec_trait<data_type>::type;

template <data_type_t data_type>
inline void cblas_gemm(CBLAS_LAYOUT layout,
        CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
        cblas_int M, cblas_int N, cblas_int K,
        data_t<data_type> alpha, const data_t<data_type> *A, cblas_int lda,
        const data_t<data_type> *B, cblas_int ldb,
        data_t<data_type> beta, data_t<data_type> *C, cblas_int ldc);

template <>
inline void cblas_gemm<data_type::f32>(CBLAS_LAYOUT layout,
        CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
        cblas_int M, cblas_int N, cblas_int K,
        float alpha, const float *A, cblas_int lda,
        const float *B, cblas_int ldb,
        float beta, float *C, cblas_int ldc) {
    cblas_sgemm(layout, transa, transb,
            M, N, K, alpha, A, lda, B, ldb, beta, C, ldc);
}

template <data_type_t data_type>
inline void cblas_axpy(cblas_int N,
        data_t<data_type> alpha, const data_t<data_type> *X, cblas_int incx,
        data_t<data_type> *Y, cblas_int incy);

template <>
inline void cblas_axpy<data_type::f32>(cblas_int N,
        float alpha, const float *X, cblas_int incx,
        float *Y, cblas_int incy) {
    cblas_saxpy(N, alpha, X, incx, Y, incy);
}

}
#endif

using namespace mkldnn::impl::status;
using namespace mkldnn::impl::prop_kind;
using namespace mkldnn::impl::data_type;
using namespace mkldnn::impl::memory_format;
using namespace mkldnn::impl::primitive_kind;

template <impl::data_type_t data_type>
void gemm_inner_product_fwd_t<data_type>::execute_forward() {
#ifdef USE_CBLAS
    auto src = reinterpret_cast<const data_t *>(this->input_memory(0));
    auto weights = reinterpret_cast<const data_t *>(this->input_memory(1));
    auto bias = reinterpret_cast<const data_t *>(this->input_memory(2));
    auto dst = reinterpret_cast<data_t*>(this->memory());

    const memory_desc_wrapper dst_d(conf_.dst_pd());
    // TODO: consistency checks
    const cblas_int MB = conf_.MB();
    const cblas_int OC = conf_.OC();
    const cblas_int IC = conf_.IC_total();

    cblas_gemm<data_type>(CblasColMajor, CblasTrans, CblasNoTrans, OC, MB, IC,
            1.0, weights, IC, src, IC, 0.0, dst, OC);
    if (bias)
#       pragma omp parallel for schedule(static)
        for (cblas_int mb = 0; mb < MB; mb++)
            cblas_axpy<data_type>(OC, 1.0, bias, 1, dst + dst_d.blk_off(mb), 1);
#endif
}

template <impl::data_type_t data_type>
void gemm_inner_product_bwd_data_t<data_type>::execute_backward_data() {
#ifdef USE_CBLAS
    auto diff_dst = reinterpret_cast<const data_t *>(this->input_memory(0));
    auto weights = reinterpret_cast<const data_t *>(this->input_memory(1));
    auto diff_src = reinterpret_cast<data_t*>(this->memory());

    // TODO: consistency checks
    const cblas_int MB = conf_.MB();
    const cblas_int OC = conf_.OC();
    const cblas_int IC = conf_.IC_total();

    cblas_gemm<data_type>(CblasColMajor, CblasNoTrans, CblasNoTrans, IC, MB, OC,
            1.0, weights, IC, diff_dst, OC, 0.0, diff_src, IC);
#endif
}

template <impl::data_type_t data_type>
void gemm_inner_product_bwd_weights_t<data_type>::execute_backward_weights() {
#ifdef USE_CBLAS


    auto src = reinterpret_cast<const data_t *>(this->input_memory(0));
    auto diff_dst = reinterpret_cast<const data_t *>(this->input_memory(1));
    auto diff_weights = reinterpret_cast<data_t *>(this->memory(0));
    auto diff_bias = reinterpret_cast<data_t *>(this->memory(1));

    const memory_desc_wrapper diff_dst_d(conf_.diff_dst_pd());
    const memory_desc_wrapper diff_bias_d(conf_.diff_weights_pd(1));

    // TODO: consistency checks
    const cblas_int MB = conf_.MB();
    const cblas_int OC = conf_.OC();
    const cblas_int IC = conf_.IC_total();

    cblas_gemm<data_type>(CblasColMajor, CblasNoTrans, CblasTrans, IC, OC, MB,
            1.0, src, IC, diff_dst, OC, 0.0, diff_weights, IC);


    if (diff_bias) {
#       pragma omp parallel for schedule(static)
        for (int oc = 0; oc < OC; ++oc) {
            data_t *db = &diff_bias[diff_bias_d.off(oc)];
            *db = data_t(0);
            for (int mb = 0; mb < MB; ++mb) {
                *db += diff_dst[diff_dst_d.off(mb, oc)];
            }
        }
    }
#endif
}

template struct gemm_inner_product_fwd_t<data_type::f32>;
template struct gemm_inner_product_bwd_data_t<data_type::f32>;
template struct gemm_inner_product_bwd_weights_t<data_type::f32>;

}
}
}

// vim: et ts=4 sw=4 cindent cino^=l0,\:0,N-s
