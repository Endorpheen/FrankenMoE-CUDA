#include "main.h"
#include "graph-execute.h"

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <time.h>

extern int64_t __real_fg_q35_generation_prepare(void);
extern int64_t __real_fg_q35_generation_eval(sl_i32, sl_f32, uint8_t);
extern void __real_fg_http_stats(sl_i64);
extern void __real_ggml_build_forward_expand(struct ggml_cgraph *, struct ggml_tensor *);
extern struct fg_standalone_graph_plan * __real_fg_standalone_graph_plan_alloc(
    struct ggml_cgraph *, const struct ggml_tensor *);
extern void __real_fg_standalone_graph_plan_free(struct fg_standalone_graph_plan *);
extern enum ggml_status __real_fg_standalone_graph_plan_compute(struct fg_standalone_graph_plan *);

static _Thread_local struct {
    uint64_t last_eval_end, gap_ns, eval_ns, expand_ns, alloc_ns, execute_ns, free_ns, calls;
    bool decode, reported;
} timing;

static uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t) t.tv_sec * UINT64_C(1000000000) + (uint64_t) t.tv_nsec;
}

static void report(void) {
    if (!timing.calls || timing.reported) return;
    fprintf(stderr, "Apex decode boundaries: calls=%" PRIu64 " gap_ns=%" PRIu64 " eval_ns=%" PRIu64
        " expand_ns=%" PRIu64 " alloc_ns=%" PRIu64 " execute_ns=%" PRIu64 " free_ns=%" PRIu64 "\n",
        timing.calls, timing.gap_ns, timing.eval_ns, timing.expand_ns, timing.alloc_ns,
        timing.execute_ns, timing.free_ns);
    timing.reported = true;
}

int64_t __wrap_fg_q35_generation_prepare(void) {
    report();
    timing.last_eval_end = timing.gap_ns = timing.eval_ns = timing.expand_ns = 0;
    timing.alloc_ns = timing.execute_ns = timing.free_ns = timing.calls = 0;
    timing.decode = timing.reported = false;
    return __real_fg_q35_generation_prepare();
}

int64_t __wrap_fg_q35_generation_eval(sl_i32 tokens, sl_f32 output, uint8_t prefill) {
    const uint64_t begin = now_ns();
    timing.decode = !prefill;
    if (timing.decode) {
        ++timing.calls;
        if (timing.last_eval_end) timing.gap_ns += begin - timing.last_eval_end;
    }
    const int64_t result = __real_fg_q35_generation_eval(tokens, output, prefill);
    timing.last_eval_end = now_ns();
    if (timing.decode) timing.eval_ns += timing.last_eval_end - begin;
    timing.decode = false;
    return result;
}

void __wrap_fg_http_stats(sl_i64 stats) {
    report();
    __real_fg_http_stats(stats);
}

void __wrap_ggml_build_forward_expand(struct ggml_cgraph * graph, struct ggml_tensor * root) {
    const uint64_t start = timing.decode ? now_ns() : 0;
    __real_ggml_build_forward_expand(graph, root);
    if (start) timing.expand_ns += now_ns() - start;
}

struct fg_standalone_graph_plan * __wrap_fg_standalone_graph_plan_alloc(
        struct ggml_cgraph * graph, const struct ggml_tensor * output) {
    const uint64_t start = timing.decode ? now_ns() : 0;
    struct fg_standalone_graph_plan * plan = __real_fg_standalone_graph_plan_alloc(graph, output);
    if (start) timing.alloc_ns += now_ns() - start;
    return plan;
}

enum ggml_status __wrap_fg_standalone_graph_plan_compute(struct fg_standalone_graph_plan * plan) {
    const uint64_t start = timing.decode ? now_ns() : 0;
    const enum ggml_status status = __real_fg_standalone_graph_plan_compute(plan);
    if (start) timing.execute_ns += now_ns() - start;
    return status;
}

void __wrap_fg_standalone_graph_plan_free(struct fg_standalone_graph_plan * plan) {
    const uint64_t start = timing.decode ? now_ns() : 0;
    __real_fg_standalone_graph_plan_free(plan);
    if (start) timing.free_ns += now_ns() - start;
}
