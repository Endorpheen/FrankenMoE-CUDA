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
extern uint8_t q35_cpumask_parse(sl_u8 text, sl_i64 mask);
extern int64_t th_cpumask_next(sl_i64 mask, int64_t strict, int64_t first, sl_i64 local);
extern uint64_t fg_cuda_take_matrices(void);
extern uint64_t fg_cuda_take_f32_matrices(void);
extern void fg_cuda_take_matrix_launches(uint64_t * launches, uint64_t * cached);
extern void fg_cuda_take_routed_graph(uint64_t * groups, uint64_t * replays, uint64_t * full, uint64_t * full_replays);
extern uint64_t fg_cuda_take_mmq(void);
extern void fg_cuda_take_mmq_stream(uint64_t * windows, uint64_t * fixups);
extern uint64_t fg_cuda_take_dense_mma(void);
extern uint64_t fg_cuda_take_attentions(void);
extern uint64_t fg_cuda_take_flash_mmas(void);
extern uint64_t fg_cuda_take_causal_attentions(void);
extern uint64_t fg_cuda_take_gdns(void);
extern uint64_t fg_cuda_take_gdn_mmas(void);
extern void fg_cuda_take_resident_state(uint64_t * kv, uint64_t * gdn);
extern uint64_t fg_cuda_take_q4_writes(void);
extern uint64_t fg_cuda_take_vectors(void);
extern uint64_t fg_cuda_take_fwhts(void);
extern uint64_t fg_cuda_take_moe_weighted(void);
extern void fg_cuda_take_position(uint64_t * ropes, uint64_t * convs);
extern void fg_cuda_take_routes(uint64_t * routes);
extern void fg_cuda_take_blits(uint64_t * blits);
extern int64_t fg_cuda_activation_read_address(uint64_t host, int64_t bytes);
extern int64_t fg_cuda_state_read(uint64_t host, int64_t bytes);
extern int fg_cuda_activation_graph_end(bool success, bool all, uint64_t host, int64_t bytes, uint64_t * hits,
                                        uint64_t * skipped_download_bytes, uint64_t * expired_bytes);

static pthread_once_t cpu_init = PTHREAD_ONCE_INIT;
static atomic_uint_fast64_t cpu_values;
static atomic_int_fast64_t cpu_batch_threads = 1;
static atomic_int_fast64_t cpu_decode_threads = 1;
static _Thread_local bool cpu_prefill = true;
enum { CPU_PROFILE_MASK_BITS = 512 };
struct cpu_affinity_profile {
    cpu_set_t placement[CPU_PROFILE_MASK_BITS];
    int64_t count;
};
static struct cpu_affinity_profile cpu_affinity[2];

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

static int64_t configure_cpu_threads(atomic_int_fast64_t * setting, int64_t threads) {
    if (threads <= 0 || threads > CPU_PROFILE_MASK_BITS) return -1;
    pthread_once(&cpu_init, init_cpu);
    if (!fg143_cpu_batch_supported(threads)) return -1;
    atomic_store_explicit(setting, threads, memory_order_relaxed);
    return 0;
}

int64_t fg_standalone_configure_cpu_batch(int64_t threads) {
    return configure_cpu_threads(&cpu_batch_threads, threads);
}

int64_t fg_standalone_configure_cpu_decode(int64_t threads) {
    return configure_cpu_threads(&cpu_decode_threads, threads);
}

int64_t fg_standalone_cpu_batch_threads(void) {
    return atomic_load_explicit(&cpu_batch_threads, memory_order_relaxed);
}

int64_t fg_standalone_cpu_decode_threads(void) {
    return atomic_load_explicit(&cpu_decode_threads, memory_order_relaxed);
}

bool fg_standalone_set_cpu_prefill(bool prefill) {
    const bool previous = cpu_prefill;
    cpu_prefill = prefill;
    return previous;
}

int64_t fg_standalone_configure_cpu_affinity(const char * text, bool strict, bool prefill) {
    int64_t mask[CPU_PROFILE_MASK_BITS], local[CPU_PROFILE_MASK_BITS];
    cpu_set_t allowed;
    if (!text || sched_getaffinity(0, sizeof allowed, &allowed) != 0) return -1;
    pthread_once(&cpu_init, init_cpu);
    if (!q35_cpumask_parse((sl_u8) {(void *) text, (int64_t) strlen(text)},
                         (sl_i64) {mask, CPU_PROFILE_MASK_BITS})) return -1;
    for (int cpu = 0; cpu < CPU_PROFILE_MASK_BITS; ++cpu) {
        if (mask[cpu] && !CPU_ISSET(cpu, &allowed)) return -1;
    }
    struct cpu_affinity_profile * profile = &cpu_affinity[prefill ? 0 : 1];
    const int64_t threads = prefill ? fg_standalone_cpu_batch_threads() : fg_standalone_cpu_decode_threads();
    int64_t next = 0;
    for (int64_t worker = 0; worker < threads; ++worker) {
        next = th_cpumask_next((sl_i64) {mask, CPU_PROFILE_MASK_BITS}, strict, next,
                              (sl_i64) {local, CPU_PROFILE_MASK_BITS});
        CPU_ZERO(&profile->placement[worker]);
        for (int cpu = 0; cpu < CPU_PROFILE_MASK_BITS; ++cpu) {
            if (local[cpu]) CPU_SET(cpu, &profile->placement[worker]);
        }
    }
    profile->count = threads;
    return 0;
}

uint8_t fg143_cpu_prefill(void) {
    return cpu_prefill;
}

int64_t fg143_cpu_gather_threads(void) {
    return cpu_prefill ? fg_standalone_cpu_batch_threads() : fg_standalone_cpu_decode_threads();
}

int64_t fg143_gather_affinity(uint8_t prefill, int64_t worker) {
    const struct cpu_affinity_profile * profile = &cpu_affinity[prefill ? 0 : 1];
    if (profile->count == 0) return 0;
    if (worker < 0 || worker >= profile->count ||
        sched_setaffinity(0, sizeof(cpu_set_t), &profile->placement[worker]) != 0) {
        fprintf(stderr, "Goose CPU affinity failed for %s worker %" PRId64 "\n",
                prefill ? "prefill" : "decode", worker);
        return -1;
    }
    if (getenv("GOOSE_GRAPH_LOG")) {
        cpu_set_t effective;
        if (sched_getaffinity(0, sizeof effective, &effective) != 0) return -1;
        fprintf(stderr, "Goose CPU affinity: %s worker %" PRId64 " cpu %d allowed %d\n",
                prefill ? "prefill" : "decode", worker, sched_getcpu(), CPU_COUNT(&effective));
    }
    return 0;
}

int64_t fg143_gather_index(uint64_t address) {
    int32_t value;
    memcpy(&value, (const void *) (uintptr_t) address, sizeof value);
    return value;
}

void fg143_gather_fetch(uint64_t address, sl_u8 row) {
    memcpy(row.data, (const void *) (uintptr_t) address, (size_t) row.len);
}

void fg143_gather_store(uint64_t address, sl_f32 row) {
    memcpy((void *) (uintptr_t) address, row.data, (size_t) row.len * sizeof(*row.data));
}

void fg143_gather_witness(int64_t rows, int64_t tasks) {
    if (getenv("GOOSE_GRAPH_LOG")) {
        fprintf(stderr, "Goose CPU GET_ROWS: %" PRId64 " rows over %" PRId64 " threads\n", rows, tasks);
    }
}

uint32_t fg143_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

float fg143_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

float fg143_codec_from_bits(uint32_t bits) {
    return fg143_from_bits(bits);
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

void * fg_standalone_graph_alloc(struct ggml_cgraph * graph) {
    pthread_once(&cpu_init, init_cpu);
    return (void *) (uintptr_t) fg143_storage_allocate((uint64_t) (uintptr_t) graph);
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

struct fg_standalone_graph_plan * fg_standalone_graph_plan_alloc(struct ggml_cgraph * graph, const struct ggml_tensor * output) {
    if (!graph || graph->n_nodes < 0) return NULL;
    const size_t nodes = (size_t) graph->n_nodes;
    if (nodes > (SIZE_MAX - sizeof(struct fg_standalone_graph_plan)) / sizeof(int64_t)) return NULL;
    struct fg_standalone_graph_plan * plan = malloc(sizeof(*plan) + nodes * sizeof(int64_t));
    if (!plan) return NULL;
    plan->graph = graph;
    plan->output = output;
    plan->nodes = (int64_t) nodes;
    plan->decode = NULL;
    for (size_t i = 0; i < nodes; ++i) plan->deadlines[i] = -1;
    pthread_once(&cpu_init, init_cpu);
    plan->arena = (void *) (uintptr_t) fg143_storage_allocate_output((uint64_t) (uintptr_t) graph,
        (uint64_t) (uintptr_t) output, (sl_i64) {plan->deadlines, plan->nodes});
    if (!plan->arena) {
        free(plan);
        return NULL;
    }
    return plan;
}

void fg_standalone_graph_plan_free(struct fg_standalone_graph_plan * plan) {
    if (!plan) return;
    if (plan->decode && getenv("GOOSE_GRAPH_LOG")) {
        fprintf(stderr, "Goose decode plan reuses: %zu\n", plan->decode->replays);
    }
    free(plan->decode);
    free(plan->arena);
    free(plan);
}

_Static_assert(offsetof(struct fg_standalone_graph_plan, graph) == 0 &&
    offsetof(struct fg_standalone_graph_plan, decode) == 32 &&
    offsetof(struct q35_decode_bindings, count) == 0 &&
    offsetof(struct q35_decode_bindings, replays) == 8 &&
    offsetof(struct q35_decode_bindings, layers) == 16 &&
    sizeof(struct q35_decode_attention) == 40 &&
    sizeof(struct q35_decode_bindings) == 5136 &&
    GGML_TYPE_Q4_0 == 2 && GGML_OP_SET_ROWS == 42, "Goose decode plan ABI");

uint64_t fg143_graph_resize(uint64_t address, int64_t bytes) {
    void * work = (void *) (uintptr_t) address;
    if (bytes == 0) {
        free(work);
        return 0;
    }
    if (bytes < 0 || (uint64_t) bytes > SIZE_MAX) return 0;
    return (uint64_t) (uintptr_t) realloc(work, (size_t) bytes);
}

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

uint8_t fg143_graph_restore_affinity(uint64_t address) {
    if (!address) return true;
    const cpu_set_t *previous = (const void *)(uintptr_t)address;
    if (sched_setaffinity(0, sizeof *previous, previous) == 0) return true;
    perror("Goose CPU affinity restore");
    return false;
}

void fg143_graph_collect(uint64_t graph_address, int64_t failed, sl_u64 counters) {
    const struct ggml_cgraph *graph = (const void *)(uintptr_t)graph_address;
    if (failed > 0) {
        const struct ggml_tensor *node = graph->nodes[failed - 1];
        fprintf(stderr, "Goose graph: node %lld (%s) failed\n",
                (long long)failed - 1, ggml_op_name(node->op));
    }
    uint64_t *c = counters.data;
    c[0] = fg_cuda_take_matrices();
    c[1] = fg_cuda_take_f32_matrices();
    fg_cuda_take_matrix_launches(c + 2, c + 3);
    fg_cuda_take_routed_graph(c + 4, c + 5, c + 6, c + 7);
    c[8] = fg_cuda_take_mmq();
    fg_cuda_take_mmq_stream(c + 9, c + 10);
    c[11] = fg_cuda_take_dense_mma();
    c[12] = fg_cuda_take_attentions();
    c[13] = fg_cuda_take_flash_mmas();
    c[14] = fg_cuda_take_causal_attentions();
    c[15] = fg_cuda_take_gdns();
    c[16] = fg_cuda_take_gdn_mmas();
    fg_cuda_take_resident_state(c + 17, c + 18);
    c[19] = fg_cuda_take_q4_writes();
    c[20] = fg_cuda_take_vectors();
    c[21] = fg_cuda_take_fwhts();
    c[22] = fg_cuda_take_moe_weighted();
    fg_cuda_take_position(c + 23, c + 24);
    fg_cuda_take_routes(c + 25);
    fg_cuda_take_blits(c + 30);
}

int32_t fg143_graph_publish(uint64_t output_address, uint8_t valid, uint8_t all, sl_u64 counters) {
    const struct ggml_tensor *output = (const void *)(uintptr_t)output_address;
    return fg_cuda_activation_graph_end(valid, all, output ? (uintptr_t)output->data : 0,
        output ? (int64_t)ggml_nbytes(output) : 0, counters.data + 33,
        counters.data + 34, counters.data + 35);
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

static enum ggml_status compute_graph(struct ggml_cgraph *graph, bool all,
                                      const struct ggml_tensor *output, sl_i64 deadlines) {
    pthread_once(&cpu_init, init_cpu);
    const bool pin = cpu_affinity[cpu_prefill ? 0 : 1].count != 0;
    cpu_set_t previous;
    if (pin && sched_getaffinity(0, sizeof previous, &previous) != 0) {
        perror("Goose CPU affinity snapshot");
        return GGML_STATUS_FAILED;
    }
    return fg143_graph_execute((uintptr_t)graph, (uintptr_t)output, all, deadlines,
                               checkpoint != NULL, cpu_prefill, pin ? (uintptr_t)&previous : 0);
}

enum ggml_status fg_standalone_graph_compute(struct ggml_cgraph * graph) {
    return compute_graph(graph, true, NULL, (sl_i64) {0});
}

enum ggml_status fg_standalone_graph_compute_output(struct ggml_cgraph * graph, const struct ggml_tensor * output) {
    return compute_graph(graph, false, output, (sl_i64) {0});
}

enum ggml_status fg_standalone_graph_plan_compute(struct fg_standalone_graph_plan * plan) {
    if (!plan || plan->nodes != plan->graph->n_nodes) return GGML_STATUS_FAILED;
    return compute_graph(plan->graph, false, plan->output, (sl_i64) {plan->deadlines, plan->nodes});
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
