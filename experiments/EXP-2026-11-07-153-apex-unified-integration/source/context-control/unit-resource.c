// worker-4: C wrappers for the generated resource/checkpoint module.
#include "bridge.h"
#define main fg107_resource_unused_main
#include "gen/context-resource.c"
#undef main

int64_t fg107_goose_resource_init(int64_t frame) {
    return gg_function_1(frame, 0);
}
int64_t fg107_goose_resource_release(int64_t frame) {
    gg_function_13(frame, 0);
    return 0;
}
int64_t fg107_goose_sched_new(int64_t frame, int64_t max_nodes, int64_t parallel) {
    gg_function_15(frame, max_nodes, parallel, 0);
    return 0;
}
int64_t fg107_goose_sched_reserve(int64_t frame) {
    return gg_function_16(frame, 0);
}
int64_t fg107_goose_state_get_size(int64_t frame) {
    return gg_function_17(frame, 0);
}
int64_t fg107_goose_state_get_data(int64_t frame, int64_t dst, int64_t size) {
    return gg_function_20(frame, dst, size, 0);
}
int64_t fg107_goose_state_set_data(int64_t frame, int64_t src, int64_t size) {
    return gg_function_22(frame, src, size, 0);
}
int64_t fg107_goose_state_seq_get_size(int64_t frame, int64_t seq, int64_t flags) {
    return gg_function_24(frame, seq, flags, 0);
}
int64_t fg107_goose_state_seq_get_data(int64_t frame, int64_t seq, int64_t dst, int64_t size, int64_t flags) {
    return gg_function_26(frame, seq, dst, size, flags, 0);
}
int64_t fg107_goose_state_seq_set_data(int64_t frame, int64_t seq, int64_t src, int64_t size, int64_t flags) {
    return gg_function_28(frame, seq, src, size, flags, 0);
}
