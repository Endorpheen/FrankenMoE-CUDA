// worker-4: source-backed optional bindings for context entry points that may
// not exist in the native header/snapshot pair. Unavailable is logged and
// returns 0; it is never reported as success.
#include "llama-context.h"
#include "llama-model.h"
#include "llama-impl.h"
#include "ggml-backend.h"

#include <type_traits>
#include <utility>

// Newer scheduler entry points: weak C symbols, resolved only if the linked
// ggml provides them.
extern "C" void ggml_backend_sched_set_expert_pipeline(ggml_backend_sched_t, bool) __attribute__((weak));
extern "C" void ggml_backend_sched_set_buffer_growth_limits(ggml_backend_sched_t, const size_t *) __attribute__((weak));

template <typename M, typename = void>
struct fg107_has_pin_experts : std::false_type {};

template <typename M>
struct fg107_has_pin_experts<M, decltype(void(
        std::declval<const M &>().pin_mmap_experts(std::declval<ggml_backend_reg_t>())))> : std::true_type {};

template <typename M>
int fg107_pin_impl(const M * model, ggml_backend_reg_t reg) {
    if constexpr (fg107_has_pin_experts<M>::value) {
        model->pin_mmap_experts(reg);
        return 1;
    } else {
        return 0;
    }
}

extern "C" int fg107_compat_pin_experts(const void * model, void * reg) {
    const int ok = fg107_pin_impl(static_cast<const llama_model *>(model), (ggml_backend_reg_t) reg);
    if (!ok) {
        LLAMA_LOG_WARN("%s: pin_mmap_experts is unavailable in this native header; "
                       "experts remain unpinned (copy callback overlap still active)\n", __func__);
    }
    return ok;
}

extern "C" int fg107_compat_set_expert_pipeline(void * sched, int enabled) {
    if (ggml_backend_sched_set_expert_pipeline) {
        ggml_backend_sched_set_expert_pipeline((ggml_backend_sched_t) sched, enabled != 0);
        return 1;
    }
    if (enabled) {
        LLAMA_LOG_WARN("%s: ggml_backend_sched_set_expert_pipeline is unavailable; "
                       "expert overlap falls back to the MoE copy callback\n", __func__);
    }
    return 0;
}

extern "C" int fg107_compat_set_buffer_growth_limits(void * sched, const size_t * limits) {
    if (ggml_backend_sched_set_buffer_growth_limits) {
        ggml_backend_sched_set_buffer_growth_limits((ggml_backend_sched_t) sched, limits);
        return 1;
    }
    LLAMA_LOG_WARN("%s: ggml_backend_sched_set_buffer_growth_limits is unavailable\n", __func__);
    return 0;
}
