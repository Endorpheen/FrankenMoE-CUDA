#include "model-bindings.h"
#include "graph.h"
#include "ggml.h"
#include <string.h>
#include <stdlib.h>

#define WORDS(array) ((sl_i64){ (array), sizeof(array) / sizeof((array)[0]) })

void giga_unbind_model(struct giga_bindings *bindings) {
    if (!bindings) return;
    if (bindings->mla_weights) ggml_free(bindings->mla_weights);
    if (bindings->descriptors) ggml_free(bindings->descriptors);
    memset(bindings, 0, sizeof(*bindings));
}

int giga_bind_model(struct giga_bindings *out, const void *mapping, size_t bytes) {
    if (!out || out->descriptors || !mapping || bytes < 24 || bytes > INT64_MAX) return -1;

    const size_t header_limit = 64u << 20;
    sl_u8 data = { (uint8_t *)mapping, bytes < header_limit ? bytes : header_limit };
    int64_t state[8], kv[8 * 512], tensors[12 * 2048], kv_slots[1024], slots[4096];
    int64_t info[12], ids[GIGA_TENSOR_COUNT];
    struct giga_bindings pending = {0};

    if (gr_parse(data, bytes, WORDS(state), WORDS(kv), WORDS(tensors),
                 WORDS(kv_slots), WORDS(slots)) != 0 ||
        giga_read_layout(data, WORDS(state), WORDS(kv), WORDS(kv_slots),
                         WORDS(info), WORDS(pending.kinds)) != 0 ||
        giga_bind_tensors(data, WORDS(state), WORDS(tensors), WORDS(slots),
                          WORDS(pending.kinds), WORDS(ids), WORDS(pending.offsets)) != GIGA_TENSOR_COUNT) {
        return -2;
    }

    float limits[65];
    int64_t flags[2];
    if (giga_read_config(data, WORDS(state), WORDS(kv), WORDS(kv_slots),
                         (sl_f32){pending.config.tuning, 5}, (sl_f32){pending.config.rope, 6},
                         (sl_f32){limits, 65}, WORDS(flags)) != 0) return -5;
    memcpy(pending.config.routed_limits, limits, sizeof(pending.config.routed_limits));
    memcpy(pending.config.shared_limits, limits + 32, sizeof(pending.config.shared_limits));
    memcpy(pending.config.index_rope, pending.config.rope, sizeof(pending.config.rope));
    pending.config.original_context = (int32_t)flags[0];
    pending.config.normalize = flags[1] != 0;

    pending.descriptors = ggml_init((struct ggml_init_params){
        .mem_size = GIGA_TENSOR_COUNT * ggml_tensor_overhead(),
        .no_alloc = true,
    });
    if (!pending.descriptors) return -3;

    for (size_t i = 0; i < GIGA_TENSOR_COUNT; ++i) {
        int64_t d[9];
        if (gr_tensor_info(WORDS(state), WORDS(tensors), ids[i], WORDS(d)) != 0 ||
            d[0] < 0 || d[0] >= GGML_TYPE_COUNT || d[1] < 1 || d[1] > 4 ||
            d[8] < 0 || (uint64_t)d[8] >= bytes) {
            giga_unbind_model(&pending);
            return -4;
        }
        struct ggml_tensor *tensor = ggml_new_tensor(pending.descriptors, d[0], d[1], &d[2]);
        if (ggml_nbytes(tensor) > bytes - (size_t)d[8]) {
            giga_unbind_model(&pending);
            return -4;
        }
        tensor->data = (uint8_t *)mapping + d[8];
        pending.weights[i] = (uintptr_t)tensor;
    }
    *out = pending;
    return 0;
}

void fg457_st_i64(uint64_t address, uint64_t offset, int64_t value) {
    memcpy((uint8_t *)(uintptr_t)address + offset, &value, sizeof(value));
}

float giga_bits_to_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

int64_t giga_dense_span(uint64_t address, sl_i64 out) {
    const struct ggml_tensor *tensor = (const void *)(uintptr_t)address;
    if (!tensor || !tensor->data || out.len < 2 || tensor->ne[2] != 1 || tensor->ne[3] != 1 ||
        (tensor->type != GGML_TYPE_F32 && tensor->type != GGML_TYPE_Q4_K &&
         tensor->type != GGML_TYPE_Q6_K)) return 0;
    out.data[0] = (int64_t)(uintptr_t)tensor->data;
    out.data[1] = (int64_t)ggml_nbytes(tensor);
    return 1;
}

extern int64_t qz_dequant(int64_t type, sl_u8 src, int64_t elements, sl_f32 out);

int giga_expand_mla_weights(struct giga_bindings *bindings) {
    if (!bindings || bindings->mla_weights) return -1;
    const int64_t elements = 128 * 512 * 32;
    struct ggml_context *ctx = ggml_init((struct ggml_init_params){
        .mem_size = 9 * (ggml_tensor_overhead() + (size_t)elements * sizeof(float)),
    });
    if (!ctx) return -1;
    uint64_t expanded[33] = {0};
    for (int layer = 0; layer < 33; ++layer) {
        if (bindings->kinds[layer] == 0) continue;
        int64_t slot = bindings->offsets[layer] + 16;
        const struct ggml_tensor *source = (const void *)(uintptr_t)bindings->weights[slot];
        if (!source || source->type != GGML_TYPE_Q5_0 || source->ne[0] != 128 ||
            source->ne[1] != 512 || source->ne[2] != 32 || source->ne[3] != 1) {
            ggml_free(ctx);
            return -1;
        }
        struct ggml_tensor *target = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, 128, 512, 32);
        if (qz_dequant(source->type, (sl_u8){source->data, ggml_nbytes(source)}, elements,
                       (sl_f32){target->data, elements}) != elements) {
            ggml_free(ctx);
            return -1;
        }
        expanded[layer] = (uintptr_t)target;
    }
    for (int layer = 0; layer < 33; ++layer) {
        if (expanded[layer]) bindings->weights[bindings->offsets[layer] + 16] = expanded[layer];
    }
    bindings->mla_weights = ctx;
    return 0;
}

uint64_t giga_dsa_mask_node(uint64_t context, uint64_t original, uint64_t selected) {
    const char *enabled = getenv("GOOSE_GIGA_DSA_MASK");
    if (enabled && strcmp(enabled, "0") == 0) return 0;
    struct ggml_tensor *mask = (void *)(uintptr_t)original;
    struct ggml_tensor *picks = (void *)(uintptr_t)selected;
    struct ggml_tensor *out = ggml_new_tensor_2d((void *)(uintptr_t)context, GGML_TYPE_F16,
        mask->ne[0], mask->ne[1]);
    out->op = GGML_OP_CUSTOM;
    out->src[0] = mask;
    out->src[1] = picks;
    out->op_params[0] = 0x4453414d;
    return (uintptr_t)out;
}

uint8_t giga_dsa_prefix_enabled(void) {
    const char * prefix = getenv("GOOSE_GIGA_DSA_PREFIX");
    const char * mask = getenv("GOOSE_GIGA_DSA_MASK");
    return (!prefix || strcmp(prefix, "0") != 0) && (!mask || strcmp(mask, "0") != 0);
}
