#include "graph-execute.h"
#include "moe-epilogue.h"
#include "situ.h"
#include "bridge.h"
#include "native-kv.h"

#include "cpu.h"
#include "ggml-impl.h"
#include "goose-host.h"

#include <pthread.h>
#include <fcntl.h>
#include <inttypes.h>
#ifdef _WIN32
#include "windows/platform.h"
#else
#include <sched.h>
#endif
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void fg143_cpu_init(int argc, char ** argv);
extern int64_t fg_cuda_activation_read_address(uint64_t host, int64_t bytes);
extern int64_t fg_cuda_state_read(uint64_t host, int64_t bytes);
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

struct numerical_checkpoint {
    FILE * data;
    FILE * manifest;
    sl_i64 frames;
    int64_t layers;
    uint64_t offset;
};

static _Thread_local struct numerical_checkpoint * checkpoint;

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
bool fg_graph_native_checkpoint(void) { return checkpoint != NULL; }

void fg143_gather_witness(int64_t rows, int64_t tasks) {
    if (getenv("GOOSE_GRAPH_LOG")) {
        fprintf(stderr, "Goose CPU GET_ROWS: %" PRId64 " rows over %" PRId64 " threads\n", rows, tasks);
    }
}

void fg143_witness(int64_t op, int64_t values) {
    (void) op;
    atomic_fetch_add_explicit(&cpu_values, (uint64_t) values, memory_order_relaxed);
}

void fg141_witness(int64_t stage, int64_t kind, int64_t values) {
    (void) stage;
    fg143_witness(kind, values);
}

uint64_t fg143_storage_alloc(uint64_t bytes) {
    if (bytes > SIZE_MAX) return 0;
#ifdef _WIN32
    _Static_assert(GGML_MEM_ALIGN <= 16, "Win64 CRT heap alignment");
    return (uint64_t)(uintptr_t)malloc((size_t)bytes);
#else
    return (uint64_t) (uintptr_t) aligned_alloc(GGML_MEM_ALIGN, (size_t) bytes);
#endif
}

uint64_t fg143_storage_ram_available(void) {
#ifdef _WIN32
    return fg_win_memory(true);
#else
    FILE * file = fopen("/proc/meminfo", "r");
    if (!file) return 0;
    uint64_t kib = 0;
    char line[128];
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "MemAvailable: %" SCNu64 " kB", &kib) == 1) break;
    }
    fclose(file);
    return kib <= UINT64_MAX / 1024 ? kib * 1024 : 0;
#endif
}

void fg143_storage_ram_error(uint64_t required, uint64_t available) {
    fprintf(stderr, "Goose graph allocation refused: need %" PRIu64 " bytes, RAM available %" PRIu64 " bytes\n",
        required, available);
}

void fg143_storage_report(uint64_t bytes, int64_t blocks, int64_t reused) {
    if (getenv("GOOSE_ARENA_LOG")) {
        fprintf(stderr, "Goose tensor arena: %" PRIu64 " bytes, %" PRId64 " blocks, %" PRId64 " reuses\n",
            bytes, blocks, reused);
    }
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

void fg143_graph_plan_log_replays(uint64_t replays) {
    fprintf(stderr, "Goose decode plan reuses: %zu\n", (size_t)replays);
}

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

static bool checkpoint_write(const struct ggml_tensor * t, const char * role, int64_t layer,
                             int64_t row, int64_t rows, bool persistent, bool whole) {
    if (!t || !t->data || row < 0 || rows <= 0 || row > t->ne[1] || rows > t->ne[1] - row) return false;
    const size_t total = ggml_nbytes(t);
    const size_t width = ggml_row_size(t->type, t->ne[0]);
    const size_t stride = t->nb[1];
    if (!width || stride < width || width > total || (uint64_t) row > (total - width) / stride) return false;
    const size_t start = (size_t) row * stride;
    if ((uint64_t) (rows - 1) > (total - start - width) / stride) return false;
    if (whole && (!ggml_is_contiguous(t) || row != 0 || rows != t->ne[1])) return false;
    if (!whole && (t->ne[2] != 1 || t->ne[3] != 1)) return false;
    const size_t span = whole ? total : (size_t) (rows - 1) * stride + width;
    const size_t bytes = whole ? total : (size_t) rows * width;
    if (span > INT64_MAX || bytes > UINT64_MAX - checkpoint->offset) return false;
    const uint8_t * data = (const uint8_t *) t->data + start;
    const uint64_t source = (uint64_t) (uintptr_t) data;
    const int64_t copied = persistent ? fg_cuda_state_read(source, (int64_t) span)
                                     : fg_cuda_activation_read_address(source, (int64_t) span);
    if (copied != 0) return false;
    if (whole || stride == width) {
        if (fwrite(data, 1, bytes, checkpoint->data) != bytes) return false;
    } else {
        for (int64_t i = 0; i < rows; ++i) {
            if (fwrite(data + (size_t) i * stride, 1, width, checkpoint->data) != width) return false;
        }
    }
    const int recorded = fprintf(checkpoint->manifest,
        "{\"role\":\"%s\",\"layer\":%" PRId64 ",\"dtype\":\"%s\","
        "\"shape\":[%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64 "],"
        "\"row\":%" PRId64 ",\"rows\":%" PRId64 ",\"offset\":%" PRIu64 ",\"bytes\":%zu}\n",
        role, layer, ggml_type_name(t->type), t->ne[0], t->ne[1], t->ne[2], t->ne[3], row, rows, checkpoint->offset, bytes);
    checkpoint->offset += bytes;
    return recorded >= 0;
}

uint8_t fg143_graph_observe(uint64_t address) {
    if (!checkpoint) return 0;
    const struct ggml_tensor * t = (const void *) (uintptr_t) address;
    static const char * roles[] = {"post_attention_norm", "experts", "ffn", "layer_output"};
    static const char * tail[] = {"head_rms", "head_norm", "logits"};
    const int64_t end = 1 + 4 * checkpoint->layers;
    for (int64_t i = 0; i < checkpoint->frames.len; ++i) {
        if ((uint64_t) checkpoint->frames.data[i] != address) continue;
        const char * role = i == 0 ? "embedding" : i < end ? roles[(i - 1) % 4] : tail[i - end];
        const int64_t layer = i > 0 && i < end ? (i - 1) / 4 : -1;
        if (i == end + 2 || (layer >= 0 && (i - 1) % 4 == 1)) {
            if (!checkpoint_write(t, role, layer, 0, t->ne[1], false, false)) return 0;
        } else {
            if (!checkpoint_write(t, role, layer, 0, 1, false, false)) return 0;
            if (t->ne[1] > 1 && !checkpoint_write(t, role, layer, t->ne[1] - 1, 1, false, false)) return 0;
        }
    }
    return 1;
}

void fg143_graph_log_failure(int64_t index, int32_t op) {
    fprintf(stderr, "Goose graph: node %lld (%s) failed\n",
            (long long) index, ggml_op_name(op));
}

uint8_t fg143_graph_log_enabled(void) { return getenv("GOOSE_GRAPH_LOG") != NULL; }

uint64_t fg143_graph_take_cpu_values(void) {
    return atomic_exchange_explicit(&cpu_values, 0, memory_order_relaxed);
}

void fg143_graph_log_count(sl_u8 label, uint64_t count, sl_u8 unit) {
    fprintf(stderr, "%.*s: %" PRIu64 "%.*s\n", (int)label.len, label.data, count, (int)unit.len, unit.data);
}

void fg143_graph_log_pair(sl_u8 label, uint64_t count, sl_u8 unit, uint64_t next, sl_u8 next_unit) {
    fprintf(stderr, "%.*s: %" PRIu64 "%.*s, %" PRIu64 "%.*s\n",
        (int)label.len, label.data, count, (int)unit.len, unit.data,
        next, (int)next_unit.len, next_unit.data);
}

#ifndef _WIN32
bool fg_q35_checkpoint_enabled(void) {
    const char * path = getenv("GOOSE_APEX_TRACE_DIR");
    return path && *path;
}

static FILE * checkpoint_file(int directory, int64_t prefix, const char * suffix) {
    char name[56];
    snprintf(name, sizeof name, "step-%012" PRId64 ".%s", prefix, suffix);
    const int fd = openat(directory, name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0) return NULL;
    FILE * file = fdopen(fd, "wb");
    if (!file) close(fd);
    return file;
}

enum ggml_status fg_q35_checkpoint_compute(struct fg_standalone_graph_plan * plan, sl_i64 frames,
                                          sl_i64 layers, int64_t prefix, int64_t tokens) {
    const char * path = getenv("GOOSE_APEX_TRACE_DIR");
    const int64_t count = layers.len / 32;
    if (!path || !*path || !plan || checkpoint || prefix < 0 || tokens <= 0 || prefix > INT64_MAX - tokens ||
        count < 1 || count > 128 || layers.len % 32 || frames.len != 4 * count + 4) return GGML_STATUS_FAILED;
    const int directory = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0) return GGML_STATUS_FAILED;
    struct numerical_checkpoint trace = {0};
    trace.data = checkpoint_file(directory, prefix, "bin");
    if (trace.data) trace.manifest = checkpoint_file(directory, prefix, "jsonl");
    close(directory);
    if (!trace.data || !trace.manifest) {
        if (trace.data) fclose(trace.data);
        fprintf(stderr, "Goose: cannot create numerical checkpoint for prefix %" PRId64 "\n", prefix);
        return GGML_STATUS_FAILED;
    }
    trace.frames = frames;
    trace.layers = count;
    checkpoint = &trace;
    bool valid = fg_standalone_graph_plan_compute(plan) == GGML_STATUS_SUCCESS;
    for (int64_t i = 0; valid && i < count; ++i) {
        const int64_t * layer = layers.data + 32 * i;
        const bool recurrent = layer[0] == 1;
        const struct ggml_tensor * left = (const void *) (uintptr_t) layer[recurrent ? 20 : 22];
        const struct ggml_tensor * right = (const void *) (uintptr_t) layer[recurrent ? 21 : 23];
        if (!left || !right) { valid = false; break; }
        valid = checkpoint_write(left, recurrent ? "ssm_history" : "kv_keys", i, 0,
            recurrent ? left->ne[1] : prefix + tokens, true, recurrent) &&
            checkpoint_write(right, recurrent ? "gdn_state" : "kv_values", i, 0,
                recurrent ? right->ne[1] : prefix + tokens, true, recurrent);
    }
    checkpoint = NULL;
    if (fclose(trace.data) != 0) valid = false;
    if (valid && fprintf(trace.manifest, "{\"complete\":true,\"prefix\":%" PRId64 ",\"tokens\":%" PRId64 "}\n",
            prefix, tokens) < 0) valid = false;
    if (fclose(trace.manifest) != 0) valid = false;
    if (!valid) fprintf(stderr, "Goose: numerical checkpoint failed for prefix %" PRId64 "\n", prefix);
    return valid ? GGML_STATUS_SUCCESS : GGML_STATUS_FAILED;
}

#else
bool fg_q35_checkpoint_enabled(void) { return false; }

enum ggml_status fg_q35_checkpoint_compute(struct fg_standalone_graph_plan *plan,
        sl_i64 frames, sl_i64 layers, int64_t prefix, int64_t tokens) {
    (void)frames; (void)layers; (void)prefix; (void)tokens;
    return fg_standalone_graph_plan_compute(plan);
}
#endif

_Static_assert(GGML_MAX_SRC == 10 && offsetof(struct ggml_tensor, src) == 152 &&
        GGML_TENSOR_FLAG_PARAM == 4 && GGML_TENSOR_FLAG_COMPUTE == 16 &&
        offsetof(struct ggml_cgraph, order) == 80 &&
        GGML_CGRAPH_EVAL_ORDER_RIGHT_TO_LEFT == 1, "Goose graph visitation ABI");

void fg143_graph_name_unnamed(uint64_t address, bool leaf, int index) {
    struct ggml_tensor *node = (void *)(uintptr_t)address;
    if (!node->name[0]) ggml_format_name(node, leaf ? "leaf_%d" : "node_%d", index);
}
