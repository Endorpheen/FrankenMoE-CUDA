#include "server-adapter.h"
#include "graph.h"
#include "graph-execute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct giga_bindings model;
static struct giga_request request;
static bool draft_enabled;
static bool trace;
static bool planned_enabled;
static bool planned_prefill_only;
static float previous_hidden[2560];

int32_t giga_server_expand_mla(void) { return giga_expand_mla_weights(&model); }

void giga_server_placement_trace(int32_t kind, int64_t bytes, int64_t available) {
    if (!trace) return;
    if (kind == 1) {
        fprintf(stderr, "GigaChat4: expert VRAM=%lld bytes, available=%lld bytes\n",
                (long long)bytes, (long long)available);
    } else {
        fprintf(stderr, "GigaChat4: %s VRAM=%lld bytes\n",
                kind == 0 ? "dense" : "state logical", (long long)bytes);
    }
}

static bool setting_is(const char *name, const char *value) {
    const char *setting = getenv(name);
    return setting && strcmp(setting, value) == 0;
}

int giga_server_open(const void *mapping, size_t bytes, int64_t capacity) {
    trace = getenv("GOOSE_GIGACHAT4_TRACE") != NULL;
#ifdef _WIN32
    if (!getenv("GOOSE_MMQ") && _putenv_s("GOOSE_MMQ", "1") != 0) return -1;
#else
    if (setenv("GOOSE_MMQ", "1", 0) != 0) return -1;
#endif
    const char *planned = getenv("GOOSE_GIGACHAT4_PLANNED");
    planned_prefill_only = planned && strcmp(planned, "prefill") == 0;
    planned_enabled = !planned || planned_prefill_only || strcmp(planned, "1") == 0;
    if (trace) fputs("GigaChat4: binding model\n", stderr);
    giga_goose_init(0, NULL);
    int status = giga_bind_model(&model, mapping, bytes);
    if (status != 0) return status;
    sl_i64 kinds = {model.kinds, GIGA_LAYER_COUNT};
    status = giga_server_place_weights((sl_u64){model.weights, GIGA_TENSOR_COUNT},
        (sl_i64){model.offsets, GIGA_LAYER_COUNT + 1}, kinds, (sl_f32){model.config.tuning, 6},
        (uintptr_t)mapping, (int64_t)bytes,
        !setting_is("GOOSE_GIGACHAT4_DSA_CUDA", "0"),
        !setting_is("GOOSE_GIGACHAT4_MLA_F32", "0"),
        setting_is("GOOSE_GIGACHAT4_DENSE", "1"),
        setting_is("GOOSE_GIGACHAT4_EXPERTS", "1"));
    if (status != 0) {
        giga_unbind_model(&model);
        return status;
    }
    status = giga_request_init(&request, &model, capacity);
    if (status == 0) {
        const int32_t placed = giga_server_place_state((sl_u64){request.state, 128}, kinds, capacity,
            !setting_is("GOOSE_GIGACHAT4_GDN_STATE", "0"));
        if (placed < 0) status = -1;
        else request.device_state = placed > 0;
    }
    if (status != 0) {
        giga_request_free(&request);
        giga_unbind_model(&model);
    }
    draft_enabled = status == 0 && setting_is("GOOSE_GIGACHAT4_MTP_CATCHUP", "1");
    if (trace) fprintf(stderr, "GigaChat4: model ready status=%d\n", status);
    return status;
}

void giga_server_reset(void) {
    giga_request_reset(&request);
    memset(previous_hidden, 0, sizeof(previous_hidden));
}

static int64_t execute(const int32_t *tokens, int64_t count, int64_t emit,
                       float *logits, const float *draft_input, float *next_input) {
    const bool draft = draft_input != NULL;
    const size_t max_nodes = draft ? 512 : 8192;
    struct ggml_context *ctx = ggml_init((struct ggml_init_params){
        .mem_size = ggml_graph_overhead_custom(max_nodes, false) + max_nodes * ggml_tensor_overhead(),
        .no_alloc = true,
    });
    if (!ctx) return -1;

    struct ggml_tensor *hidden = NULL;
    if (draft) {
        hidden = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 2560, count);
        hidden->data = (void *)draft_input;
        ggml_set_input(hidden);
    }
    struct ggml_cgraph *graph = ggml_new_graph_custom(ctx, max_nodes, false);
    struct giga_step step;
    struct giga_step_output output;
    uint64_t arena = 0;
    int64_t status = giga_step_prepare(&request, ctx, count, emit, draft, &step);
    if (status == 0) {
        step.draft_catchup = next_input != NULL;
        status = giga_step_graph(&request, &model, ctx, graph, &step, hidden, &model.config, &output);
    }
    const bool planned = planned_enabled && !draft && !next_input &&
                         (!planned_prefill_only || count > 1);
    if (status == 0) {
        if (trace) fprintf(stderr, "GigaChat4: computing %lld tokens, draft=%d\n", (long long)count, draft);
        status = giga_execute_step((uintptr_t)&request, (uintptr_t)&step, (uintptr_t)graph,
                                  (uintptr_t)tokens, (uintptr_t)output.logits, planned, (sl_u64){ &arena, 1 },
                                  (sl_i64){step.draft ? &request.draft_committed : &request.committed, 1},
                                  step.base, step.count, request.capacity);
    }
    if (trace) fprintf(stderr, "GigaChat4: step status=%lld\n", (long long)status);
    if (status == 0 && emit > 0) memcpy(logits, output.logits->data, (size_t)emit * 128256 * sizeof(float));
    if (status == 0 && next_input) {
        const int64_t values = count * 2560;
        status = giga_mtp_align((sl_f32){output.hidden->data, values},
            (sl_f32){previous_hidden, 2560}, (sl_f32){next_input, values}, count);
    }
    if (planned) fg_standalone_graph_plan_free((void *)(uintptr_t)arena);
    else free((void *)(uintptr_t)arena);
    ggml_free(ctx);
    return status;
}

int64_t giga_server_execute(uint64_t tokens, int64_t count, int64_t emit,
                            uint64_t logits, uint64_t draft_input, uint64_t next_input) {
    return execute((const void *)(uintptr_t)tokens, count, emit, (void *)(uintptr_t)logits,
                   (const void *)(uintptr_t)draft_input, (void *)(uintptr_t)next_input);
}

int64_t giga_server_step(const int32_t *tokens, int64_t count, int64_t rows,
                         float *logits, int64_t logit_capacity) {
    return giga_run_server_step((uintptr_t)tokens, (sl_i32){(int32_t *)tokens, count}, rows,
                                (uintptr_t)logits, logit_capacity, draft_enabled);
}
