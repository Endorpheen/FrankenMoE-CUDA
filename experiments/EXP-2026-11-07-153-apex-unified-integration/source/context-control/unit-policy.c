// worker-4: C wrappers for the generated policy module.
#include "bridge.h"
#define main fg107_policy_unused_main
#include "gen/context-policy.c"
#undef main

int64_t fg107_goose_resolve_fused(int64_t frame, int64_t mctx, int64_t n_seqs) {
    return gg_function_1(frame, mctx, n_seqs, 0);
}
int64_t fg107_goose_graph_cb_policy(int64_t frame, int64_t n_tokens, int64_t il,
                                    int64_t is_norm, int64_t is_last, int64_t full_offload) {
    return gg_function_6(frame, n_tokens, il, is_norm, is_last, full_offload, 0);
}
int64_t fg107_goose_realloc_policy(int64_t frame, int64_t ids_changed, int64_t size_changed,
                                   int64_t nodes, int64_t splits) {
    return gg_function_8(frame, ids_changed, size_changed, nodes, splits, 0);
}
int64_t fg107_goose_cparams_policy(int64_t frame) {
    return gg_function_10(frame, 0);
}
