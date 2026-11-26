#include "request-state.h"
#include "graph.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern int64_t fg_cuda_reset_model_state(void);

static void allocate_mla(struct ggml_context *ctx, int64_t capacity, uint64_t state[3]) {
    state[0] = (uintptr_t)ggml_new_tensor_2d(ctx, GGML_TYPE_F16, 576, capacity);
    state[1] = (uintptr_t)ggml_new_tensor_2d(ctx, GGML_TYPE_F16, 128, capacity);
}

void giga_request_free(struct giga_request *request) {
    if (!request) return;
    if (request->descriptors) ggml_free(request->descriptors);
    memset(request, 0, sizeof(*request));
}

int giga_request_init(struct giga_request *request, const struct giga_bindings *model,
                      int64_t capacity) {
    if (!request || request->descriptors || !model || !model->descriptors ||
        capacity < 1 || capacity > 1048576) return -1;
    struct giga_request pending = {0};
    pending.capacity = capacity;
    pending.descriptors = ggml_init((struct ggml_init_params){
        .mem_size = 66 * ggml_tensor_overhead() + 24u * (3u * 8192u + 128u * 128u * 32u) * sizeof(float)
                    + (size_t)capacity * 9u * (576u + 128u) * sizeof(uint16_t),
    });
    if (!pending.descriptors) return -2;

    for (int layer = 0; layer < 32; ++layer) {
        uint64_t *state = pending.state + layer * 4;
        if (model->kinds[layer] == 0) {
            state[0] = (uintptr_t)ggml_new_tensor_3d(pending.descriptors, GGML_TYPE_F32, 3, 8192, 1);
            state[1] = (uintptr_t)ggml_new_tensor_4d(pending.descriptors, GGML_TYPE_F32, 128, 128, 32, 1);
            state[2] = state[0];
            state[3] = state[1];
        } else if (model->kinds[layer] == 1) {
            allocate_mla(pending.descriptors, capacity, state);
        } else {
            giga_request_free(&pending);
            return -1;
        }
    }
    allocate_mla(pending.descriptors, capacity, pending.draft);
    giga_request_reset(&pending);
    *request = pending;
    return 0;
}

void giga_request_reset(struct giga_request *request) {
    if (!request || !request->descriptors) return;
    for (struct ggml_tensor *tensor = ggml_get_first_tensor(request->descriptors);
         tensor; tensor = ggml_get_next_tensor(request->descriptors, tensor)) {
        memset(tensor->data, 0, ggml_nbytes(tensor));
    }
    if (request->device_state && fg_cuda_reset_model_state() != 0) {
        request->committed = -1;
        request->draft_committed = -1;
        return;
    }
    request->committed = 0;
    request->draft_committed = 0;
}

int giga_step_prepare(const struct giga_request *request, struct ggml_context *ctx,
                      int64_t count, int64_t output_count, bool draft, struct giga_step *step) {
    if (!request || !ctx || !step || count < 1 || count > 4096 ||
        output_count < 0 || output_count > count) return -1;
    const int64_t base = draft ? request->draft_committed : request->committed;
    if (base < 0 || count > request->capacity - base) return -1;

    const int64_t live_cells = base + count;
    const int64_t padded_cells = (live_cells + 31) & ~INT64_C(31);
    *step = (struct giga_step){
        .tokens = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, count),
        .positions = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, count),
        .rows = ggml_new_tensor_1d(ctx, GGML_TYPE_I64, count),
        .mask = ggml_new_tensor_4d(ctx, GGML_TYPE_F32,
                                   padded_cells <= request->capacity ? padded_cells : live_cells,
                                   count, 1, 1),
        .output_ids = output_count > 0 && output_count < count ? ggml_new_tensor_1d(ctx, GGML_TYPE_I32, output_count) : NULL,
        .draft = draft,
        .base = base,
        .count = count,
        .output_count = output_count,
        .cells = padded_cells <= request->capacity ? padded_cells : live_cells,
    };
    ggml_set_input(step->tokens);
    ggml_set_input(step->positions);
    ggml_set_input(step->rows);
    ggml_set_input(step->mask);
    if (step->output_ids) ggml_set_input(step->output_ids);
    return 0;
}

int giga_step_upload(const struct giga_step *step, const int32_t *tokens) {
    if (!step || !tokens || !step->tokens || !step->tokens->data || !step->positions->data || !step->rows->data ||
        !step->mask->data || (step->output_ids && !step->output_ids->data)) return -1;
    const size_t count = (size_t)step->count;
    const size_t cells = (size_t)step->cells;
    int32_t positions[4096];
    int64_t rows[4096];
    float *mask = step->mask->data;
    for (size_t row = 0; row < count; ++row) {
        positions[row] = (int32_t)(step->base + row);
        rows[row] = positions[row];
        for (size_t cell = 0; cell < cells; ++cell) {
            mask[row * cells + cell] = cell <= (size_t)positions[row] ? 0.0f : -INFINITY;
        }
    }
    memcpy(step->tokens->data, tokens, count * sizeof(*tokens));
    memcpy(step->positions->data, positions, count * sizeof(*positions));
    memcpy(step->rows->data, rows, count * sizeof(*rows));
    if (step->output_ids) {
        for (int64_t i = 0; i < step->output_count; ++i) {
            positions[i] = (int32_t)(step->count - step->output_count + i);
        }
        memcpy(step->output_ids->data, positions,
                                (size_t)step->output_count * sizeof(*positions));
    }
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

int giga_step_commit(struct giga_request *request, const struct giga_step *step) {
    if (!request || !step) return -1;
    int64_t *cursor = step->draft ? &request->draft_committed : &request->committed;
    if (*cursor != step->base || step->count < 1 ||
        step->count > request->capacity - *cursor) return -1;
    *cursor += step->count;
    return 0;
}

int64_t giga_state_span(uint64_t address, sl_i64 out) {
    const struct ggml_tensor *tensor = (const void *)(uintptr_t)address;
    if (!tensor || !tensor->data || out.len < 2 || tensor->type != GGML_TYPE_F32 ||
        !ggml_is_contiguous(tensor)) return 0;
    out.data[0] = (int64_t)(uintptr_t)tensor->data;
    out.data[1] = (int64_t)ggml_nbytes(tensor);
    return 1;
}
