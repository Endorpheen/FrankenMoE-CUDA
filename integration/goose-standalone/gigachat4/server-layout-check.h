#include "server-adapter.h"
#include "graph-execute.h"

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

_Static_assert(offsetof(struct ggml_tensor, ne) == 16 &&
    offsetof(struct ggml_tensor, data) == 248 &&
    GGML_TYPE_F32 == 0 && GGML_TYPE_F16 == 1 && GGML_TYPE_Q5_0 == 6 &&
    GGML_TYPE_Q4_K == 12 && GGML_TYPE_Q6_K == 14, "GigaChat4 tensor ABI");

_Static_assert(sizeof(struct giga_bindings) == 9448 && offsetof(struct giga_bindings, config) == 8840 &&
    offsetof(struct ggml_tensor, op) == 80 && offsetof(struct ggml_tensor, op_params) == 84 &&
    offsetof(struct ggml_tensor, src) == 152 && GGML_OP_CUSTOM == 95 &&
    GIGA_TENSOR_COUNT == 1036 && GIGA_LAYER_COUNT == 33 && GGML_TYPE_COUNT == 43, "GigaChat4 bindings ABI");
