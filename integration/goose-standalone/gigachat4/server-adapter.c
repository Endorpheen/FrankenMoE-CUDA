#include "server-adapter.h"
#include "graph.h"
#include "graph-execute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int64_t fg_cuda_free_bytes(void);

static struct giga_bindings model;
static struct giga_request request;
static bool draft_enabled;
static bool trace;
static bool planned_enabled;
static bool planned_prefill_only;
static float previous_hidden[2560];

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
    const char *dsa = getenv("GOOSE_GIGACHAT4_DSA_CUDA");
    if ((!dsa || strcmp(dsa, "0") != 0) && fg_cuda_free_bytes() > 0) model.config.tuning[5] = 1.0f;
    const char *mla_setting = getenv("GOOSE_GIGACHAT4_MLA_F32");
    const int64_t minimum_mla_vram = INT64_C(6) * 1024 * 1024 * 1024;
    const bool mla = (!mla_setting || strcmp(mla_setting, "0") != 0) &&
                     fg_cuda_free_bytes() >= minimum_mla_vram;
    if (mla && giga_expand_mla_weights(&model) != 0) {
        giga_unbind_model(&model);
        return -1;
    }
    const char *dense = getenv("GOOSE_GIGACHAT4_DENSE");
    if (mla || (dense && strcmp(dense, "1") == 0)) {
        const int64_t reserved = INT64_C(3) * 1024 * 1024 * 1024;
        const int64_t resident = giga_place_dense((sl_u64){model.weights, GIGA_TENSOR_COUNT}, reserved,
            (sl_i64){model.offsets, GIGA_LAYER_COUNT + 1}, (sl_i64){model.kinds, GIGA_LAYER_COUNT}, mla);
        if (resident < 0) {
            giga_unbind_model(&model);
            return -1;
        }
        if (trace) fprintf(stderr, "GigaChat4: dense VRAM=%lld bytes\n", (long long)resident);
    }
    const char *experts = getenv("GOOSE_GIGACHAT4_EXPERTS");
    if (experts && strcmp(experts, "1") == 0) {
        const int64_t reserve = INT64_C(3) * 1024 * 1024 * 1024;
        const int64_t budget = INT64_C(4) * 1024 * 1024 * 1024;
        int64_t memory[2] = {0};
        const int64_t cached = giga_place_experts((sl_u64){model.weights, GIGA_TENSOR_COUNT},
            (sl_i64){model.offsets, GIGA_LAYER_COUNT + 1}, (sl_i64){model.kinds, GIGA_LAYER_COUNT},
            (uintptr_t)mapping, (int64_t)bytes, reserve, budget, (sl_i64){memory, 2});
        if (trace) fprintf(stderr, "GigaChat4: expert VRAM=%lld bytes, available=%lld bytes\n",
                           (long long)cached, (long long)memory[0]);
        if (cached < 0) {
            giga_unbind_model(&model);
            return -1;
        }
    }
    status = giga_request_init(&request, &model, capacity);
    const char *state = getenv("GOOSE_GIGACHAT4_GDN_STATE");
    extern uint8_t fg_cuda_giga_kv_configure(int64_t capacity);
    const bool resident_kv = status == 0 && fg_cuda_giga_kv_configure(capacity);
    if (status == 0 && (!state || strcmp(state, "0") != 0) && fg_cuda_free_bytes() > 0) {
        const int64_t resident = giga_place_gdn_state((sl_u64){request.state, 128},
            (sl_i64){model.kinds, GIGA_LAYER_COUNT}, resident_kv);
        if (trace) fprintf(stderr, "GigaChat4: state logical VRAM=%lld bytes\n", (long long)resident);
        if (resident <= 0) status = -1;
        else request.device_state = true;
    }
    if (status != 0) {
        giga_request_free(&request);
        giga_unbind_model(&model);
    }
    const char *catchup = getenv("GOOSE_GIGACHAT4_MTP_CATCHUP");
    draft_enabled = status == 0 && catchup && strcmp(catchup, "1") == 0;
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
                                  (uintptr_t)tokens, (uintptr_t)output.logits, planned, (sl_u64){ &arena, 1 });
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

int64_t giga_server_step(const int32_t *tokens, int64_t count, int64_t rows,
                         float *logits, int64_t logit_capacity) {
    if (!tokens || count < 1 || count > 4096 || rows < -1 || rows > 1 ||
        (rows == 0 && logit_capacity != 0)) return -1;
    const int64_t emit = rows < 0 ? count : rows;
    if (emit > 0 && (!logits || logit_capacity < emit * 128256)) return -1;
    for (int64_t i = 0; i < count; ++i) {
        if (tokens[i] < 0 || tokens[i] >= 128256) return -1;
    }

    float *aligned = draft_enabled ? malloc((size_t)count * 2560 * sizeof(float)) : NULL;
    if (draft_enabled && !aligned) return -1;
    int64_t status = execute(tokens, count, emit, logits, NULL, aligned);
    if (status == 0 && aligned) status = execute(tokens, count, 0, NULL, aligned, NULL);
    free(aligned);
    if (status != 0) {
        giga_server_reset();
        return status;
    }
    return emit * 128256;
}
