#include "goose-host-llm.h"
#include "graph-execute.h"
#include "main.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Wrap ggml_init, ggml_free, fg_standalone_graph_alloc,
// fg_standalone_graph_compute_output, fg143_graph_resize,
// fg143_storage_ram_available, fg_cuda_routing and fg_q35_generation_eval.
enum failure { NONE, BEFORE_EXECUTE, AFTER_EXECUTE };
static enum failure failure;
static bool track_frames;
static size_t frame_bytes;
static unsigned opened, closed;
static bool fail_workspace_growth;
static unsigned workspaces;
static const int64_t * expected_steps;
static size_t expected_count, step_index;
static bool refuse_graph_ram;
static unsigned verified_gathers;

extern void fg_goose_host_ensure_init(void);
extern struct ggml_context * __real_ggml_init(struct ggml_init_params params);
extern void __real_ggml_free(struct ggml_context * ctx);
extern void * __real_fg_standalone_graph_alloc(struct ggml_cgraph * graph);
extern enum ggml_status __real_fg_standalone_graph_compute_output(struct ggml_cgraph * graph,
    const struct ggml_tensor * output);
extern uint64_t __real_fg143_graph_resize(uint64_t address, int64_t bytes);
extern int64_t __real_fg_q35_generation_eval(sl_i32 tokens, sl_f32 logits);
extern uint64_t __real_fg143_storage_ram_available(void);
extern int64_t __real_fg_cuda_routing(sl_i64 config, sl_u8 x, sl_u8 ids, sl_u8 output);

int64_t __wrap_fg_cuda_routing(sl_i64 config, sl_u8 x, sl_u8 ids, sl_u8 output) {
    if (expected_steps && config.data[0] == 2 && config.data[4] == 32 && config.data[5] == 4) {
        assert(step_index > 0 && ids.len == expected_steps[step_index - 1] * (int64_t) sizeof(int32_t));
        for (int64_t i = 0; i < ids.len / (int64_t) sizeof(int32_t); ++i) {
            assert(((const int32_t *) ids.data)[i] == 2);
        }
        ++verified_gathers;
    }
    return __real_fg_cuda_routing(config, x, ids, output);
}

uint64_t __wrap_fg143_storage_ram_available(void) {
    return refuse_graph_ram ? 1 : __real_fg143_storage_ram_available();
}

int64_t __wrap_fg_q35_generation_eval(sl_i32 tokens, sl_f32 logits) {
    if (expected_steps) {
        assert(step_index < expected_count);
        assert(tokens.len == expected_steps[step_index++]);
    }
    return __real_fg_q35_generation_eval(tokens, logits);
}

uint64_t __wrap_fg143_graph_resize(uint64_t address, int64_t bytes) {
    if (address && bytes > 0 && fail_workspace_growth) {
        fail_workspace_growth = false;
        return 0;
    }
    const uint64_t result = __real_fg143_graph_resize(address, bytes);
    if (address && bytes == 0) {
        assert(workspaces > 0);
        --workspaces;
    } else if (!address && result) {
        ++workspaces;
    }
    return result;
}

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

static void check_workspace_and_scan(void) {
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){1024 * 1024, NULL, true});
    assert(ctx);
    struct ggml_tensor * small = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);
    struct ggml_tensor * large = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 128);
    struct ggml_tensor * sorted = ggml_argsort(ctx, large, GGML_SORT_ORDER_DESC);
    struct ggml_cgraph * graph = ggml_new_graph_custom(ctx, 32, false);
    ggml_build_forward_expand(graph, ggml_argsort(ctx, small, GGML_SORT_ORDER_DESC));
    ggml_build_forward_expand(graph, sorted);

    struct ggml_tensor * state = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, 1, 1, 1, 2);
    struct ggml_tensor * x = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, 1, 1, 2);
    struct ggml_tensor * dt = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, 1, 2, 1);
    struct ggml_tensor * a = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 1, 1);
    struct ggml_tensor * b = ggml_dup_tensor(ctx, x);
    struct ggml_tensor * c = ggml_dup_tensor(ctx, x);
    struct ggml_tensor * ids = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, 1);
    struct ggml_tensor * scan = ggml_ssm_scan(ctx, state, x, dt, a, b, c, ids, 2);
    ggml_build_forward_expand(graph, scan);

    void * storage = fg_standalone_graph_alloc(graph);
    assert(storage);
    for (int i = 0; i < 4; ++i) ((float *) small->data)[i] = (float) i;
    for (int i = 0; i < 128; ++i) ((float *) large->data)[i] = (float) i;
    memcpy(state->data, (float[]){-7.0f, 4.0f}, 2 * sizeof(float));
    memcpy(x->data, (float[]){2.0f, 3.0f}, 2 * sizeof(float));
    memset(dt->data, 0, ggml_nbytes(dt));
    *(float *) a->data = 0.0f;
    memcpy(b->data, (float[]){1.0f, 1.0f}, 2 * sizeof(float));
    memcpy(c->data, (float[]){2.0f, 3.0f}, 2 * sizeof(float));
    *(int32_t *) ids->data = 1;

    fail_workspace_growth = true;
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_FAILED);
    assert(!fail_workspace_growth && workspaces == 0);
    assert(fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS && workspaces == 0);
    for (int i = 0; i < 128; ++i) assert(((int32_t *) sorted->data)[i] == 127 - i);
    const float first = 4.0f + 2.0f * logf(2.0f);
    const float last = first + 3.0f * logf(2.0f);
    const float expected[] = {2.0f * first, 3.0f * last, last, first};
    for (int i = 0; i < 4; ++i) assert(fabsf(((float *) scan->data)[i] - expected[i]) < 2e-6f);
    puts("Goose dispatch recovers after workspace growth fails and preserves both SSM snapshots");
    free(storage);
    ggml_free(ctx);
}

static void check_prefill(const int64_t * prompt, int64_t count, int64_t batch, int64_t ubatch,
                          const int64_t * sizes, size_t size_count) {
    int64_t selected[2];
    assert(hose_configure_prefill(batch, ubatch) == 0);
    expected_steps = sizes;
    expected_count = size_count;
    step_index = 0;
    verified_gathers = 0;
    assert(hose_generate_ids(prompt, count, 2, true, selected, NULL) == 2);
    assert(step_index == expected_count);
    assert(selected[0] == 2 && selected[1] == 2);
    assert(verified_gathers == expected_count);
    expected_steps = NULL;
}

static void check_prefill_profile(const char * path) {
    hose_host_load_context(path, 8192);
    int64_t prompt[4096];
    for (int i = 0; i < 4096; ++i) prompt[i] = 2;
    const int64_t split[] = {3, 2, 3, 2, 2, 1};
    check_prefill(prompt, 12, 5, 3, split, sizeof split / sizeof *split);
    const int64_t boundaries[] = {3, 128, 512, 1024, 2048, 4096};
    for (size_t i = 0; i < sizeof boundaries / sizeof *boundaries; ++i) {
        const int64_t n = boundaries[i];
        const int64_t full[] = {n, 1};
        check_prefill(prompt, n, n, n, full, sizeof full / sizeof *full);
        printf("GET_ROWS IDs verified at batch/ubatch %lld/%lld\n", (long long) n, (long long) n);
    }
    assert(hose_configure_prefill(4096, 4097) == -1);
    int64_t profile[2] = {0};
    hose_prefill_profile(profile);
    assert(profile[0] == 4096 && profile[1] == 4096);
    int64_t selected[2];
    refuse_graph_ram = true;
    assert(hose_generate_ids(prompt, 12, 2, true, selected, NULL) == -1);
    refuse_graph_ram = false;
    check_prefill(prompt, 12, 5, 3, split, sizeof split / sizeof *split);
    puts("Goose prefill profile: 5/3 respects batch boundaries, 4096/4096 emits one 4096-token graph");
}

int main(int argc, char ** argv) {
    assert(argc == 2 || (argc == 3 && strcmp(argv[2], "--profile") == 0));
    fg_goose_host_ensure_init();
    if (argc == 3) {
        check_prefill_profile(argv[1]);
        return 0;
    }
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
    assert(workspaces == 0);
    track_frames = false;
    check_workspace_and_scan();
}
