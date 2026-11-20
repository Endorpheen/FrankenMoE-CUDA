#include "goose-host-llm.h"
#include "graph-execute.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Link with --wrap for ggml_init, ggml_free, fg_standalone_graph_alloc and
// fg_standalone_graph_compute_output to exercise the shipped step ABI.
enum failure { NONE, BEFORE_EXECUTE, AFTER_EXECUTE };
static enum failure failure;
static bool track_frames;
static size_t frame_bytes;
static unsigned opened, closed;

extern void fg_goose_host_ensure_init(void);
extern struct ggml_context * __real_ggml_init(struct ggml_init_params params);
extern void __real_ggml_free(struct ggml_context * ctx);
extern void * __real_fg_standalone_graph_alloc(struct ggml_cgraph * graph);
extern enum ggml_status __real_fg_standalone_graph_compute_output(struct ggml_cgraph * graph,
    const struct ggml_tensor * output);

struct ggml_context * __wrap_ggml_init(struct ggml_init_params params) {
    if (track_frames) {
        assert(params.no_alloc);
        frame_bytes = params.mem_size;
        ++opened;
    }
    return __real_ggml_init(params);
}

void __wrap_ggml_free(struct ggml_context * ctx) {
    if (track_frames) ++closed;
    __real_ggml_free(ctx);
}

void * __wrap_fg_standalone_graph_alloc(struct ggml_cgraph * graph) {
    if (failure == BEFORE_EXECUTE) {
        failure = NONE;
        return NULL;
    }
    return __real_fg_standalone_graph_alloc(graph);
}

enum ggml_status __wrap_fg_standalone_graph_compute_output(struct ggml_cgraph * graph,
    const struct ggml_tensor * output) {
    const enum ggml_status status = __real_fg_standalone_graph_compute_output(graph, output);
    if (failure == AFTER_EXECUTE) {
        assert(status == GGML_STATUS_SUCCESS);
        failure = NONE;
        return GGML_STATUS_FAILED;
    }
    return status;
}

static float * step(const int32_t * ids, int64_t count) {
    float * output = NULL;
    assert(hose_step(ids, count, &output) == count * 4 && output);
    return output;
}

static void reject_step(int32_t token, enum failure where) {
    float * output = NULL;
    failure = where;
    assert(hose_step(&token, 1, &output) == -1 && !output);
    assert(failure == NONE && opened == closed);
}

int main(int argc, char ** argv) {
    assert(argc == 2);
    fg_goose_host_ensure_init();
    hose_host_load_context(argv[1], 256);
    track_frames = true;
    const int32_t prompt[] = {1, 2, 0, 2, 1};
    const int32_t token = 2;
    free(step(prompt, 5));
    float * continued = step(&token, 1);

    hose_reset_state();
    free(step(prompt, 5));
    reject_step(token, BEFORE_EXECUTE);
    float * retried = step(&token, 1);
    assert(memcmp(continued, retried, 4 * sizeof(*continued)) == 0);

    reject_step(token, AFTER_EXECUTE);
    float * reset_after_failure = step(&token, 1);
    hose_reset_state();
    float * reset_reference = step(&token, 1);
    assert(memcmp(reset_after_failure, reset_reference, 4 * sizeof(*reset_reference)) == 0);
    assert(memcmp(continued, reset_reference, 4 * sizeof(*reset_reference)) != 0);
    assert(frame_bytes > 0 && frame_bytes < 256u * 1024u * 1024u && opened == closed && opened == 8);
    printf("Goose step lifecycle: %u graph frames balanced, %zu metadata bytes per frame\n", opened, frame_bytes);

    free(continued);
    free(retried);
    free(reset_after_failure);
    free(reset_reference);
}
