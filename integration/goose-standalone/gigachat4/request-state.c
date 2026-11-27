#include "request-state.h"
#include "graph.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void giga_request_free(struct giga_request *request) {
    if (!request) return;
    if (request->descriptors) ggml_free(request->descriptors);
    memset(request, 0, sizeof(*request));
}

int giga_request_init(struct giga_request *request, const struct giga_bindings *model,
                      int64_t capacity) {
    if (!request || request->descriptors || !model || !model->descriptors) return -1;
    const uint64_t bytes = giga_request_bytes(capacity);
    if (!bytes) return -1;
    struct giga_request pending = {.capacity = capacity};
    pending.descriptors = ggml_init((struct ggml_init_params){.mem_size = (size_t)bytes});
    if (!pending.descriptors) return -2;
    if (giga_request_allocate_state((uintptr_t)pending.descriptors, capacity,
            (sl_i64){(int64_t *)model->kinds, 32}, (sl_u64){pending.state, 128},
            (sl_u64){pending.draft, 3}) != 0) {
        giga_request_free(&pending);
        return -1;
    }
    giga_request_reset(&pending);
    *request = pending;
    return 0;
}

void giga_request_reset(struct giga_request *request) {
    if (!request || !request->descriptors) return;
    const int64_t cursor = giga_request_clear_state((uintptr_t)request->descriptors, request->device_state);
    request->committed = request->draft_committed = cursor;
}

int giga_step_prepare(const struct giga_request *request, struct ggml_context *ctx,
                      int64_t count, int64_t output_count, bool draft, struct giga_step *step) {
    if (!request || !ctx || !step) return -1;
    const int64_t base = draft ? request->draft_committed : request->committed;
    uint64_t tensors[5];
    const int64_t cells = giga_prepare_step_inputs((uintptr_t)ctx, request->capacity,
        base, count, output_count, (sl_u64){tensors, 5});
    if (cells < 0) return -1;
    *step = (struct giga_step){
        .tokens = (void *)(uintptr_t)tensors[0],
        .positions = (void *)(uintptr_t)tensors[1],
        .rows = (void *)(uintptr_t)tensors[2],
        .mask = (void *)(uintptr_t)tensors[3],
        .output_ids = (void *)(uintptr_t)tensors[4],
        .draft = draft, .base = base, .count = count, .output_count = output_count, .cells = cells,
    };
    return 0;
}

int giga_step_upload(const struct giga_step *step, const int32_t *tokens) {
    if (!step || !tokens || !step->tokens || !step->tokens->data || !step->positions->data || !step->rows->data ||
        !step->mask->data || (step->output_ids && !step->output_ids->data)) return -1;
    const size_t count = (size_t)step->count;
    const size_t cells = (size_t)step->cells;
    const sl_i32 output_ids = step->output_ids
        ? (sl_i32){step->output_ids->data, step->output_count} : (sl_i32){0};
    giga_fill_step_inputs(step->base, step->count, step->cells,
        (sl_i32){step->positions->data, step->count},
        (sl_i64){step->rows->data, step->count},
        (sl_u16){step->mask->data, (int64_t)(count * cells)}, output_ids);
    memcpy(step->tokens->data, tokens, count * sizeof(*tokens));
    return 0;
}

#define WORDS(array) ((sl_i64){ (array), sizeof(array) / sizeof((array)[0]) })
#define PTRS(array)  ((sl_u64){ (array), sizeof(array) / sizeof((array)[0]) })

int giga_step_graph(struct giga_request *request, struct giga_bindings *model,
                    struct ggml_context *ctx, struct ggml_cgraph *graph,
                    const struct giga_step *step, struct ggml_tensor *draft_hidden,
                    const struct giga_graph_config *config, struct giga_step_output *output) {
    if (!request || !model || !ctx || !graph || !step || !config || !output ||
        (step->draft && !draft_hidden)) return -1;

    uint64_t batch[] = { (uintptr_t)step->tokens, (uintptr_t)step->positions,
        (uintptr_t)step->rows, (uintptr_t)step->mask, (uintptr_t)step->output_ids };
    int64_t shape[] = { step->count, step->cells, request->capacity, 1,
        step->output_count, config->original_context, step->draft_catchup };
    uint64_t strides[] = { ggml_row_size(GGML_TYPE_F16, 576), ggml_row_size(GGML_TYPE_F16, 128) };
    struct giga_graph_config options = *config;
    sl_f32 tuning = { options.tuning, 6 };
    sl_f32 rope = { options.rope, 6 };
    sl_f32 index_rope = { options.index_rope, 6 };
    uint64_t result[2];
    int64_t status;
    if (step->draft) {
        const int64_t first = model->offsets[32];
        const sl_u64 weights = { model->weights + first, model->offsets[33] - first };
        status = giga_mtp_graph((uintptr_t)ctx, (uintptr_t)graph, (uintptr_t)draft_hidden,
            weights, PTRS(request->draft), PTRS(batch), WORDS(shape), PTRS(strides), tuning,
            options.shared_limits[32], options.factors[32], rope, index_rope, PTRS(result));
    } else {
        status = giga_decoder_graph((uintptr_t)ctx, (uintptr_t)graph, PTRS(model->weights),
            WORDS(model->offsets), WORDS(model->kinds), PTRS(request->state), PTRS(batch),
            WORDS(shape), PTRS(strides), tuning, (sl_f32){ options.routed_limits, 32 },
            (sl_f32){ options.shared_limits, 32 }, (sl_u64){ options.factors, 32 },
            rope, index_rope, options.normalize, PTRS(result));
    }
    if (status != 0) return (int)status;

    output->hidden = (void *)(uintptr_t)result[0];
    output->logits = (void *)(uintptr_t)result[1];
    ggml_set_output(output->hidden);
    if (output->logits) ggml_set_output(output->logits);
    return 0;
}

int64_t giga_state_span(uint64_t address, sl_i64 out) {
    const struct ggml_tensor *tensor = (const void *)(uintptr_t)address;
    if (!tensor || !tensor->data || out.len < 2 || (tensor->type != GGML_TYPE_F32 && tensor->type != GGML_TYPE_F16) ||
        !ggml_is_contiguous(tensor)) return 0;
    out.data[0] = (int64_t)(uintptr_t)tensor->data;
    out.data[1] = (int64_t)ggml_nbytes(tensor);
    return 1;
}
