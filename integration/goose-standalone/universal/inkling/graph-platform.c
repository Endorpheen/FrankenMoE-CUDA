#include "graph-execute.h"
#include "moe-epilogue.h"
#include "situ.h"
#include "bridge.h"
#include "native-kv.h"

#include "cpu.h"
#include "ggml-impl.h"
#include "goose-host.h"

#include <pthread.h>
#ifdef _WIN32
#include "windows/platform.h"
#else
#include <sched.h>
#endif
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

extern void fg143_cpu_init(int argc, char ** argv);
static pthread_once_t cpu_init = PTHREAD_ONCE_INIT;
static atomic_uint_fast64_t cpu_values;
static atomic_int_fast64_t cpu_batch_threads = 1;
static atomic_int_fast64_t cpu_decode_threads = 1;
static _Thread_local bool cpu_prefill = true;
#ifdef _WIN32
_Static_assert(sizeof(cpu_set_t) == 8, "Goose Windows affinity ABI");
#else
_Static_assert(sizeof(cpu_set_t) == 128, "Goose Linux affinity ABI");
#endif
_Static_assert(offsetof(struct ggml_cgraph, n_nodes) == 4 && GGML_STATUS_FAILED == -1, "Goose graph execution ABI");

static _Thread_local uint64_t checkpoint_address;

static void init_cpu(void) {
    fg_goose_host_ensure_init();
    fg143_cpu_init(0, NULL);
}

void fg_graph_native_cpu_init(void) { pthread_once(&cpu_init, init_cpu); }

int64_t fg_graph_native_threads(bool prefill, int64_t value) {
    atomic_int_fast64_t *setting = prefill ? &cpu_batch_threads : &cpu_decode_threads;
    if (value) atomic_store_explicit(setting, value, memory_order_relaxed);
    return atomic_load_explicit(setting, memory_order_relaxed);
}

bool fg_graph_native_set_prefill(bool prefill) {
    const bool previous = cpu_prefill;
    cpu_prefill = prefill;
    return previous;
}

bool fg_graph_native_prefill(void) { return cpu_prefill; }
bool fg_graph_native_checkpoint(void) { return checkpoint_address != 0; }
uint64_t fg_graph_native_trace_get(void) { return checkpoint_address; }
void fg_graph_native_trace_set(uint64_t address) { checkpoint_address = address; }

void fg143_witness(int64_t op, int64_t values) {
    (void) op;
    atomic_fetch_add_explicit(&cpu_values, (uint64_t) values, memory_order_relaxed);
}

void fg141_witness(int64_t stage, int64_t kind, int64_t values) {
    (void) stage;
    fg143_witness(kind, values);
}

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

uint64_t fg143_graph_take_cpu_values(void) {
    return atomic_exchange_explicit(&cpu_values, 0, memory_order_relaxed);
}

_Static_assert(GGML_MAX_SRC == 10 && offsetof(struct ggml_tensor, src) == 152 &&
        GGML_TENSOR_FLAG_PARAM == 4 && GGML_TENSOR_FLAG_COMPUTE == 16 &&
        offsetof(struct ggml_cgraph, order) == 80 &&
        offsetof(struct ggml_tensor, ne) == 16 && offsetof(struct ggml_tensor, nb) == 48 &&
        offsetof(struct ggml_tensor, data) == 248 && offsetof(struct ggml_tensor, name) == 256 &&
        GGML_MAX_NAME >= 17 && GGML_MEM_ALIGN == 16 &&
        GGML_CGRAPH_EVAL_ORDER_RIGHT_TO_LEFT == 1, "Goose graph visitation ABI");
