#pragma once
// worker-4: routing ABI between the patched native context TU and the generated
// Goose modules. Default-on: these calls are active unless the build defines
// FG107_NATIVE_ROLLBACK.
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// execution
int64_t fg107_goose_decode(int64_t frame, int64_t batch);
// resource / state
int64_t fg107_goose_resource_init(int64_t frame);
int64_t fg107_goose_sched_reserve(int64_t frame);
int64_t fg107_goose_sched_new(int64_t frame, int64_t max_nodes, int64_t parallel);
int64_t fg107_goose_resource_release(int64_t frame);
int64_t fg107_goose_state_get_size(int64_t frame);
int64_t fg107_goose_state_get_data(int64_t frame, int64_t dst, int64_t size);
int64_t fg107_goose_state_set_data(int64_t frame, int64_t src, int64_t size);
int64_t fg107_goose_state_seq_get_size(int64_t frame, int64_t seq, int64_t flags);
int64_t fg107_goose_state_seq_get_data(int64_t frame, int64_t seq, int64_t dst, int64_t size, int64_t flags);
int64_t fg107_goose_state_seq_set_data(int64_t frame, int64_t seq, int64_t src, int64_t size, int64_t flags);
// policy/control
int64_t fg107_goose_resolve_fused(int64_t frame, int64_t mctx, int64_t n_seqs);
int64_t fg107_goose_graph_cb_policy(int64_t frame, int64_t n_tokens, int64_t il,
                                    int64_t is_norm, int64_t is_last, int64_t full_offload);
int64_t fg107_goose_realloc_policy(int64_t frame, int64_t ids_changed, int64_t size_changed,
                                   int64_t nodes, int64_t splits);
int64_t fg107_goose_cparams_policy(int64_t frame);

// optional native bindings (source-backed; unavailable is explicit, never a
// silent success)
int fg107_compat_pin_experts(const void * model, void * reg);
int fg107_compat_set_expert_pipeline(void * sched, int enabled);
int fg107_compat_set_buffer_growth_limits(void * sched, const size_t * limits);

#ifdef __cplusplus
}
#endif

// The context snapshot may reference scheduler entry points that are newer than
// the native header snapshot. Route them through the optional bindings above.
#ifndef FG107_NO_COMPAT_MACROS
#define ggml_backend_sched_set_expert_pipeline(s, v) \
    fg107_compat_set_expert_pipeline((void *) (s), (v) ? 1 : 0)
#define ggml_backend_sched_set_buffer_growth_limits(s, l) \
    fg107_compat_set_buffer_growth_limits((void *) (s), (l))
#endif
