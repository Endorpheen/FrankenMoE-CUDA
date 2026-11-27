#include "model-bindings.h"
#include "graph.h"
#include "ggml.h"
#include <string.h>
#include <stdlib.h>

void giga_unbind_model(struct giga_bindings *bindings) {
    if (!bindings) return;
    if (bindings->mla_weights) ggml_free(bindings->mla_weights);
    if (bindings->descriptors) ggml_free(bindings->descriptors);
    memset(bindings, 0, sizeof(*bindings));
}

uint64_t giga_open_descriptors(void) {
    return (uintptr_t)ggml_init((struct ggml_init_params){
        .mem_size = GIGA_TENSOR_COUNT * ggml_tensor_overhead(), .no_alloc = true,
    });
}

uint64_t giga_create_descriptor(uint64_t ctx, uint64_t data, uint64_t available, sl_i64 metadata) {
    const int64_t *d = metadata.data;
    if (d[0] < 0 || d[0] >= GGML_TYPE_COUNT || d[1] < 1 || d[1] > 4) return 0;
    struct ggml_tensor *tensor = ggml_new_tensor((void *)(uintptr_t)ctx, d[0], d[1], d + 2);
    if (ggml_nbytes(tensor) > available) return 0;
    tensor->data = (void *)(uintptr_t)data;
    return (uintptr_t)tensor;
}

int giga_bind_model(struct giga_bindings *out, const void *mapping, size_t bytes) {
    if (!out || out->descriptors || !mapping || bytes < 24 || bytes > INT64_MAX) return -1;
    const size_t header_limit = 64u << 20;
    struct giga_bindings pending = {0};
    int64_t flags[2];
    uint64_t context;
    const int status = giga_parse_bound_model(
        (sl_u8){(uint8_t *)mapping, bytes < header_limit ? bytes : header_limit},
        (uintptr_t)mapping, (int64_t)bytes, (sl_i64){pending.kinds, GIGA_LAYER_COUNT},
        (sl_i64){pending.offsets, GIGA_LAYER_COUNT + 1}, (sl_u64){pending.weights, GIGA_TENSOR_COUNT},
        (sl_f32){pending.config.tuning, 5}, (sl_f32){pending.config.rope, 6},
        (sl_f32){pending.config.routed_limits, 32}, (sl_f32){pending.config.shared_limits, 33},
        (sl_i64){flags, 2}, (sl_u64){&context, 1});
    if (status != 0) return status;
    pending.descriptors = (void *)(uintptr_t)context;
    memcpy(pending.config.index_rope, pending.config.rope, sizeof(pending.config.rope));
    pending.config.original_context = (int32_t)flags[0];
    pending.config.normalize = flags[1] != 0;
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

uint64_t giga_expand_mla_descriptor(uint64_t ctx, uint64_t address) {
    const struct ggml_tensor *source = (const void *)(uintptr_t)address;
    if (!source || source->type != GGML_TYPE_Q5_0 || source->ne[0] != 128 ||
        source->ne[1] != 512 || source->ne[2] != 32 || source->ne[3] != 1) return 0;
    const int64_t elements = 128 * 512 * 32;
    struct ggml_tensor *target = ggml_new_tensor_3d((void *)(uintptr_t)ctx, GGML_TYPE_F32, 128, 512, 32);
    if (qz_dequant(source->type, (sl_u8){source->data, ggml_nbytes(source)}, elements,
                   (sl_f32){target->data, elements}) != elements) return 0;
    return (uintptr_t)target;
}

int giga_expand_mla_weights(struct giga_bindings *bindings) {
    if (!bindings || bindings->mla_weights) return -1;
    struct ggml_context *ctx = ggml_init((struct ggml_init_params){
        .mem_size = 9 * (ggml_tensor_overhead() + 128u * 512u * 32u * sizeof(float)),
    });
    if (!ctx) return -1;
    if (giga_expand_mla_descriptors((uintptr_t)ctx, (sl_i64){bindings->kinds, GIGA_LAYER_COUNT},
            (sl_i64){bindings->offsets, GIGA_LAYER_COUNT + 1},
            (sl_u64){bindings->weights, GIGA_TENSOR_COUNT}) != 0) {
        ggml_free(ctx);
        return -1;
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
