// worker-4/active-context-closure: exact pin/expert-pipeline seam.
//
// The active mmap model header (inputs/active-mmap-model/llama-model.h:892)
// declares llama_model::pin_mmap_experts(ggml_backend_reg_t) const; the generic
// U snapshot header omits it. Compile this TU with inputs/active-mmap-model
// before inputs/native/src so the typed member call is source-backed. A generic
// header build falls back to the exact exported Itanium symbol, still typed and
// still no dlsym/cast. Pin is a raw native effect until loader15 owns it, so
// fg107_compat_pin_residual() reports it explicitly.
#include "llama-context.h"
#include "llama-model.h"
#include "llama-impl.h"
#include "ggml-backend.h"

#include <type_traits>
#include <utility>

extern "C" void ggml_backend_sched_set_expert_pipeline(ggml_backend_sched_t, bool) __attribute__((weak));
extern "C" void ggml_backend_sched_set_buffer_growth_limits(ggml_backend_sched_t, const size_t *) __attribute__((weak));

// Exact exported symbol of the active model object; used only when the generic
// header lacks the member declaration. Itanium ABI, typed signature.
extern "C" void fg107_pin_mmap_experts_symbol(const llama_model *, ggml_backend_reg_t)
        __asm__("_ZNK11llama_model16pin_mmap_expertsEP16ggml_backend_reg") __attribute__((weak));

namespace {

template <typename M, typename = void>
struct has_pin : std::false_type {};

template <typename M>
struct has_pin<M, decltype(void(
        std::declval<const M &>().pin_mmap_experts(std::declval<ggml_backend_reg_t>())))> : std::true_type {};

int g_pin_kind = 0;      // 1 = typed member, 2 = exact exported symbol, 0 = unavailable
int g_pin_residual = 0;  // 1 = raw native pin effect (loader15 canonical owner pending)

template <typename M>
int pin_impl(const M * model, ggml_backend_reg_t reg) {
    if constexpr (has_pin<M>::value) {
        model->pin_mmap_experts(reg);
        g_pin_kind = 1;
        return 1;
    } else if (fg107_pin_mmap_experts_symbol != nullptr) {
        fg107_pin_mmap_experts_symbol(model, reg);
        g_pin_kind = 2;
        return 1;
    } else {
        g_pin_kind = 0;
        return 0;
    }
}

} // namespace

extern "C" int fg107_compat_pin_experts(const void * model, void * reg) {
    if (model == nullptr || reg == nullptr) {
        return 0;
    }
    const int ok = pin_impl(static_cast<const llama_model *>(model), (ggml_backend_reg_t) reg);
    if (ok) {
        g_pin_residual = 1;
    } else {
        LLAMA_LOG_WARN("%s: pin_mmap_experts unavailable in both the active header and the linked model object\n", __func__);
    }
    return ok;
}

extern "C" int fg107_compat_set_expert_pipeline(void * sched, int enabled) {
    if (ggml_backend_sched_set_expert_pipeline != nullptr) {
        ggml_backend_sched_set_expert_pipeline((ggml_backend_sched_t) sched, enabled != 0);
        return 1;
    }
    if (enabled) {
        LLAMA_LOG_WARN("%s: ggml_backend_sched_set_expert_pipeline is unavailable\n", __func__);
    }
    return 0;
}

extern "C" int fg107_compat_set_buffer_growth_limits(void * sched, const size_t * limits) {
    if (ggml_backend_sched_set_buffer_growth_limits != nullptr) {
        ggml_backend_sched_set_buffer_growth_limits((ggml_backend_sched_t) sched, limits);
        return 1;
    }
    LLAMA_LOG_WARN("%s: ggml_backend_sched_set_buffer_growth_limits is unavailable\n", __func__);
    return 0;
}

extern "C" int fg107_compat_pin_seam_kind(void) { return g_pin_kind; }
extern "C" int fg107_compat_pin_decl_available(void) { return has_pin<llama_model>::value ? 1 : 0; }
extern "C" int fg107_compat_pin_raw_available(void) {
    return (has_pin<llama_model>::value || fg107_pin_mmap_experts_symbol != nullptr) ? 1 : 0;
}
extern "C" int fg107_compat_pin_residual(void) { return g_pin_residual; }
extern "C" int fg107_compat_expert_pipeline_available(void) {
    return ggml_backend_sched_set_expert_pipeline != nullptr;
}
extern "C" int fg107_compat_buffer_growth_available(void) {
    return ggml_backend_sched_set_buffer_growth_limits != nullptr;
}
