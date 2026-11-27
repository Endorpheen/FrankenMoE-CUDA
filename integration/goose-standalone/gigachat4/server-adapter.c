#include "server-adapter.h"
#include "graph.h"
#include "graph-execute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void giga_server_log_placement(int32_t kind, int64_t bytes, int64_t available) {
    if (kind == 1) {
        fprintf(stderr, "GigaChat4: expert VRAM=%lld bytes, available=%lld bytes\n",
                (long long)bytes, (long long)available);
    } else {
        fprintf(stderr, "GigaChat4: %s VRAM=%lld bytes\n",
                kind == 0 ? "dense" : "state logical", (long long)bytes);
    }
}

void giga_server_log_open(int32_t status, uint8_t finished) {
    if (finished) fprintf(stderr, "GigaChat4: model ready status=%d\n", status);
    else fputs("GigaChat4: binding model\n", stderr);
}

int giga_server_open(const void *mapping, size_t bytes, int64_t capacity) {
#ifdef _WIN32
    if (!getenv("GOOSE_MMQ") && _putenv_s("GOOSE_MMQ", "1") != 0) return -1;
#else
    if (setenv("GOOSE_MMQ", "1", 0) != 0) return -1;
#endif
    giga_goose_init(0, NULL);
    return giga_server_open_bound((uintptr_t)mapping, bytes, capacity);
}

void giga_server_trace_execution(int64_t value, uint8_t draft, uint8_t finished) {
    if (finished) fprintf(stderr, "GigaChat4: step status=%lld\n", (long long)value);
    else fprintf(stderr, "GigaChat4: computing %lld tokens, draft=%d\n", (long long)value, draft);
}
