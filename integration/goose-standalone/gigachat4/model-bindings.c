#include "request-state.h"
#include "graph.h"
#include "ggml.h"
#include <string.h>
#include <stdlib.h>

_Static_assert(sizeof(struct giga_request) == 1088 &&
    offsetof(struct giga_request, state) == 8 && offsetof(struct giga_request, draft) == 1032 &&
    offsetof(struct giga_request, capacity) == 1056 && offsetof(struct giga_request, committed) == 1064 &&
    offsetof(struct giga_request, draft_committed) == 1072 && offsetof(struct giga_request, device_state) == 1080 &&
    offsetof(struct giga_bindings, kinds) == 8576, "GigaChat4 request ABI");

_Static_assert(sizeof(struct giga_step) == 80 &&
    offsetof(struct giga_step, output_ids) == 32 && offsetof(struct giga_step, base) == 40 &&
    offsetof(struct giga_step, count) == 48 && offsetof(struct giga_step, output_count) == 56 &&
    offsetof(struct giga_step, cells) == 64 && offsetof(struct giga_step, draft) == 72 &&
    offsetof(struct giga_step, draft_catchup) == 73, "GigaChat4 step ABI");

_Static_assert(sizeof(struct giga_graph_config) == 608 &&
    offsetof(struct giga_graph_config, routed_limits) == 24 && offsetof(struct giga_graph_config, shared_limits) == 152 &&
    offsetof(struct giga_graph_config, factors) == 288 && offsetof(struct giga_graph_config, rope) == 552 &&
    offsetof(struct giga_graph_config, index_rope) == 576 && offsetof(struct giga_graph_config, original_context) == 600 &&
    offsetof(struct giga_graph_config, normalize) == 604 && offsetof(struct giga_bindings, weights) == 16 &&
    offsetof(struct giga_bindings, offsets) == 8304, "GigaChat4 graph config ABI");

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

_Static_assert(offsetof(struct ggml_tensor, ne) == 16 &&
    offsetof(struct ggml_tensor, data) == 248 &&
    GGML_TYPE_F32 == 0 && GGML_TYPE_F16 == 1 && GGML_TYPE_Q5_0 == 6 &&
    GGML_TYPE_Q4_K == 12 && GGML_TYPE_Q6_K == 14, "GigaChat4 tensor ABI");

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
