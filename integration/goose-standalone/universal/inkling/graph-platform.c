#include "graph-execute.h"
#include "moe-epilogue.h"
#include "situ.h"
#include "bridge.h"
#include "native-kv.h"

#include "cpu.h"
#include "ggml-impl.h"

#include <pthread.h>
#ifdef _WIN32
#include "windows/platform.h"
#else
#include <sched.h>
#endif
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
_Static_assert(sizeof(cpu_set_t) == 8, "Goose Windows affinity ABI");
#else
_Static_assert(sizeof(cpu_set_t) == 128, "Goose Linux affinity ABI");
#endif
_Static_assert(offsetof(struct ggml_cgraph, n_nodes) == 4 && GGML_STATUS_FAILED == -1, "Goose graph execution ABI");

struct q35_decode_attention {
    struct ggml_tensor *attention;
    struct ggml_tensor *key, *value;
    struct ggml_tensor *raw_key, *raw_value;
};

struct q35_decode_bindings {
    size_t count, replays;
    struct q35_decode_attention layers[128];
};

struct fg_standalone_graph_plan {
    struct ggml_cgraph * graph;
    const struct ggml_tensor * output;
    void * arena;
    int64_t nodes;
    struct q35_decode_bindings *decode;
    int64_t deadlines[];
};

_Static_assert(offsetof(struct fg_standalone_graph_plan, graph) == 0 &&
    offsetof(struct fg_standalone_graph_plan, output) == 8 &&
    offsetof(struct fg_standalone_graph_plan, arena) == 16 &&
    offsetof(struct fg_standalone_graph_plan, nodes) == 24 &&
    offsetof(struct fg_standalone_graph_plan, deadlines) == 40 &&
    sizeof(struct fg_standalone_graph_plan) == 40 &&
    offsetof(struct fg_standalone_graph_plan, decode) == 32 &&
    offsetof(struct q35_decode_bindings, count) == 0 &&
    offsetof(struct q35_decode_bindings, replays) == 8 &&
    offsetof(struct q35_decode_bindings, layers) == 16 &&
    sizeof(struct q35_decode_attention) == 40 &&
    sizeof(struct q35_decode_bindings) == 5136 &&
    GGML_TYPE_Q4_0 == 2 && GGML_OP_SET_ROWS == 42, "Goose decode plan ABI");

_Static_assert(sizeof(struct inkling_attention_graph_call) == 232 &&
        offsetof(struct inkling_attention_graph_call, batch.count) == 12 &&
        offsetof(struct inkling_attention_graph_call, batch.output) == 72 &&
        offsetof(struct inkling_attention_graph_call, inputs.input) == 80 &&
        offsetof(struct inkling_attention_graph_call, output.rows) == 224 &&
        INKLING_BRIDGE_GRAPH == 0x49324252 && INKLING_ATTENTION_GRAPH == 0x49324147 &&
        INKLING_MOE_EPILOGUE == 0x49324d45 && INKLING_MOE_SITU == 0x49325354 &&
        GGML_UNARY_OP_TANH == 4, "Goose Inkling graph ABI");

_Static_assert(GGML_MAX_SRC == 10 && offsetof(struct ggml_tensor, src) == 152 &&
        GGML_TENSOR_FLAG_PARAM == 4 && GGML_TENSOR_FLAG_COMPUTE == 16 &&
        offsetof(struct ggml_cgraph, order) == 80 &&
        offsetof(struct ggml_tensor, ne) == 16 && offsetof(struct ggml_tensor, nb) == 48 &&
        offsetof(struct ggml_tensor, data) == 248 && offsetof(struct ggml_tensor, name) == 256 &&
        GGML_MAX_NAME >= 17 && GGML_MEM_ALIGN == 16 &&
        GGML_CGRAPH_EVAL_ORDER_RIGHT_TO_LEFT == 1, "Goose graph visitation ABI");
