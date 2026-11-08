// worker-4: C wrappers for the generated execution module.
// Mirrors inputs/parent-modules/scheduler/unit.c: include the generated C with
// its main renamed, then expose stable C entry points.
#include "bridge.h"
#define main fg107_exec_unused_main
#include "gen/context-execution.c"
#undef main

int64_t fg107_goose_decode(int64_t frame, int64_t batch) {
    return gg_function_1(frame, batch, 0);
}
int64_t fg107_goose_output_reorder(int64_t frame) {
    gg_function_35(frame, 0);
    return 0;
}
int64_t fg107_goose_sched_copy_experts(int64_t frame, int64_t be, int64_t src, int64_t dst, int64_t graph) {
    return gg_function_38(frame, be, src, dst, graph, 0);
}
