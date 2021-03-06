/*******************************************************************************
* Copyright 2017 Intel Corporation
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

#ifndef JIT_AVX512_MIC_INNER_PRODUCT_HPP
#define JIT_AVX512_MIC_INNER_PRODUCT_HPP

#include <assert.h>

#include "c_types_map.hpp"
#include "cpu_engine.hpp"
#include "cpu_inner_product_pd.hpp"
#include "jit_avx512_mic_gemm_f32.hpp"
#include "type_helpers.hpp"
#include "utils.hpp"

namespace mkldnn {
namespace impl {
namespace cpu {

struct jit_avx512_mic_inner_product_fwd_t : public cpu_primitive_t {
    struct pd_t : public cpu_inner_product_fwd_pd_t {
        pd_t(engine_t *engine, const inner_product_desc_t *adesc,
                const inner_product_fwd_pd_t *hint_fwd_pd)
            : cpu_inner_product_fwd_pd_t(engine, adesc, hint_fwd_pd)
        {
        }

        DECLARE_COMMON_PD_T(jit_avx512_mic_inner_product_fwd_t);

        virtual status_t init() override
        {
            using namespace prop_kind;
            using namespace memory_format;
            using namespace utils;
            if (!mayiuse(avx512_mic)) return status::unimplemented;
            assert(engine()->kind() == engine_kind::cpu);
            bool ok = true && this->set_default_params() == status::success
                    && one_of(desc()->prop_kind, forward_training,
                               forward_inference)
                    && everyone_is(data_type::f32, desc()->src_desc.data_type,
                               desc()->weights_desc.data_type,
                               desc()->dst_desc.data_type)
                    && implication(this->with_bias(),
                               data_type::f32 == desc()->bias_desc.data_type)
                    && true && implication(src_pd_.desc()->format == nChw8c,
                                       weights_pd_.desc()->format == oIhw8i)
                    && implication(src_pd_.desc()->format == nchw,
                               weights_pd_.desc()->format == oihw)
                    && implication(src_pd_.desc()->format == nc,
                               weights_pd_.desc()->format == oi)
                    && dst_pd_.desc()->format == nc && true
                    && memory_desc_wrapper(src_pd()).is_dense()
                    && memory_desc_wrapper(dst_pd()).is_dense()
                    && memory_desc_wrapper(weights_pd()).is_dense();
            return ok ? status::success : status::unimplemented;
        }

    protected:
        virtual status_t set_default_params() override
        {
            using namespace memory_format;
            if (src_pd_.desc()->format == any && ndims() == 4)
                CHECK(src_pd_.set_format(nChw8c));
            if (weights_pd_.desc()->format == any
                    && src_pd_.desc()->format == nChw8c)
                CHECK(weights_pd_.set_format(oIhw8i));
            return cpu_inner_product_fwd_pd_t::set_default_params();
        }
    };

    jit_avx512_mic_inner_product_fwd_t(const pd_t *pd,
            const input_vector &inputs, const output_vector &outputs);
    ~jit_avx512_mic_inner_product_fwd_t();

    typedef typename prec_trait<data_type::f32>::type data_t;

    virtual void execute(event_t *e)
    {
        switch (conf_.desc()->prop_kind) {
        case prop_kind::forward_training:
        case prop_kind::forward_inference: execute_forward(); break;
        default: assert(!"invalid prop_kind");
        }
        e->set_state(event_t::ready);
    }

private:
    void execute_forward();
    pd_t conf_;
    jit_avx512_mic_gemm_f32 *sgemm_;
};
}
}
}

#endif

// vim: et ts=4 sw=4 cindent cino^=l0,\:0,N-s
