#include "cpu.h"
#include "goose-cuda-image.h"
#include "attention-cubin.h"
#include "attention-first-row-image.h"
#include "attention-unroll-image.h"
#include "fused-image.h"
#include "shared-image.h"
#include "compact-route-image.h"
#include "softmax-image.h"

#include <dlfcn.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int64_t dw_cuda_dense_offset(sl_i64 ranges, uint64_t address, int64_t bytes);
extern int64_t qr_state_offset(sl_i64 ranges, uint64_t address, int64_t bytes, int64_t kind);
extern int64_t qr_state_index(sl_i64 ranges, uint64_t address, int64_t bytes, int64_t kind);
extern int64_t fg143_activation_lookup(sl_i64 ranges, uint64_t host, int64_t bytes);
extern int64_t fg143_activation_overlap(sl_i64 ranges, uint64_t host, int64_t bytes, int64_t first);
extern uint64_t fg143_activation_discard_overwritten(sl_i64 ranges, uint64_t host, int64_t bytes);
extern uint64_t fg143_activation_expire(sl_i64 ranges, int64_t step);
extern int64_t fg143_activation_slot(sl_i64 ranges, int64_t capacity, int64_t bytes);
extern int64_t fg143_activation_record_slot(sl_i64 ranges);
extern int64_t fg143_activation_device_overlap(sl_i64 ranges, int64_t offset, int64_t bytes, int64_t first);
extern int64_t fg143_activation_eviction_slot(sl_i64 ranges, int64_t capacity, int64_t bytes);
extern int64_t fg143_activation_last_input(sl_i64 ranges, uint64_t host, int64_t bytes,
                                           int64_t step, int64_t window, int64_t output_bytes);
extern bool fg_standalone_option_enabled(const char * name);
extern uint8_t fg143_cpu_prefill(void);
extern void ec_reset(sl_i64 groups, sl_i64 layers, uint64_t words32, sl_i64 words64);
extern int64_t ec_prepare(sl_i64 groups, sl_i64 layers, uint64_t words32, sl_i64 words64,
    uint64_t tensor, int64_t bytes, sl_i64 heads, sl_i64 result, uint8_t stage_all);
extern int64_t ec_prepare_compact(sl_i64 groups, sl_i64 layers, uint64_t words32, sl_i64 words64,
    uint64_t tensor, int64_t bytes, sl_i64 ids, sl_i64 result, uint8_t stage_all);
extern void exp1744_init(int argc, char **argv);
extern int64_t exp1744_prepare(sl_i64 groups, sl_i64 layers, uint64_t words32, sl_i64 words64,
    uint64_t tensor, int64_t bytes, sl_i64 ids, sl_i64 result, uint8_t stage_all, uint8_t compact, int64_t policy);
extern int64_t ec_manage_pins(sl_i64 spans, int64_t acquire);
extern uint64_t fg143_storage_ram_available(void);

typedef struct CUctx_st * Context;
typedef struct CUmod_st * Module;
typedef struct CUfunc_st * Function;
typedef struct CUstream_st * Stream;
typedef struct CUevent_st * Event;
typedef struct CUgraph_st * Graph;
typedef struct CUgraphExec_st * GraphExec;
typedef struct CUgraphNode_st * GraphNode;
typedef unsigned long long DevicePtr;
typedef unsigned long long AllocationHandle;
// CUDA_KERNEL_NODE_PARAMS_v2 layout for the optional Graph Driver API.
struct kernel_node_params {
    Function function;
    unsigned grid_x, grid_y, grid_z;
    unsigned block_x, block_y, block_z, shared_bytes;
    void **params, **extra;
    void * kernel;
    Context context;
};
_Static_assert(sizeof(struct kernel_node_params) == 72, "CUDA graph kernel node ABI");

// Linux CUDA Driver ABI for device-local memory mappings.
struct memory_location { int type, id; };
struct memcpy_batch_attributes {
    int source_access_order;
    struct memory_location source_location, destination_location;
    unsigned flags;
};
struct memory_properties {
    int type, handle_types;
    struct memory_location location;
    void * platform_metadata;
    struct { unsigned char compression, rdma; unsigned short usage; unsigned char reserved[4]; } flags;
};
struct memory_access { struct memory_location location; int flags; };
_Static_assert(sizeof(struct memory_properties) == 32, "CUDA allocation property ABI");
_Static_assert(sizeof(struct memory_access) == 12, "CUDA access descriptor ABI");

struct vmm_mapping {
    struct vmm_mapping * previous;
    AllocationHandle handle;
    size_t offset, bytes;
    bool mapped;
};

enum { MIB_BYTES = 1024 * 1024, DEFAULT_ACTIVATION_BYTES = 64 * MIB_BYTES,
    WIDE_ACTIVATION_BYTES = 384 * MIB_BYTES };
enum { EXPERT_HOST_SLOTS = 3, EXPERT_HOST_SLOT_BYTES = 4 * MIB_BYTES };
// Release events kept for expert waves, so a transfer may run while an earlier
// layer still computes instead of waiting for the immediately preceding one.
enum { EXPERT_SLOT_EVENTS = 4 };
enum { MMVQ_GRAPH_SLOTS = 1024, MMVQ_GRAPH_KEY_WORDS = 16 };

struct mmvq_graph_entry {
    uint64_t key[MMVQ_GRAPH_KEY_WORDS];
    GraphExec executable;
};
enum { DECODE_GRAPH_REGIONS = 256 };
enum { REPLAY_OPS = 128, REPLAY_ARGS = 32, REPLAY_BYTES = 64 * 1024 };
enum replay_kind { replay_kernel, replay_h2d, replay_d2d, replay_wait, replay_wait_planes };

static bool replay_is_wait(enum replay_kind kind) {
    return kind == replay_wait || kind == replay_wait_planes;
}

struct replay_layout {
    Function function;
    size_t count, bytes;
    size_t offset[REPLAY_ARGS], size[REPLAY_ARGS];
};

struct replay_operation {
    enum replay_kind kind;
    union {
        struct {
            const struct replay_layout *layout;
            unsigned shape[7];
            size_t start;
        } kernel;
        struct { DevicePtr dst; uint64_t src; size_t bytes; } copy;
    };
};

struct replay_region {
    Graph graph;
    GraphExec exec;
    GraphNode nodes[REPLAY_OPS];
    size_t count;
    struct replay_operation *ops;
    uint8_t *args;
};

struct graph_update_result {
    int result;
    GraphNode node, from;
};

struct device_buffer {
    DevicePtr address;
    size_t bytes;
    struct vmm_mapping * mappings;
    size_t reservation_bytes;
    bool vmm_failed;
};

struct expert_cache {
    struct device_buffer weights;
    size_t bank_bytes, prefill_offset, prefill_bytes, prefill_workspace;
    Event prefill_release, prefill_ready;
    bool prefill_double, prefill_compact;
    uint64_t prefill_full_buffer, prefill_skipped_capacity;
    unsigned prefill_next_slot;
    uint64_t prefill_next_tensor, prefill_next_fills;
    int64_t prefill_next_bytes, prefill_next_count;
    uint64_t prefill_next_active[64];
    uint64_t prefill_prefetch_used, prefill_prefetch_discarded;
    bool prefill_wait_decode, decode_seen, one_touch;
    uint64_t prefill_projections, prefill_hits, prefill_upload_bytes;
    sl_i64 groups, layers, work;
    int32_t * state;
    uint64_t hits, fills, evictions, uploaded_bytes;
};

struct expert_upload_job {
    DevicePtr destination;
    const void * source;
    size_t bytes;
    bool pageable;
};

struct expert_host_ring {
    uint8_t * storage;
    Event ready[EXPERT_HOST_SLOTS];
    bool busy[EXPERT_HOST_SLOTS];
    size_t index, used;
    bool enabled;
};

struct expert_copy_task {
    const struct expert_upload_job * jobs;
    size_t count;
    uint8_t * destination;
};

static struct {
    pthread_mutex_t lock;
    pthread_cond_t ready, done;
    pthread_t thread;
    struct expert_copy_task task;
    bool running, stop;
} expert_copy_worker = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .ready = PTHREAD_COND_INITIALIZER,
    .done = PTHREAD_COND_INITIALIZER,
};

static void copy_expert_payloads(struct expert_copy_task task) {
    for (size_t i = 0; i < task.count; ++i) {
        memcpy(task.destination, task.jobs[i].source, task.jobs[i].bytes);
        task.destination += task.jobs[i].bytes;
    }
}

static void * copy_expert_worker_main(void * unused) {
    (void) unused;
    pthread_mutex_lock(&expert_copy_worker.lock);
    while (!expert_copy_worker.stop) {
        if (expert_copy_worker.task.count == 0) {
            pthread_cond_wait(&expert_copy_worker.ready, &expert_copy_worker.lock);
            continue;
        }
        const struct expert_copy_task task = expert_copy_worker.task;
        pthread_mutex_unlock(&expert_copy_worker.lock);
        copy_expert_payloads(task);
        pthread_mutex_lock(&expert_copy_worker.lock);
        expert_copy_worker.task.count = 0;
        pthread_cond_signal(&expert_copy_worker.done);
    }
    pthread_mutex_unlock(&expert_copy_worker.lock);
    return NULL;
}

static struct {
    void * library;
    Context context;
    Module module;
    Function kernel, attention_tiles, attention_merge, gdn, gdn_decode, q4_rows, vector, vector_flat, vector_flat4, rms_reference;
    Function mmq_pack, mmq_pack_shared, mmq_pack_glu, mmq_matrix, mmq_q4_matrix, mmq_q6_matrix;
    Function mmq_pack_k_major, mmq_q6_dense_half_k, mmq_q6_dense_pipeline;
    Function mmq_q4_dense_shared, mmq_q4_routed_shared;
    Function mmq_q4_dense_x64, mmq_q4_routed_x64;
    Function mmq_q6_dense_shared, mmq_q6_routed_shared;
    Function mmq_q6_dense_x64, mmq_q6_routed_x64;
    Function mmq_q4_dense_x128, mmq_q6_dense_x128;
    Function mmq_q4_dense_m64, mmq_q4_dense_m128;
    Function mmq_q4_dense_large, mmq_q4_prepare, mmq_q4_dense_prepared, mmq_q4_dense_pipeline;
    unsigned mmq_q4_large_rows, mmq_q4_large_cols;
    Function mmq_q4_routed_x128, mmq_q6_routed_x128, mmq_q6_routed_pipeline;
    Function mmq_q4_routed_m128, mmq_q4_routed_down_m128;
    Function mmvq_pack, mmvq, mmvq_short, mmvq_dense_pack, mmvq_dense_row;
    Function dense_mma, dense_mma_reference, gdn_mma, gdn_mma_v32, flash_mma, flash_stage, flash_vec, flash_vec_merge;
    Function flash_vec_pack, flash_vec_packed, flash_vec_paired, flash_vec_quad;
    Function flash_vec_quad_key_bound;
    Function mmvq_up_gate_glu, mmvq_shared_up_gate_glu;
    Function moe_add_shared_residual;
    Function rms_decode_2048, route_compact8, route_softmax_fixed;
    struct device_buffer route_weights;
    Function mmq_stream, mmq_stream_fixup;
    Function flash_stream, flash_fixup_uniform, flash_fixup_general;
    Function rope_phases, rope_apply, ssm_conv, ssm_conv_decode, ssm_conv_long;
    Function ssm_conv_direct, ssm_conv_save_tail;
    Function rope_decode;
    Function route_reduce, route_sort, route_topk, route_gather, route_norm, blit, blit_transpose, fwht, fwht_q4, moe_weighted;
    struct device_buffer input, output, weight, jobs;
    struct device_buffer mmq_packed, mmq_tiles, mmq_partials;
    struct device_buffer attention_value, attention_scratch, attention_config;
    struct device_buffer gdn_beta;
    struct device_buffer activations;
    size_t activation_capacity;
    bool adapt_activation_capacity;
    bool direct_wide_vectors;
    bool stage_matrix_input;
    bool defer_matrix_waits;
    bool async_packets;
    unsigned char * packet_host;
    struct device_buffer dense;
    sl_i64 dense_ranges;
    struct device_buffer state;
    sl_i64 state_ranges;
    struct device_buffer * state_views;
    size_t state_logical_bytes;
    struct expert_cache experts;
    Stream expert_stream;
    Event expert_slots_ready, expert_uploads_ready, expert_planes_ready;
    Event expert_slots_ring[EXPERT_SLOT_EVENTS];
    unsigned expert_slots_index, expert_slots_records;
    bool expert_upload_queued, expert_decode_overlap;
    bool expert_prefill_staging, expert_current_staging;
    bool expert_split_planes, expert_planes_split;
    bool expert_async_copies;
    struct expert_upload_job * expert_upload_jobs;
    size_t expert_upload_count, expert_upload_capacity, expert_upload_split;
    struct expert_host_ring expert_host;
    sl_i64 expert_pins;
    int64_t expert_pinned_bytes;
    int status, multiprocessors;
    bool mmq_window, mmq_shared, moe_topk, activation_reference, rms_postscale;
    bool mmq_align_window, mmq_dense_partial, mmq_shared_reuse, mmq_dense_reuse;
    bool mmvq_dense_tail;
    size_t vmm_granularity, vmm_capacity;
    bool scratch_vmm, lazy_kv;
    struct memory_properties vmm_properties;
    bool flash_graph_enabled, flash_graph_pack;
    bool flash_stream_all, flash_tail_mma;
    Stream graph_stream;
    Graph flash_graph;
    GraphExec flash_graph_exec;
    GraphNode flash_graph_nodes[3];
    size_t flash_graph_node_count;
    struct mmvq_graph_entry * mmvq_graph_cache;
    bool mmvq_graph_enabled, routed_graph_enabled, full_ffn_enabled;
    bool decode_graph_enabled, decode_reuse, decode_early_ffn, compact_moe_ids;
    GraphExec decode_regions[DECODE_GRAPH_REGIONS];
    int (*graph_update)(GraphExec, Graph, struct graph_update_result *);
    int (*copy_device_async)(DevicePtr, DevicePtr, size_t, Stream);
    struct device_buffer routed_graph_scratch;
    int (*init)(unsigned);
    int (*device_get)(int *, int);
    int (*device_attribute)(int *, int, int);
    int (*create)(Context *, unsigned, int);
    int (*destroy)(Context);
    int (*push)(Context);
    int (*pop)(Context *);
    int (*module_load)(Module *, const void *);
    int (*function_get)(Function *, Module, const char *);
    int (*function_set_attribute)(Function, int, int);
    int (*alloc)(DevicePtr *, size_t);
    int (*memory_info)(size_t *, size_t *);
    int (*release)(DevicePtr);
    int (*vmm_get_granularity)(size_t *, const struct memory_properties *, int);
    int (*vmm_reserve)(DevicePtr *, size_t, size_t, DevicePtr, unsigned long long);
    int (*vmm_free)(DevicePtr, size_t);
    int (*vmm_create)(AllocationHandle *, size_t, const struct memory_properties *, unsigned long long);
    int (*vmm_map)(DevicePtr, size_t, size_t, AllocationHandle, unsigned long long);
    int (*vmm_access)(DevicePtr, size_t, const struct memory_access *, size_t);
    int (*vmm_unmap)(DevicePtr, size_t);
    int (*vmm_release)(AllocationHandle);
    int (*stream_create)(Stream *, unsigned);
    int (*stream_destroy)(Stream);
    int (*stream_wait_event)(Stream, Event, unsigned);
    int (*event_create)(Event *, unsigned);
    int (*event_destroy)(Event);
    int (*event_record)(Event, Stream);
    int (*event_sync)(Event);
    int (*graph_begin)(Stream, int);
    int (*graph_end)(Stream, Graph *);
    int (*graph_roots)(Graph, GraphNode *, size_t *);
    int (*graph_dependents)(GraphNode, GraphNode *, size_t *);
    int (*graph_node_type)(GraphNode, int *);
    int (*graph_instantiate)(GraphExec *, Graph, unsigned long long);
    int (*graph_set_kernel)(GraphExec, GraphNode, const struct kernel_node_params *);
    int (*graph_launch)(GraphExec, Stream);
    int (*graph_destroy)(Graph);
    int (*graph_exec_destroy)(GraphExec);
    int (*function_param)(Function, size_t, size_t *, size_t *);
    int (*function_name)(const char **, Function);
    int (*host_register)(void *, size_t, unsigned);
    int (*host_unregister)(void *);
    int (*upload_batch)(DevicePtr *, DevicePtr *, size_t *, size_t,
        struct memcpy_batch_attributes *, size_t *, size_t, Stream);
    int (*host_alloc)(void **, size_t, unsigned);
    int (*host_free)(void *);
    int (*upload)(DevicePtr, const void *, size_t);
    int (*upload_async)(DevicePtr, const void *, size_t, Stream);
    int (*download)(void *, DevicePtr, size_t);
    int (*copy_device)(DevicePtr, DevicePtr, size_t);
    int (*zero)(DevicePtr, unsigned char, size_t);
    int (*launch)(Function, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned,
                  unsigned, void *, void **, void **);
    int (*sync)(void);
} cuda;

enum { ACTIVATION_WORDS = 5, ACTIVATION_RECORDS = 256 };
enum { GDN_MMA_SHARED_BYTES = 93312 };
enum { GDN_V32_SHARED_BYTES = 38016 };
enum { Q4_PIPE_SHARED_BYTES = 2 * 30720 };
enum { Q6_HALF_K_SHARED_BYTES = 53760 };
enum { Q6_PIPE_SHARED_BYTES = Q6_HALF_K_SHARED_BYTES + 18432 };
enum { Q6_ROUTED_SHARED_BYTES = Q6_PIPE_SHARED_BYTES + 128 * sizeof(uint32_t) };
static _Thread_local struct {
    bool active, unavailable, defer_waits, pending;
    bool retire, reuse, selective_eviction;
    uint64_t target;
    int64_t target_bytes, target_deadline, step, count, used;
    int64_t ranges[ACTIVATION_RECORDS * ACTIVATION_WORDS];
    int64_t large_output[ACTIVATION_WORDS];
    uint64_t hits, skipped_download_bytes, expired_bytes;
    uint64_t shared_pack_reuses, dense_pack_reuses, flat_vectors, flat4_vectors, q4_prepared, q4_pipelined, q6_pipelined;
    uint64_t q6_routed_pipelined;
    uint64_t expert_parallel_batches;
    size_t packet_used;
} activations;

static struct {
    __typeof__(cuda.launch) launch;
    __typeof__(cuda.upload) upload;
    __typeof__(cuda.upload_async) upload_async;
    __typeof__(cuda.download) download;
    __typeof__(cuda.copy_device) copy_device;
    __typeof__(cuda.zero) zero;
    __typeof__(cuda.alloc) alloc;
    __typeof__(cuda.release) release;
    __typeof__(cuda.sync) sync;
    __typeof__(cuda.memory_info) memory_info;
} decode_raw;

static _Thread_local struct {
    bool scope, capturing, buffering;
    size_t region, kernels, replays, updates, maximum, reused, changed_nodes;
} decode_capture;

static struct replay_layout replay_layouts[128];
static size_t replay_layout_count;
static struct replay_region replay_regions[DECODE_GRAPH_REGIONS];
// The activation graph lock serializes buffered decode regions.
static struct {
    size_t count, used;
    struct replay_operation ops[REPLAY_OPS];
    uint8_t args[REPLAY_BYTES];
} replay_buffer;

static const struct replay_layout * replay_layout_for(Function function) {
#ifdef GOOSE_CUDA_HAS_ARG_COUNTS
    for (size_t i = 0; i < replay_layout_count; ++i)
        if (replay_layouts[i].function == function) return &replay_layouts[i];
    if (replay_layout_count == sizeof replay_layouts / sizeof *replay_layouts) return NULL;
    const char *name;
    if (cuda.function_name(&name, function) != 0) return NULL;
    const size_t entries = sizeof goose_cuda_arg_counts / sizeof *goose_cuda_arg_counts;
    size_t index = 0;
    while (index < entries && strcmp(goose_cuda_arg_counts[index].name, name) != 0) ++index;
    const size_t args = function == cuda.route_softmax_fixed ? 3 :
        function == cuda.route_compact8 ? 4 :
        function == cuda.mmvq_up_gate_glu ? 7 :
        function == cuda.mmvq_shared_up_gate_glu ? 6 :
        index == entries ? REPLAY_ARGS + 1 : goose_cuda_arg_counts[index].count;
    if (args > REPLAY_ARGS) return NULL;
    struct replay_layout layout = { .function = function, .count = args };
    for (size_t i = 0; i < layout.count; ++i) {
        size_t offset, bytes;
        const int error = cuda.function_param(function, i, &offset, &bytes);
        if (error || offset > REPLAY_BYTES || bytes > REPLAY_BYTES - offset) return NULL;
        layout.offset[i] = offset;
        layout.size[i] = bytes;
        if (offset + bytes > layout.bytes) layout.bytes = offset + bytes;
    }
    replay_layouts[replay_layout_count] = layout;
    return &replay_layouts[replay_layout_count++];
#else
    (void) function;
    return NULL;
#endif
}

static struct kernel_node_params replay_kernel_params(const struct replay_operation *op, void *args[REPLAY_ARGS]) {
    const struct replay_layout *layout = op->kernel.layout;
    for (size_t i = 0; i < layout->count; ++i)
        args[i] = replay_buffer.args + op->kernel.start + layout->offset[i];
    const unsigned *s = op->kernel.shape;
    return (struct kernel_node_params) { .function = layout->function, .grid_x = s[0], .grid_y = s[1],
        .grid_z = s[2], .block_x = s[3], .block_y = s[4], .block_z = s[5], .shared_bytes = s[6],
        .params = layout->count ? args : NULL };
}

static int record_buffered_decode(void) {
    enum { thread_local_capture = 1, wait_external = 1 };
    int error = cuda.graph_begin(cuda.graph_stream, thread_local_capture);
    if (error != 0) return error;
    decode_capture.buffering = false;
    for (size_t i = 0; error == 0 && i < replay_buffer.count; ++i) {
        const struct replay_operation *op = &replay_buffer.ops[i];
        if (op->kind == replay_kernel) {
            void *args[REPLAY_ARGS];
            const struct kernel_node_params p = replay_kernel_params(op, args);
            error = decode_raw.launch(p.function, p.grid_x, p.grid_y, p.grid_z,
                p.block_x, p.block_y, p.block_z, p.shared_bytes, cuda.graph_stream, p.params, NULL);
        } else if (op->kind == replay_h2d) {
            error = decode_raw.upload_async(op->copy.dst, (const void *) (uintptr_t) op->copy.src,
                op->copy.bytes, cuda.graph_stream);
        } else if (op->kind == replay_d2d) {
            error = cuda.copy_device_async(op->copy.dst, op->copy.src, op->copy.bytes, cuda.graph_stream);
        } else if (op->kind == replay_wait_planes) {
            error = cuda.stream_wait_event(cuda.graph_stream, cuda.expert_planes_ready, wait_external);
        } else {
            error = cuda.stream_wait_event(cuda.graph_stream, cuda.expert_uploads_ready, wait_external);
        }
    }
    return error;
}

static void release_replay_region(struct replay_region *region) {
    if (region->exec) cuda.graph_exec_destroy(region->exec);
    if (region->graph) cuda.graph_destroy(region->graph);
    free(region->ops);
    free(region->args);
    *region = (struct replay_region) {0};
}

static void clear_replay_regions(void) {
    if (!cuda.decode_reuse) return;
    for (size_t i = 0; i < DECODE_GRAPH_REGIONS; ++i) release_replay_region(&replay_regions[i]);
}

static bool update_replay_region(struct replay_region *region) {
    if (!region->exec || region->count != replay_buffer.count) return false;
    for (size_t i = 0; i < region->count; ++i) {
        const struct replay_operation *old = &region->ops[i], *op = &replay_buffer.ops[i];
        if (old->kind != op->kind) return false;
        if (op->kind == replay_kernel) {
            if (old->kernel.layout != op->kernel.layout || old->kernel.start != op->kernel.start) return false;
        } else if (op->kind != replay_wait && op->kind != replay_wait_planes &&
            (old->copy.dst != op->copy.dst || old->copy.src != op->copy.src || old->copy.bytes != op->copy.bytes)) {
            return false;
        }
    }
    for (size_t i = 0; i < region->count; ++i) {
        struct replay_operation *old = &region->ops[i];
        const struct replay_operation *op = &replay_buffer.ops[i];
        if (op->kind != replay_kernel) continue;
        const size_t start = op->kernel.start, bytes = op->kernel.layout->bytes;
        if (memcmp(old->kernel.shape, op->kernel.shape, sizeof op->kernel.shape) == 0 &&
            memcmp(region->args + start, replay_buffer.args + start, bytes) == 0) continue;
        void *args[REPLAY_ARGS];
        const struct kernel_node_params p = replay_kernel_params(op, args);
        if (cuda.graph_set_kernel(region->exec, region->nodes[i], &p) != 0) return false;
        memcpy(region->args + start, replay_buffer.args + start, bytes);
        *old = *op;
        ++decode_capture.changed_nodes;
    }
    return true;
}

static bool store_replay_region(struct replay_region *region, Graph graph, GraphExec exec) {
    // Only reuse captures that match the recorded linear stream command by command.
    GraphNode next = NULL;
    size_t count = 0;
    if (cuda.graph_roots(graph, NULL, &count) != 0 || count != 1 ||
        cuda.graph_roots(graph, &next, &count) != 0) return false;
    for (size_t i = 0; i < replay_buffer.count; ++i) {
        enum { gpu_kernel = 0, gpu_memcpy = 1, gpu_event_wait = 6 };
        const enum replay_kind kind = replay_buffer.ops[i].kind;
        const int expected = kind == replay_kernel ? gpu_kernel : replay_is_wait(kind) ? gpu_event_wait : gpu_memcpy;
        int type;
        if (cuda.graph_node_type(next, &type) != 0 || type != expected) return false;
        region->nodes[i] = next;
        count = 0;
        if (cuda.graph_dependents(next, NULL, &count) != 0 ||
            count != (size_t) (i + 1 < replay_buffer.count)) return false;
        if (count && cuda.graph_dependents(next, &next, &count) != 0) return false;
    }
    struct replay_operation *ops = malloc(replay_buffer.count * sizeof(*ops));
    uint8_t *args = malloc(replay_buffer.used ? replay_buffer.used : 1);
    if (!ops || !args) { free(ops); free(args); return false; }
    memcpy(ops, replay_buffer.ops, replay_buffer.count * sizeof(*ops));
    memcpy(args, replay_buffer.args, replay_buffer.used);
    region->graph = graph;
    region->exec = exec;
    region->count = replay_buffer.count;
    region->ops = ops;
    region->args = args;
    return true;
}

static int submit_buffered_decode(size_t index) {
    struct replay_region *region = index < DECODE_GRAPH_REGIONS ? &replay_regions[index] : NULL;
    if (region && update_replay_region(region)) {
        ++decode_capture.reused;
        return cuda.graph_launch(region->exec, cuda.graph_stream);
    }
    if (region) release_replay_region(region);
    Graph graph = NULL;
    GraphExec exec = NULL;
    int error = record_buffered_decode();
    if (!decode_capture.buffering) {
        const int ended = cuda.graph_end(cuda.graph_stream, &graph);
        if (error == 0) error = ended;
    }
    if (error == 0) error = cuda.graph_instantiate(&exec, graph, 0);
    if (error == 0) error = cuda.graph_launch(exec, cuda.graph_stream);
    const bool cached = error == 0 && region && store_replay_region(region, graph, exec);
    if (!cached) {
        if (exec) cuda.graph_exec_destroy(exec);
        if (graph) cuda.graph_destroy(graph);
    }
    return error;
}

static int decode_capture_submit(void) {
    if (!decode_capture.capturing) return 0;
    decode_capture.capturing = false;
    if (decode_capture.buffering) {
        const int error = submit_buffered_decode(decode_capture.region++);
        decode_capture.buffering = false;
        if (error == 0) {
            ++decode_capture.replays;
            if (decode_capture.kernels > decode_capture.maximum) decode_capture.maximum = decode_capture.kernels;
        }
        decode_capture.kernels = 0;
        return error;
    }
    Graph graph = NULL;
    int error = cuda.graph_end(cuda.graph_stream, &graph);
    if (error != 0) return error;
    GraphExec temporary = NULL;
    GraphExec * exec = decode_capture.region < DECODE_GRAPH_REGIONS ?
        &cuda.decode_regions[decode_capture.region++] : &temporary;
    if (*exec) {
        struct graph_update_result result = {0};
        if (cuda.graph_update(*exec, graph, &result) == 0 && result.result == 0) {
            ++decode_capture.updates;
        } else {
            cuda.graph_exec_destroy(*exec);
            *exec = NULL;
        }
    }
    if (!*exec) error = cuda.graph_instantiate(exec, graph, 0);
    if (error == 0) error = cuda.graph_launch(*exec, cuda.graph_stream);
    if (error == 0) {
        ++decode_capture.replays;
        if (decode_capture.kernels > decode_capture.maximum) decode_capture.maximum = decode_capture.kernels;
    }
    if (temporary) cuda.graph_exec_destroy(temporary);
    cuda.graph_destroy(graph);
    decode_capture.kernels = 0;
    return error;
}

static int decode_capture_discard(void) {
    decode_capture.capturing = false;
    decode_capture.kernels = 0;
    Graph graph = NULL;
    const int ended = decode_capture.buffering ? 0 : cuda.graph_end(cuda.graph_stream, &graph);
    decode_capture.buffering = false;
    const int destroyed = graph ? cuda.graph_destroy(graph) : 0;
    const int drained = decode_raw.sync();
    return ended ? ended : destroyed ? destroyed : drained;
}

static int decode_capture_begin(void) {
    if (decode_capture.capturing || !decode_capture.scope) return 0;
    enum { blocking_stream = 0, thread_local_capture = 1 };
    int error = cuda.graph_stream ? 0 : cuda.stream_create(&cuda.graph_stream, blocking_stream);
    if (error == 0 && cuda.decode_reuse) {
        replay_buffer.count = 0;
        replay_buffer.used = 0;
        decode_capture.buffering = true;
    } else if (error == 0) {
        error = cuda.graph_begin(cuda.graph_stream, thread_local_capture);
    }
    if (error == 0) decode_capture.capturing = true;
    return error;
}

static bool buffer_decode_kernel(const struct replay_layout *layout, const unsigned shape[7], void **params) {
    const size_t start = (replay_buffer.used + 15) & ~(size_t) 15;
    if (!layout || replay_buffer.count == REPLAY_OPS || start > REPLAY_BYTES ||
        layout->bytes > REPLAY_BYTES - start || (layout->count && !params)) return false;
    for (size_t i = 0; i < layout->count; ++i)
        if (!params[i]) return false;
    uint8_t *args = replay_buffer.args + start;
    memset(replay_buffer.args + replay_buffer.used, 0, start - replay_buffer.used + layout->bytes);
    for (size_t i = 0; i < layout->count; ++i)
        memcpy(args + layout->offset[i], params[i], layout->size[i]);
    struct replay_operation *op = &replay_buffer.ops[replay_buffer.count++];
    *op = (struct replay_operation) { .kind = replay_kernel, .kernel = { .layout = layout, .start = start } };
    memcpy(op->kernel.shape, shape, sizeof op->kernel.shape);
    replay_buffer.used = start + layout->bytes;
    return true;
}

// Diagnostic/candidate (GOOSE_DECODE_SPLIT=N): after a decode FFN submission, submit the
// following region every N buffered kernels so the GPU starts the next layer while the
// host still records the rest. Kernels and their order are unchanged.
static int decode_split_every = -1, decode_split_armed;
static int decode_capture_submit(void);
static int decode_launch(Function kernel, unsigned gx, unsigned gy, unsigned gz, unsigned bx, unsigned by,
                         unsigned bz, unsigned shared, void * stream, void ** params, void ** extra) {
    if (decode_split_every < 0) { const char * v = getenv("GOOSE_DECODE_SPLIT"); decode_split_every = v ? atoi(v) : 0; }
    const int error = stream ? decode_capture_submit() : decode_capture_begin();
    if (error != 0) return error;
    if (decode_capture.capturing) {
        stream = cuda.graph_stream;
        ++decode_capture.kernels;
    }
    if (decode_capture.buffering) {
        const unsigned shape[] = {gx, gy, gz, bx, by, bz, shared};
        if (!extra && buffer_decode_kernel(replay_layout_for(kernel), shape, params)) {
            if (decode_split_armed && decode_split_every > 0 && decode_capture.kernels >= decode_split_every) {
                decode_split_armed = decode_split_armed > 1 ? decode_split_armed - 1 : 0;
                return decode_capture_submit();
            }
            return 0;
        }
        const int recorded = record_buffered_decode();
        if (recorded != 0) return recorded;
    }
    return decode_raw.launch(kernel, gx, gy, gz, bx, by, bz, shared, stream, params, extra);
}

static int decode_upload_async(DevicePtr dst, const void * src, size_t bytes, Stream stream) {
    const int error = stream ? decode_capture_submit() : decode_capture_begin();
    if (error != 0) return error;
    if (decode_capture.capturing) stream = cuda.graph_stream;
    if (decode_capture.buffering) {
        if (replay_buffer.count < REPLAY_OPS) {
            replay_buffer.ops[replay_buffer.count++] = (struct replay_operation) {
                .kind = replay_h2d, .copy = {dst, (uintptr_t) src, bytes} };
            return 0;
        }
        const int recorded = record_buffered_decode();
        if (recorded != 0) return recorded;
    }
    return decode_raw.upload_async(dst, src, bytes, stream);
}

static const void * snapshot_packet(const void * data, size_t bytes) {
    if (!cuda.packet_host || !bytes || bytes > MIB_BYTES) return NULL;
    const int64_t checked = (int64_t) (bytes > 4096 ? 4096 : bytes);
    const int64_t slot = fg143_pinned_packet_slot((int64_t) activations.packet_used, MIB_BYTES, checked);
    if (slot < 0 || bytes > MIB_BYTES - (size_t) slot) return NULL;
    unsigned char * host = cuda.packet_host + slot;
    memcpy(host, data, bytes);
    activations.packet_used = (size_t) slot + bytes;
    activations.pending = true;
    return host;
}

static int decode_upload(DevicePtr dst, const void * src, size_t bytes) {
    if (decode_capture.scope && decode_raw.upload_async && bytes == 8192) {
        // Device-only consumers wait through the captured stream. Retain a
        // pinned copy because the caller may release its host buffer now.
        const void * host = snapshot_packet(src, bytes);
        if (host) return decode_upload_async(dst, host, bytes, NULL);
    }
    const int error = decode_capture_submit();
    return error == 0 ? decode_raw.upload(dst, src, bytes) : error;
}

static int decode_copy_device(DevicePtr dst, DevicePtr src, size_t bytes) {
    if (decode_capture.scope && cuda.copy_device_async) {
        int error = decode_capture_begin();
        if (error == 0 && decode_capture.buffering) {
            if (replay_buffer.count < REPLAY_OPS) {
                replay_buffer.ops[replay_buffer.count++] = (struct replay_operation) {
                    .kind = replay_d2d, .copy = {dst, src, bytes} };
                return 0;
            }
            error = record_buffered_decode();
        }
        return error == 0 ? cuda.copy_device_async(dst, src, bytes, cuda.graph_stream) : error;
    }
    const int error = decode_capture_submit();
    return error == 0 ? decode_raw.copy_device(dst, src, bytes) : error;
}

static int decode_wait_for_expert_planes(void) {
    enum { wait_external = 1 };
    if (decode_capture.buffering) {
        if (replay_buffer.count < REPLAY_OPS) {
            replay_buffer.ops[replay_buffer.count++] = (struct replay_operation) { .kind = replay_wait_planes };
            return 0;
        }
        const int recorded = record_buffered_decode();
        if (recorded != 0) return recorded;
    }
    return cuda.stream_wait_event(cuda.graph_stream, cuda.expert_planes_ready, wait_external);
}

static int decode_wait_for_experts(void) {
    enum { wait_external = 1 };
    if (decode_capture.buffering) {
        if (replay_buffer.count < REPLAY_OPS) {
            replay_buffer.ops[replay_buffer.count++] = (struct replay_operation) { .kind = replay_wait };
            return 0;
        }
        const int recorded = record_buffered_decode();
        if (recorded != 0) return recorded;
    }
    return cuda.stream_wait_event(cuda.graph_stream, cuda.expert_uploads_ready, wait_external);
}

// Host accesses and allocation changes are submission barriers.
#define DECODE_BARRIER(name, parameters, args) \
    static int decode_##name parameters { \
        const int error = decode_capture_submit(); \
        return error == 0 ? decode_raw.name args : error; \
    }
DECODE_BARRIER(download, (void * dst, DevicePtr src, size_t bytes), (dst, src, bytes))
DECODE_BARRIER(zero, (DevicePtr dst, unsigned char value, size_t bytes), (dst, value, bytes))
DECODE_BARRIER(alloc, (DevicePtr * dst, size_t bytes), (dst, bytes))
DECODE_BARRIER(sync, (void), ())
DECODE_BARRIER(memory_info, (size_t * free_bytes, size_t * total_bytes), (free_bytes, total_bytes))
#undef DECODE_BARRIER

static int decode_release(DevicePtr dst) {
    const int error = decode_capture_submit();
    if (error != 0) return error;
    clear_replay_regions();
    return decode_raw.release(dst);
}

static int finish_device_state_write(void) {
    if (!decode_capture.capturing) return cuda.sync();
    activations.pending = true;
    return 0;
}

static void install_decode_capture(void) {
    cuda.graph_update = dlsym(cuda.library, "cuGraphExecUpdate_v2");
    if (!cuda.graph_update) return;
    cuda.copy_device_async = dlsym(cuda.library, "cuMemcpyDtoDAsync_v2");
#define DECODE_HOOK(name) do { decode_raw.name = cuda.name; cuda.name = decode_##name; } while (0)
    DECODE_HOOK(launch);
    DECODE_HOOK(upload);
    if (cuda.upload_async) DECODE_HOOK(upload_async);
    DECODE_HOOK(download);
    DECODE_HOOK(copy_device);
    DECODE_HOOK(zero);
    DECODE_HOOK(alloc);
    DECODE_HOOK(release);
    DECODE_HOOK(sync);
    DECODE_HOOK(memory_info);
#undef DECODE_HOOK
    cuda.decode_graph_enabled = true;
    const char *compact_ids = getenv("GOOSE_CUDA_COMPACT_MOE_IDS");
    cuda.compact_moe_ids = !compact_ids || strcmp(compact_ids, "0") != 0;
    const char *early_ffn = getenv("GOOSE_CUDA_DECODE_EARLY_FFN");
    cuda.decode_early_ffn = !early_ffn || strcmp(early_ffn, "0") != 0;
#ifdef GOOSE_CUDA_HAS_ARG_COUNTS
    const char * reuse = getenv("GOOSE_CUDA_DECODE_REUSE");
    if (reuse && strcmp(reuse, "1") == 0) {
        cuda.function_param = dlsym(cuda.library, "cuFuncGetParamInfo");
        cuda.function_name = dlsym(cuda.library, "cuFuncGetName");
        cuda.graph_node_type = dlsym(cuda.library, "cuGraphNodeGetType");
        cuda.decode_reuse = cuda.function_param && cuda.function_name && cuda.graph_node_type;
    }
#endif
    const char * overlap = getenv("GOOSE_CUDA_DECODE_EXPERT_OVERLAP");
    if (cuda.expert_stream && overlap && strcmp(overlap, "1") == 0 &&
        cuda.event_record(cuda.expert_uploads_ready, cuda.expert_stream) == 0) {
        cuda.expert_decode_overlap = true;
    }
}

uint8_t fg_cuda_decode_graph_enabled(void) {
    return cuda.decode_graph_enabled;
}

static pthread_once_t cuda_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t cuda_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t activation_lock = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local DevicePtr matrix_input, matrix_output;
static _Thread_local uint64_t matrix_reusable_host;
static _Thread_local struct {
    uint64_t target;
    int64_t bytes;
    bool packed;
} matrix_glu;
static _Thread_local bool matrix_glu_active;
static _Thread_local int64_t matrix_input_bytes;
static _Thread_local struct {
    uint64_t host;
    DevicePtr input, packed;
    int64_t bytes, step, rows, stride;
    int extent, layout;
    bool dense;
} shared_activation_pack;
static _Thread_local uint64_t cuda_matrices;
static _Thread_local uint64_t cuda_routed_groups, cuda_routed_replays;
static _Thread_local uint64_t cuda_full_ffn_groups, cuda_full_ffn_replays;
static _Thread_local uint64_t cuda_matrix_launches, cuda_cached_projections;
static _Thread_local uint64_t cuda_mmq_projections;
static _Thread_local uint64_t cuda_mmq_stream_windows;
static _Thread_local uint64_t cuda_mmq_fixups;
static _Thread_local uint64_t cuda_dense_mma;
static _Thread_local uint64_t cuda_f32_matrices;
static _Thread_local int cuda_matrix_kind;
static _Thread_local uint64_t cuda_attentions;
static _Thread_local uint64_t cuda_causal_attentions;
static _Thread_local uint64_t cuda_gdns;
static _Thread_local uint64_t cuda_gdn_mmas;
static _Thread_local uint64_t cuda_flash_mmas;
static _Thread_local uint64_t cuda_resident_kv, cuda_resident_gdn;
static _Thread_local uint64_t cuda_q4_writes;
static _Thread_local uint64_t cuda_vectors;
static _Thread_local uint64_t cuda_fwhts;
static _Thread_local uint64_t cuda_moe_weighted_tiles;
static _Thread_local uint64_t cuda_ropes, cuda_convs;
static _Thread_local uint64_t cuda_routes[5];
static _Thread_local struct {
    uint64_t scores_host, ids_host;
    DevicePtr scores_device;
} route_compact_pending;
static _Thread_local uint64_t cuda_blits[3];
static _Thread_local struct {
    uint64_t source, destination;
    int64_t bytes;
} published_state;

static void free_expert_metadata(struct expert_cache * cache) {
    free(cache->groups.data);
    free(cache->layers.data);
    free(cache->work.data);
    free(cache->state);
}

static int release_vmm(struct device_buffer * buffer) {
    const int sync_error = buffer->mappings ? cuda.sync() : 0;
    if (sync_error != 0) return sync_error;
    clear_replay_regions();
    while (buffer->mappings) {
        struct vmm_mapping * segment = buffer->mappings;
        if (segment->mapped) {
            const int error = cuda.vmm_unmap(buffer->address + segment->offset, segment->bytes);
            if (error != 0) return error;
            segment->mapped = false;
            buffer->bytes = segment->offset;
        }
        const int error = cuda.vmm_release(segment->handle);
        if (error != 0) return error;
        buffer->mappings = segment->previous;
        free(segment);
    }
    const int error = cuda.vmm_free(buffer->address, buffer->reservation_bytes);
    if (error == 0) {
        if (getenv("GOOSE_GRAPH_LOG")) fprintf(stderr, "Goose CUDA VMM: freed base=%#" PRIx64 "\n", (uint64_t) buffer->address);
        *buffer = (struct device_buffer) {0};
    }
    return error;
}

static void init_vmm(int device) {
    enum { vmm_attribute = 102, allocation_pinned = 1, location_device = 1, minimum_granularity = 0 };
    const char * setting = getenv("GOOSE_CUDA_VMM");
    const bool scratch = setting && strcmp(setting, "1") == 0;
    const bool lazy = fg_standalone_option_enabled("GOOSE_KV_LAZY_VMM");
    int available = 0;
    if ((!scratch && !lazy) ||
        cuda.device_attribute(&available, vmm_attribute, device) != 0 || !available) return;
#define VMM_LOAD(field, symbol) do { cuda.field = dlsym(cuda.library, symbol); if (!cuda.field) return; } while (0)
    VMM_LOAD(vmm_get_granularity, "cuMemGetAllocationGranularity");
    VMM_LOAD(vmm_reserve, "cuMemAddressReserve");
    VMM_LOAD(vmm_free, "cuMemAddressFree");
    VMM_LOAD(vmm_create, "cuMemCreate");
    VMM_LOAD(vmm_map, "cuMemMap");
    VMM_LOAD(vmm_access, "cuMemSetAccess");
    VMM_LOAD(vmm_unmap, "cuMemUnmap");
    VMM_LOAD(vmm_release, "cuMemRelease");
#undef VMM_LOAD
    cuda.vmm_properties = (struct memory_properties) {.type = allocation_pinned,
        .location = {location_device, device}};
    size_t granularity = 0;
    if (cuda.vmm_get_granularity(&granularity, &cuda.vmm_properties, minimum_granularity) == 0 &&
        granularity > 0) {
        cuda.vmm_granularity = granularity;
        cuda.scratch_vmm = scratch;
        cuda.lazy_kv = lazy;
    }
}

static void init_cuda_graphs(void) {
    const char * flash = getenv("GOOSE_CUDA_GRAPH");
    const char * mmvq = getenv("GOOSE_CUDA_MMVQ_GRAPH");
    const char * pack = getenv("GOOSE_CUDA_GRAPH_PACK");
    const char * routed = getenv("GOOSE_CUDA_ROUTED_GRAPH");
    const char * full_ffn = getenv("GOOSE_CUDA_FULL_FFN_GRAPH");
    const bool flash_requested = cuda.flash_vec && flash && strcmp(flash, "1") == 0;
    const bool mmvq_requested = cuda.mmvq && mmvq && strcmp(mmvq, "1") == 0;
    const bool routed_requested = cuda.mmvq && routed && strcmp(routed, "1") == 0;
    if (!flash_requested && !mmvq_requested && !routed_requested) return;
#define GRAPH_LOAD(field, symbol) do { cuda.field = dlsym(cuda.library, symbol); if (!cuda.field) return; } while (0)
    GRAPH_LOAD(stream_create, "cuStreamCreate");
    GRAPH_LOAD(stream_destroy, "cuStreamDestroy_v2");
    GRAPH_LOAD(graph_begin, "cuStreamBeginCapture_v2");
    GRAPH_LOAD(graph_end, "cuStreamEndCapture");
    GRAPH_LOAD(graph_roots, "cuGraphGetRootNodes");
    GRAPH_LOAD(graph_dependents, "cuGraphNodeGetDependentNodes");
    GRAPH_LOAD(graph_instantiate, "cuGraphInstantiateWithFlags");
    GRAPH_LOAD(graph_set_kernel, "cuGraphExecKernelNodeSetParams_v2");
    GRAPH_LOAD(graph_launch, "cuGraphLaunch");
    GRAPH_LOAD(graph_destroy, "cuGraphDestroy");
    GRAPH_LOAD(graph_exec_destroy, "cuGraphExecDestroy");
#undef GRAPH_LOAD
    cuda.flash_graph_enabled = flash_requested;
    cuda.flash_graph_pack = flash_requested && pack && strcmp(pack, "1") == 0;
    if (mmvq_requested || routed_requested) {
        cuda.mmvq_graph_cache = calloc(MMVQ_GRAPH_SLOTS, sizeof(*cuda.mmvq_graph_cache));
        cuda.mmvq_graph_enabled = mmvq_requested;
        cuda.routed_graph_enabled = routed_requested;
        cuda.full_ffn_enabled = routed_requested && full_ffn && strcmp(full_ffn, "1") == 0;
        const char * decode = getenv("GOOSE_CUDA_DECODE_GRAPH");
        if (cuda.full_ffn_enabled && decode && strcmp(decode, "1") == 0) install_decode_capture();
    }
}

static void release_mmvq_graphs(void) {
    if (cuda.mmvq_graph_cache) {
        for (size_t i = 0; i < MMVQ_GRAPH_SLOTS; ++i) {
            if (cuda.mmvq_graph_cache[i].executable) cuda.graph_exec_destroy(cuda.mmvq_graph_cache[i].executable);
        }
        free(cuda.mmvq_graph_cache);
        cuda.mmvq_graph_cache = NULL;
    }
}

static void release_flash_graph(void) {
    if (cuda.flash_graph_exec) cuda.graph_exec_destroy(cuda.flash_graph_exec);
    if (cuda.flash_graph) cuda.graph_destroy(cuda.flash_graph);
    cuda.flash_graph_exec = NULL;
    cuda.flash_graph = NULL;
    cuda.flash_graph_node_count = 0;
}

static void release_expert_stream(void) {
    struct expert_host_ring * ring = &cuda.expert_host;
    if (expert_copy_worker.running) {
        pthread_mutex_lock(&expert_copy_worker.lock);
        expert_copy_worker.stop = true;
        pthread_cond_signal(&expert_copy_worker.ready);
        pthread_mutex_unlock(&expert_copy_worker.lock);
        pthread_join(expert_copy_worker.thread, NULL);
        expert_copy_worker.running = false;
    }
    for (size_t i = 0; i < EXPERT_HOST_SLOTS; ++i) {
        if (ring->ready[i]) cuda.event_destroy(ring->ready[i]);
    }
    if (ring->storage) cuda.host_free(ring->storage);
    *ring = (struct expert_host_ring) {0};
    if (cuda.expert_stream) cuda.stream_destroy(cuda.expert_stream);
    if (cuda.expert_uploads_ready) cuda.event_destroy(cuda.expert_uploads_ready);
    if (cuda.expert_planes_ready) cuda.event_destroy(cuda.expert_planes_ready);
    if (cuda.expert_slots_ready) cuda.event_destroy(cuda.expert_slots_ready);
    for (size_t i = 0; i < EXPERT_SLOT_EVENTS; ++i)
        if (cuda.expert_slots_ring[i]) cuda.event_destroy(cuda.expert_slots_ring[i]);
    cuda.expert_stream = NULL;
    cuda.expert_uploads_ready = NULL;
    cuda.expert_planes_ready = NULL;
    cuda.expert_slots_ready = NULL;
    for (size_t i = 0; i < EXPERT_SLOT_EVENTS; ++i) cuda.expert_slots_ring[i] = NULL;
    cuda.expert_slots_index = 0;
    cuda.expert_slots_records = 0;
}

// A rejected experiment must leave the accepted profile unchanged. These
// settings are enabled only when the variable is explicitly set to 1.
static bool expert_option_explicit(const char * name) {
    const char * setting = getenv(name);
    return setting && strcmp(setting, "1") == 0;
}

static void init_expert_stream(void) {    enum { nonblocking_stream = 1, disable_timing = 2 };
    if (!fg_standalone_option_enabled("GOOSE_CUDA_EXPERT_OVERLAP")) return;
#define EXPERT_LOAD(field, symbol) do { cuda.field = dlsym(cuda.library, symbol); if (!cuda.field) return; } while (0)
    EXPERT_LOAD(upload_async, "cuMemcpyHtoDAsync_v2");
    EXPERT_LOAD(stream_create, "cuStreamCreate");
    EXPERT_LOAD(stream_destroy, "cuStreamDestroy_v2");
    EXPERT_LOAD(stream_wait_event, "cuStreamWaitEvent");
    EXPERT_LOAD(event_create, "cuEventCreate");
    EXPERT_LOAD(event_destroy, "cuEventDestroy_v2");
    EXPERT_LOAD(event_record, "cuEventRecord");
#undef EXPERT_LOAD
    const char *disable_batch = getenv("GOOSE_CUDA_DISABLE_EXPERT_BATCH");
    if (!disable_batch || strcmp(disable_batch, "1") != 0)
        cuda.upload_batch = dlsym(cuda.library, "cuMemcpyBatchAsync_v2");
    int error = cuda.stream_create(&cuda.expert_stream, nonblocking_stream);
    for (size_t i = 0; i < EXPERT_SLOT_EVENTS && error == 0; ++i)
        error = cuda.event_create(&cuda.expert_slots_ring[i], disable_timing);
    if (error == 0) error = cuda.event_create(&cuda.expert_uploads_ready, disable_timing);
    if (error == 0) error = cuda.event_create(&cuda.expert_planes_ready, disable_timing);
    if (error == 0) cuda.expert_split_planes = expert_option_explicit("GOOSE_CUDA_EXPERT_SPLIT_PLANES");
    if (error == 0) cuda.expert_async_copies = expert_option_explicit("GOOSE_CUDA_EXPERT_ASYNC_COPIES");
    if (error != 0) release_expert_stream();
    if (error == 0 && fg_standalone_option_enabled("GOOSE_CUDA_EXPERT_STAGE_PAGEABLE") &&
        cuda.host_alloc && cuda.host_free) {
        cuda.event_sync = dlsym(cuda.library, "cuEventSynchronize");
        cuda.expert_host.enabled = cuda.event_sync != NULL;
        cuda.expert_prefill_staging = fg_standalone_option_enabled("GOOSE_CUDA_EXPERT_STAGE_CURRENT");
        cpu_set_t affinity;
        if (cuda.expert_host.enabled && fg_standalone_option_enabled("GOOSE_CUDA_EXPERT_PARALLEL_COPY") &&
            sched_getaffinity(0, sizeof affinity, &affinity) == 0 && CPU_COUNT(&affinity) > 1) {
            expert_copy_worker.running = pthread_create(&expert_copy_worker.thread, NULL, copy_expert_worker_main, NULL) == 0;
        }
    }
}

static void cleanup(void) {
    if (cuda.context) {
        if (cuda.push(cuda.context) == 0) {
            if (cuda.decode_reuse) cuda.sync();
            clear_replay_regions();
            if (cuda.expert_stream) {
                cuda.sync();
                release_expert_stream();
            }
            if (cuda.expert_pins.len > 0) ec_manage_pins(cuda.expert_pins, 0);
            if (cuda.packet_host) {
                cuda.sync();
                cuda.host_free(cuda.packet_host);
            }
            release_flash_graph();
            release_mmvq_graphs();
            if (cuda.experts.prefill_ready) cuda.event_destroy(cuda.experts.prefill_ready);
            if (cuda.experts.prefill_release) cuda.event_destroy(cuda.experts.prefill_release);
            for (size_t i = 0; i < DECODE_GRAPH_REGIONS; ++i) {
                if (cuda.decode_regions[i]) cuda.graph_exec_destroy(cuda.decode_regions[i]);
            }
            if (cuda.graph_stream) cuda.stream_destroy(cuda.graph_stream);
            if (cuda.attention_scratch.reservation_bytes) release_vmm(&cuda.attention_scratch);
            if (cuda.mmq_packed.reservation_bytes) release_vmm(&cuda.mmq_packed);
            for (int64_t i = 0; cuda.state_views && i < cuda.state_ranges.len / 4; ++i) {
                if (cuda.state_views[i].reservation_bytes) release_vmm(&cuda.state_views[i]);
            }
            Context previous;
            cuda.pop(&previous);
        }
        cuda.destroy(cuda.context);
    }
    free(cuda.dense_ranges.data);
    free(cuda.state_ranges.data);
    free(cuda.state_views);
    free(cuda.expert_pins.data);
    free(cuda.expert_upload_jobs);
    free_expert_metadata(&cuda.experts);
    if (cuda.library) dlclose(cuda.library);
}

static void load_cuda(void) {
    if (goose_cuda_image[0] == 0) return;
    cuda.library = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!cuda.library) return;
#define LOAD(field, name) do { cuda.field = dlsym(cuda.library, name); if (!cuda.field) return; } while (0)
    LOAD(init, "cuInit");
    LOAD(device_get, "cuDeviceGet");
    LOAD(device_attribute, "cuDeviceGetAttribute");
    LOAD(create, "cuCtxCreate_v2");
    LOAD(destroy, "cuCtxDestroy_v2");
    LOAD(push, "cuCtxPushCurrent_v2");
    LOAD(pop, "cuCtxPopCurrent_v2");
    LOAD(module_load, "cuModuleLoadData");
    LOAD(function_get, "cuModuleGetFunction");
    LOAD(function_set_attribute, "cuFuncSetAttribute");
    LOAD(alloc, "cuMemAlloc_v2");
    LOAD(memory_info, "cuMemGetInfo_v2");
    LOAD(release, "cuMemFree_v2");
    LOAD(upload, "cuMemcpyHtoD_v2");
    LOAD(download, "cuMemcpyDtoH_v2");
    LOAD(copy_device, "cuMemcpyDtoD_v2");
    LOAD(zero, "cuMemsetD8_v2");
    LOAD(launch, "cuLaunchKernel");
    LOAD(sync, "cuCtxSynchronize");
#undef LOAD
    cuda.host_register = dlsym(cuda.library, "cuMemHostRegister_v2");
    cuda.host_unregister = dlsym(cuda.library, "cuMemHostUnregister");
    const char * no_pinned = getenv("GGML_CUDA_NO_PINNED");
    if (fg_standalone_option_enabled("GOOSE_CUDA_ASYNC_PACKETS") &&
        (!no_pinned || strcmp(no_pinned, "0") == 0)) {
        cuda.host_alloc = dlsym(cuda.library, "cuMemHostAlloc");
        cuda.host_free = dlsym(cuda.library, "cuMemFreeHost");
        cuda.upload_async = dlsym(cuda.library, "cuMemcpyHtoDAsync_v2");
        cuda.async_packets = cuda.host_alloc && cuda.host_free && cuda.upload_async;
    }
    int device;
    if (cuda.init(0) != 0 || cuda.device_get(&device, 0) != 0) return;
    cuda.status = -1;
    const char * arena_setting = getenv("GOOSE_CUDA_ACTIVATION_MIB");
    cuda.activation_capacity = DEFAULT_ACTIVATION_BYTES;
    cuda.adapt_activation_capacity = !arena_setting;
    cuda.direct_wide_vectors = fg_standalone_option_enabled("GOOSE_CUDA_WIDE_VECTOR");
    cuda.stage_matrix_input = fg_standalone_option_enabled("GOOSE_CUDA_STAGE_MATRIX_INPUT");
    cuda.defer_matrix_waits = fg_standalone_option_enabled("GOOSE_CUDA_DEFER_MATRIX_WAITS");
    if (arena_setting) {
        char * end;
        const intmax_t value = strtoimax(arena_setting, &end, 10);
        if (end == arena_setting || *end || value < 64 || value > INT64_MAX / MIB_BYTES) {
            fputs("Goose CUDA: GOOSE_CUDA_ACTIVATION_MIB requires a valid budget of at least 64 MiB\n", stderr);
            return;
        }
        cuda.activation_capacity = (size_t) value * MIB_BYTES;
    }
    cuda.mmq_window = fg_standalone_option_enabled("GOOSE_MMQ_WINDOW");
    cuda.mmq_align_window = fg_standalone_option_enabled("GOOSE_MMQ_ALIGN_WINDOW");
    cuda.mmq_shared_reuse = fg_standalone_option_enabled("GOOSE_MMQ_SHARED_REUSE");
    cuda.mmq_dense_reuse = fg_standalone_option_enabled("GOOSE_MMQ_DENSE_REUSE");
    cuda.mmq_dense_partial = fg_standalone_option_enabled("GOOSE_MMQ_DENSE_PARTIAL");
    cuda.mmq_shared = fg_standalone_option_enabled("GOOSE_MMQ_SHARED");
    cuda.moe_topk = fg_standalone_option_enabled("GOOSE_MOE_TOPK");
    const char * setting = getenv("GOOSE_MMQ");
    const char * stream_setting = getenv("GOOSE_MMQ_STREAM_K");
    const bool stream_k = stream_setting && strcmp(stream_setting, "1") == 0;
    const bool mmq = stream_k || (setting && strcmp(setting, "1") == 0);
    const bool mmvq_requested = fg_standalone_option_enabled("GOOSE_MMVQ");
    const char * mmvq_tail = getenv("GOOSE_MMVQ_DENSE_TAIL");
    cuda.mmvq_dense_tail = mmvq_tail && strcmp(mmvq_tail, "1") == 0;
    const char * dense_setting = getenv("GOOSE_DENSE_MMA");
    const bool dense_mma = dense_setting && strcmp(dense_setting, "1") == 0;
    const char * dense_reference_setting = getenv("GOOSE_DENSE_MMA_REFERENCE");
    const bool dense_reference = dense_mma && dense_reference_setting && strcmp(dense_reference_setting, "1") == 0;
    const char * gdn_setting = getenv("GOOSE_GDN_MMA");
    const bool gdn_mma = gdn_setting && strcmp(gdn_setting, "1") == 0;
    const char * flash_setting = getenv("GOOSE_FLASH_MMA");
    const bool flash_mma = flash_setting && strcmp(flash_setting, "1") == 0;
    const bool vec_pack = fg_standalone_option_enabled("GOOSE_FLASH_VEC_PREPACK");
    const char * flash_stream_setting = getenv("GOOSE_FLASH_STREAM_K");
    cuda.flash_stream_all = flash_mma && flash_stream_setting && strcmp(flash_stream_setting, "1") == 0;
    const char * flash_tail_setting = getenv("GOOSE_FLASH_TAIL_MMA");
    cuda.flash_tail_mma = flash_mma && flash_tail_setting && strcmp(flash_tail_setting, "1") == 0;
    const bool flash_stream = cuda.flash_stream_all || cuda.flash_tail_mma;
    const bool flash_vec_requested = fg_standalone_option_enabled("GOOSE_FLASH_VEC");
    const char * rms_setting = getenv("GOOSE_RMS_REFERENCE");
    const bool rms_reference = rms_setting && strcmp(rms_setting, "1") == 0;
    const char * q6_setting = getenv("GOOSE_MMQ_Q6_REFERENCE");
    const bool q6_reference = mmq && q6_setting && strcmp(q6_setting, "1") == 0;
    const bool q6_shared = fg_standalone_option_enabled("GOOSE_MMQ_Q6_SHARED");
    const bool q6_x64 = fg_standalone_option_enabled("GOOSE_MMQ_Q6_X64");
    const char * q4_setting = getenv("GOOSE_MMQ_Q4_REFERENCE");
    const bool q4_reference = mmq && q4_setting && strcmp(q4_setting, "1") == 0;
    const bool q4_shared = fg_standalone_option_enabled("GOOSE_MMQ_Q4_WIDE_SHARED");
    const bool q4_routed_shared = fg_standalone_option_enabled("GOOSE_MMQ_Q4_ROUTED_SHARED");
    const bool q4_x64 = fg_standalone_option_enabled("GOOSE_MMQ_Q4_X64");
    const bool dense_x128 = fg_standalone_option_enabled("GOOSE_MMQ_DENSE_X128");
    const bool routed_x128 = fg_standalone_option_enabled("GOOSE_MMQ_ROUTED_X128");
    const char * ssm_setting = getenv("GOOSE_SSM_REFERENCE");
    const bool ssm_reference = ssm_setting && strcmp(ssm_setting, "1") == 0;
    const char * activation_setting = getenv("GOOSE_CUDA_ACTIVATION_REFERENCE");
    cuda.activation_reference = activation_setting && strcmp(activation_setting, "1") == 0;
    const bool tensor_cores = mmq || dense_mma || gdn_mma || flash_mma;
    enum { compute_capability_major = 75, compute_capability_minor = 76 };
    int major = 0, minor = 0;
    const bool sm80_image = (tensor_cores || mmvq_requested || flash_vec_requested) && goose_cuda_mmq_image[0] != 0 &&
        cuda.device_attribute(&major, compute_capability_major, device) == 0 && major >= 8;
    if (tensor_cores && !sm80_image) {
        fputs("Goose CUDA: Tensor Core paths require the SM80 image and compute capability 8 or newer\n", stderr);
        return;
    }
    const bool mmvq = mmvq_requested && sm80_image;
    const bool flash_vec = flash_vec_requested && sm80_image;
    const bool mma_image = tensor_cores || mmvq || flash_vec;
    const bool reference_arch = (q4_reference || q6_reference || dense_reference) && major == 8 &&
        cuda.device_attribute(&minor, compute_capability_minor, device) == 0 && minor == 9;
    if (stream_k || flash_stream || (reference_arch && (q4_reference || q6_reference))) {
        enum { multiprocessor_count = 16 };
        if (cuda.device_attribute(&cuda.multiprocessors, multiprocessor_count, device) != 0 || cuda.multiprocessors <= 0) return;
    }
    int error = cuda.create(&cuda.context, 0, device);
    if (error == 0) {
        init_vmm(device);
        init_expert_stream();
        error = cuda.module_load(&cuda.module, mma_image ? goose_cuda_mmq_image : goose_cuda_image);
        if (error == 0) error = cuda.function_get(&cuda.kernel, cuda.module, "goose_quantized_matmul");
        if (error == 0 && mmq) error = cuda.function_get(&cuda.mmq_pack, cuda.module, "goose_mmq_pack_q8");
        if (error == 0 && mmq) error = cuda.function_get(&cuda.mmq_pack_shared, cuda.module, "goose_mmq_pack_shared");
        if (error == 0 && mmq && fg_standalone_option_enabled("GOOSE_MMQ_SWIGLU_PACK"))
            (void) cuda.function_get(&cuda.mmq_pack_glu, cuda.module, "goose_mmq_pack_swiglu");
        if (error == 0 && mmq) error = cuda.function_get(&cuda.mmq_matrix, cuda.module, "goose_mmq_matmul");
        if (error == 0 && mmvq) error = cuda.function_get(&cuda.mmvq_pack, cuda.module, "goose_mmvq_quantize");
        if (error == 0 && mmvq) error = cuda.function_get(&cuda.mmvq, cuda.module, "goose_mmvq_matvec");
        if (error == 0 && mmvq)
            (void) cuda.function_get(&cuda.mmvq_short, cuda.module, "goose_mmvq_short_rows");
        if (error == 0 && mmvq) {
            (void) cuda.function_get(&cuda.mmvq_dense_pack, cuda.module, "goose_mmvq_dense_quantize");
            (void) cuda.function_get(&cuda.mmvq_dense_row, cuda.module, "goose_mmvq_dense_matvec");
        }
        if (error == 0 && reference_arch && q4_reference) {
            error = cuda.function_get(&cuda.mmq_q4_matrix, cuda.module, "goose_mmq_q4_reference");
            if (error == 0 && q4_shared)
                error = cuda.function_get(&cuda.mmq_q4_dense_shared, cuda.module, "goose_mmq_q4_dense_shared");
            if (error == 0 && q4_routed_shared)
                error = cuda.function_get(&cuda.mmq_q4_routed_shared, cuda.module, "goose_mmq_q4_routed_shared");
            if (error == 0 && q4_x64 && q4_shared)
                error = cuda.function_get(&cuda.mmq_q4_dense_x64, cuda.module, "goose_mmq_q4_dense_x64");
            if (error == 0 && q4_x64 && q4_shared && dense_x128)
                error = cuda.function_get(&cuda.mmq_q4_dense_x128, cuda.module, "goose_mmq_q4_dense_x128");
            const char * q4_m64 = getenv("GOOSE_MMQ_Q4_M64");
            if (error == 0 && cuda.mmq_q4_dense_x128 && q4_m64 && strcmp(q4_m64, "1") == 0)
                error = cuda.function_get(&cuda.mmq_q4_dense_m64, cuda.module, "goose_mmq_q4_dense_m64");
            const char * q4_m128 = getenv("GOOSE_MMQ_Q4_M128");
            if (error == 0 && cuda.mmq_q4_dense_m64 && q4_m128 && strcmp(q4_m128, "1") == 0)
                error = cuda.function_get(&cuda.mmq_q4_dense_m128, cuda.module, "goose_mmq_q4_dense_m128");
            const char * q4_large = getenv("GOOSE_MMQ_Q4_LARGE_TILE");
            if (error == 0 && cuda.mmq_q4_dense_m128 && q4_large) {
                const bool more_columns = strcmp(q4_large, "128x256") == 0;
                if (more_columns || strcmp(q4_large, "256x128") == 0) {
                    const char * name = more_columns ? "goose_mmq_q4_dense_m128n256" : "goose_mmq_q4_dense_m256n128";
                    error = cuda.function_get(&cuda.mmq_q4_dense_large, cuda.module, name);
                    cuda.mmq_q4_large_rows = more_columns ? 128 : 256;
                    cuda.mmq_q4_large_cols = more_columns ? 256 : 128;
                    if (error == 0 && more_columns && fg_standalone_option_enabled("GOOSE_MMQ_Q4_PREPARE")) {
                        error = cuda.function_get(&cuda.mmq_q4_prepare, cuda.module, "goose_mmq_q4_prepare");
                        if (error == 0) error = cuda.function_get(&cuda.mmq_q4_dense_prepared, cuda.module,
                            "goose_mmq_q4_dense_m128n256_prepared");
                        if (error == 0 && fg_standalone_option_enabled("GOOSE_MMQ_Q4_PIPELINE")) {
                            enum { max_dynamic_shared_size = 8 };
                            int setup = cuda.function_get(&cuda.mmq_q4_dense_pipeline, cuda.module,
                                "goose_mmq_q4_dense_m128n256_pipeline");
                            if (setup == 0) setup = cuda.function_set_attribute(cuda.mmq_q4_dense_pipeline,
                                max_dynamic_shared_size, Q4_PIPE_SHARED_BYTES);
                            if (setup != 0) cuda.mmq_q4_dense_pipeline = NULL;
                        }
                    }
                }
            }
            if (error == 0 && q4_x64 && q4_routed_shared)
                error = cuda.function_get(&cuda.mmq_q4_routed_x64, cuda.module, "goose_mmq_q4_routed_x64");
            if (error == 0 && cuda.mmq_q4_routed_x64 && routed_x128)
                error = cuda.function_get(&cuda.mmq_q4_routed_x128, cuda.module, "goose_mmq_q4_routed_x128");
            if (error == 0 && cuda.mmq_q4_routed_x128 && fg_standalone_option_enabled("GOOSE_MMQ_Q4_ROUTED_M128"))
                error = cuda.function_get(&cuda.mmq_q4_routed_m128, cuda.module, "goose_mmq_q4_routed_m128");
            if (error == 0 && fg_standalone_option_enabled("GOOSE_MMQ_Q4_ROUTED_DOWN_M128"))
                error = cuda.function_get(&cuda.mmq_q4_routed_down_m128, cuda.module, "goose_mmq_q4_routed_down_m128");
        }
        if (error == 0 && reference_arch && q6_reference) {
            error = cuda.function_get(&cuda.mmq_q6_matrix, cuda.module, "goose_mmq_q6_reference");
            if (error == 0 && q6_shared)
                error = cuda.function_get(&cuda.mmq_q6_dense_shared, cuda.module, "goose_mmq_q6_dense_shared");
            if (error == 0 && q6_shared)
                error = cuda.function_get(&cuda.mmq_q6_routed_shared, cuda.module, "goose_mmq_q6_routed_shared");
            if (error == 0 && q6_shared && q6_x64)
                error = cuda.function_get(&cuda.mmq_q6_dense_x64, cuda.module, "goose_mmq_q6_dense_x64");
            if (error == 0 && q6_shared && q6_x64 && dense_x128)
                error = cuda.function_get(&cuda.mmq_q6_dense_x128, cuda.module, "goose_mmq_q6_dense_x128");
            if (error == 0 && cuda.mmq_q6_dense_x128 && fg_standalone_option_enabled("GOOSE_MMQ_Q6_HALF_K")) {
                enum { max_dynamic_shared_size = 8 };
                error = cuda.function_get(&cuda.mmq_q6_dense_half_k, cuda.module, "goose_mmq_q6_dense_m128_half_k");
                if (error == 0) error = cuda.function_set_attribute(cuda.mmq_q6_dense_half_k,
                    max_dynamic_shared_size, Q6_HALF_K_SHARED_BYTES);
                if (error == 0) error = cuda.function_get(&cuda.mmq_pack_k_major, cuda.module, "goose_mmq_pack_k_major");
                if (error == 0 && fg_standalone_option_enabled("GOOSE_MMQ_Q6_PIPELINE")) {
                    int setup = cuda.function_get(&cuda.mmq_q6_dense_pipeline, cuda.module,
                        "goose_mmq_q6_dense_m128_pipeline");
                    if (setup == 0) setup = cuda.function_set_attribute(cuda.mmq_q6_dense_pipeline,
                        max_dynamic_shared_size, Q6_PIPE_SHARED_BYTES);
                    if (setup != 0) cuda.mmq_q6_dense_pipeline = NULL;
                }
            }
            if (error == 0 && q6_shared && q6_x64)
                error = cuda.function_get(&cuda.mmq_q6_routed_x64, cuda.module, "goose_mmq_q6_routed_x64");
            if (error == 0 && cuda.mmq_q6_routed_x64 && routed_x128)
                error = cuda.function_get(&cuda.mmq_q6_routed_x128, cuda.module, "goose_mmq_q6_routed_x128");
            if (error == 0 && cuda.mmq_q6_routed_x128 && fg_standalone_option_enabled("GOOSE_MMQ_Q6_ROUTED_PIPELINE")) {
                enum { max_dynamic_shared_size = 8 };
                int setup = cuda.function_get(&cuda.mmq_q6_routed_pipeline, cuda.module,
                    "goose_mmq_q6_routed_m128_pipeline");
                if (setup == 0) setup = cuda.function_set_attribute(cuda.mmq_q6_routed_pipeline,
                    max_dynamic_shared_size, Q6_ROUTED_SHARED_BYTES);
                if (setup != 0) cuda.mmq_q6_routed_pipeline = NULL;
            }
        }
        if (error == 0 && stream_k) error = cuda.function_get(&cuda.mmq_stream, cuda.module, "goose_mmq_stream");
        if (error == 0 && stream_k) error = cuda.function_get(&cuda.mmq_stream_fixup, cuda.module, "goose_mmq_stream_fixup");
        if (error == 0 && dense_mma) error = cuda.function_get(&cuda.dense_mma, cuda.module, "goose_dense_mma");
        if (error == 0 && reference_arch && dense_reference) {
            error = cuda.function_get(&cuda.dense_mma_reference, cuda.module, "goose_dense_mma_reference");
        }
        if (error == 0) error = cuda.function_get(&cuda.attention_tiles, cuda.module, "goose_q4_attention_tiles");
        if (error == 0) error = cuda.function_get(&cuda.attention_merge, cuda.module, "goose_q4_attention_merge");
        if (error == 0 && flash_mma) error = cuda.function_get(&cuda.flash_mma, cuda.module, "goose_flash_causal_mma");
        if (error == 0 && flash_mma) error = cuda.function_get(&cuda.flash_stage, cuda.module, "goose_flash_q4_stage");
        if (error == 0 && flash_stream) error = cuda.function_get(&cuda.flash_stream, cuda.module, "goose_flash_stream_mma");
        if (error == 0 && flash_stream) error = cuda.function_get(&cuda.flash_fixup_uniform, cuda.module, "goose_flash_fixup_uniform");
        if (error == 0 && flash_stream) error = cuda.function_get(&cuda.flash_fixup_general, cuda.module, "goose_flash_fixup_general");
        if (error == 0 && flash_vec) error = cuda.function_get(&cuda.flash_vec, cuda.module, "goose_flash_vec_q4");
        if (error == 0 && flash_vec) error = cuda.function_get(&cuda.flash_vec_merge, cuda.module, "goose_flash_vec_combine");
        if (error == 0 && flash_vec && vec_pack)
            error = cuda.function_get(&cuda.flash_vec_pack, cuda.module, "goose_flash_vec_pack_query");
        if (error == 0 && flash_vec && vec_pack) {
            error = cuda.function_get(&cuda.flash_vec_packed, cuda.module, "goose_flash_vec_q4_packed");
            if (error == 0)
                (void) cuda.function_get(&cuda.flash_vec_paired, cuda.module, "goose_flash_vec_q4_packed_pair");
            if (error == 0)
                (void) cuda.function_get(&cuda.flash_vec_quad, cuda.module, "goose_flash_vec_q4_packed_quad");
        }
        if (error == 0 && mmvq && major == 8 &&
            cuda.device_attribute(&minor, compute_capability_minor, device) == 0 && minor == 9) {
            const char *disabled = getenv("GOOSE_CUDA_DISABLE_FUSED_UP_GATE");
            if (!disabled || strcmp(disabled, "1") != 0) {
                Module candidate = NULL;
                if (cuda.module_load(&candidate, fused_up_gate_image) == 0)
                    (void)cuda.function_get(&cuda.mmvq_up_gate_glu, candidate, "goose_mmvq_up_gate_glu");
            }
            const char *disable_shared = getenv("GOOSE_CUDA_DISABLE_FUSED_SHARED");
            if (!disable_shared || strcmp(disable_shared, "1") != 0) {
                Module candidate = NULL;
                if (cuda.module_load(&candidate, fused_shared_image) == 0)
                    (void)cuda.function_get(&cuda.mmvq_shared_up_gate_glu, candidate, "goose_mmvq_shared_up_gate_glu");
            }
        }
        if (error == 0 && cuda.flash_vec_quad && major == 8 &&
            cuda.device_attribute(&minor, compute_capability_minor, device) == 0 && minor == 9) {
            const char *disable_key_bound = getenv("GOOSE_CUDA_DISABLE_ATTENTION_KEY_BOUND");
            if (!disable_key_bound || strcmp(disable_key_bound, "1") != 0) {
                Module candidate = NULL;
                const char *disable_row = getenv("GOOSE_CUDA_DISABLE_ATTENTION_FIRST_ROW");
                const bool first_row = !disable_row || strcmp(disable_row, "1") != 0;
                const char *disable_unroll = getenv("GOOSE_CUDA_DISABLE_ATTENTION_UNROLL");
                const bool unroll = first_row && (!disable_unroll || strcmp(disable_unroll, "1") != 0);
                const void *image = unroll ? attention_unroll_image :
                    first_row ? attention_first_row_image : attention_key_bound_image;
                if (cuda.module_load(&candidate, image) == 0 &&
                    cuda.function_get(&cuda.flash_vec_quad_key_bound, candidate,
                                      "goose_flash_vec_q4_packed_quad") == 0) {
                    fputs("Goose CUDA: SM89 bounded-key Attention available\n", stderr);
                    if (first_row) fputs("Goose CUDA: Attention first-row variant loaded\n", stderr);
                    if (unroll) fputs("Goose CUDA: Attention V unroll selected for SM89\n", stderr);
                }
            }
        }
        if (error == 0) init_cuda_graphs();
        if (error == 0) error = cuda.function_get(&cuda.gdn, cuda.module, "goose_gated_delta_net");
        if (error == 0 && fg_standalone_option_enabled("GOOSE_GDN_DECODE")) {
            error = cuda.function_get(&cuda.gdn_decode, cuda.module, "goose_gdn_decode");
        }
        if (error == 0 && gdn_mma) {
            enum { max_dynamic_shared_size = 8 };
            error = cuda.function_get(&cuda.gdn_mma, cuda.module, "goose_gdn_mma");
            if (error == 0) error = cuda.function_set_attribute(cuda.gdn_mma, max_dynamic_shared_size, GDN_MMA_SHARED_BYTES);
            if (error == 0 && fg_standalone_option_enabled("GOOSE_GDN_MMA_V32")) {
                int setup = cuda.function_get(&cuda.gdn_mma_v32, cuda.module, "goose_gdn_mma_v32");
                if (setup == 0) setup = cuda.function_set_attribute(cuda.gdn_mma_v32, max_dynamic_shared_size, GDN_V32_SHARED_BYTES);
                if (setup != 0) cuda.gdn_mma_v32 = NULL;
            }
        }
        if (error == 0) error = cuda.function_get(&cuda.q4_rows, cuda.module, "goose_set_q4_rows");
        if (error == 0) error = cuda.function_get(&cuda.vector, cuda.module, "goose_vector_ops");
        if (error == 0)
            (void) cuda.function_get(&cuda.moe_add_shared_residual, cuda.module, "goose_moe_add_shared_residual");
        if (error == 0 && fg_standalone_option_enabled("GOOSE_CUDA_VECTOR_FLAT"))
            error = cuda.function_get(&cuda.vector_flat, cuda.module, "goose_vector_flat");
        if (error == 0 && cuda.vector_flat && fg_standalone_option_enabled("GOOSE_CUDA_VECTOR_FLAT4"))
            error = cuda.function_get(&cuda.vector_flat4, cuda.module, "goose_vector_flat4");
        if (error == 0 && rms_reference) error = cuda.function_get(&cuda.rms_reference, cuda.module, "goose_rms_reference");
        if (error == 0 && rms_reference)
            (void) cuda.function_get(&cuda.rms_decode_2048, cuda.module, "goose_rms_decode_2048");
        cuda.rms_postscale = cuda.rms_reference && fg_standalone_option_enabled("GOOSE_CUDA_RMS_POSTSCALE");
        if (error == 0) error = cuda.function_get(&cuda.rope_phases, cuda.module, "goose_rope_phases");
        if (error == 0) error = cuda.function_get(&cuda.rope_apply, cuda.module, "goose_rope_apply");
        if (error == 0 && fg_standalone_option_enabled("GOOSE_ROPE_DECODE")) {
            error = cuda.function_get(&cuda.rope_decode, cuda.module, "goose_rope_decode");
        }
        if (error == 0) error = cuda.function_get(&cuda.ssm_conv, cuda.module, "goose_ssm_conv");
        if (error == 0 && ssm_reference) {
            error = cuda.function_get(&cuda.ssm_conv_long, cuda.module, "goose_ssm_conv_long");
            const char * decode_setting = getenv("GOOSE_SSM_DECODE_REFERENCE");
            if (error == 0 && decode_setting && strcmp(decode_setting, "1") == 0) {
                error = cuda.function_get(&cuda.ssm_conv_decode, cuda.module, "goose_ssm_conv_decode");
            }
        }
        if (error == 0) error = cuda.function_get(&cuda.route_reduce, cuda.module, "goose_route_reduce");
        if (error == 0) error = cuda.function_get(&cuda.route_sort, cuda.module, "goose_route_sort");
        if (error == 0) error = cuda.function_get(&cuda.route_topk, cuda.module, "goose_route_topk");
        if (error == 0 && mmvq && major == 8 && minor == 9) {
            const char *disable = getenv("GOOSE_CUDA_DISABLE_ROUTE_COMPACT8");
            Module routing = NULL;
            if ((!disable || strcmp(disable,"1") != 0) &&
                cuda.module_load(&routing, compact_route_image) == 0)
                (void)cuda.function_get(&cuda.route_compact8, routing, "goose_route_compact8");
        }
        if (error == 0 && mmvq && major == 8 && minor == 9) {
            const char *disable = getenv("GOOSE_CUDA_DISABLE_ROUTER_UNROLL");
            Module module = NULL;
            if ((!disable || strcmp(disable, "1") != 0) &&
                cuda.module_load(&module, router_softmax_image) == 0)
                (void)cuda.function_get(&cuda.route_softmax_fixed, module, "goose_router_softmax_fixed");
        }
        if (error == 0) error = cuda.function_get(&cuda.route_gather, cuda.module, "goose_route_gather");
        if (error == 0) error = cuda.function_get(&cuda.route_norm, cuda.module, "goose_route_norm");
        if (error == 0) error = cuda.function_get(&cuda.blit, cuda.module, "goose_tensor_blit");
        if (error == 0 && fg_standalone_option_enabled("GOOSE_CUDA_DIRECT_SSM")) {
            (void) cuda.function_get(&cuda.ssm_conv_direct, cuda.module, "goose_ssm_conv_direct");
            (void) cuda.function_get(&cuda.ssm_conv_save_tail, cuda.module, "goose_ssm_conv_save_tail");
        }
        if (error == 0 && fg_standalone_option_enabled("GOOSE_CUDA_TILED_BLIT"))
            (void) cuda.function_get(&cuda.blit_transpose, cuda.module, "goose_tensor_transpose_concat");
        if (error == 0) error = cuda.function_get(&cuda.fwht, cuda.module, "goose_kv_fwht");
        if (error == 0 && fg_standalone_option_enabled("GOOSE_CUDA_FWHT_Q4"))
            (void) cuda.function_get(&cuda.fwht_q4, cuda.module, "goose_fwht_q4");
        if (error == 0) error = cuda.function_get(&cuda.moe_weighted, cuda.module, "goose_moe_weighted");
        Context current = NULL;
        int pop_error = cuda.pop(&current);
        if (error == 0) error = pop_error;
    }
    if (error != 0) {
        fprintf(stderr, "Goose CUDA: initialization failed (%d)\n", error);
        return;
    }
    cuda.status = 1;
}

static void init_cuda(void) {
    load_cuda();
    if (cuda.library) atexit(cleanup);
}

static int release_buffer(struct device_buffer * buffer) {
    if (buffer == &cuda.mmq_packed) shared_activation_pack.host = 0;
    if (!buffer->address) return 0;
    if (buffer->reservation_bytes) return release_vmm(buffer);
    const int error = cuda.release(buffer->address);
    if (error == 0) {
        *buffer = (struct device_buffer) {0};
    }
    return error;
}

static int reserve_vmm(struct device_buffer * buffer, size_t bytes, size_t limit, bool clear) {
    enum { out_of_memory = 2, access_read_write = 3 };
    if (cuda.vmm_capacity == 0) {
        const size_t capacity = (size_t) fg143_vmm_capacity();
        if (capacity % cuda.vmm_granularity != 0) return out_of_memory;
        cuda.vmm_capacity = capacity;
    }
    if (buffer->vmm_failed || bytes > limit || limit > cuda.vmm_capacity) return out_of_memory;
    const int64_t growth = fg143_vmm_scratch_growth((int64_t) bytes, (int64_t) buffer->bytes,
        (int64_t) cuda.vmm_granularity);
    if (growth <= 0) return out_of_memory;
    const int submitted = decode_capture_submit();
    if (submitted != 0) return submitted;
    struct vmm_mapping * segment = malloc(sizeof(*segment));
    if (!segment) return out_of_memory;
    *segment = (struct vmm_mapping) {.previous = buffer->mappings, .offset = buffer->bytes, .bytes = (size_t) growth};
    int error = 0;
    if (!buffer->address) {
        DevicePtr base = 0;
        const int64_t reservation = fg143_vmm_scratch_growth((int64_t) limit, 0, (int64_t) cuda.vmm_granularity);
        if (reservation <= 0) { free(segment); return out_of_memory; }
        error = cuda.vmm_reserve(&base, (size_t) reservation, cuda.vmm_granularity, 0, 0);
        if (error == 0) {
            buffer->address = base;
            buffer->reservation_bytes = (size_t) reservation;
        }
    }
    if (error == 0) error = cuda.vmm_create(&segment->handle, segment->bytes, &cuda.vmm_properties, 0);
    if (error != 0) {
        free(segment);
        return error;
    }
    buffer->mappings = segment;
    const DevicePtr address = buffer->address + segment->offset;
    error = cuda.vmm_map(address, segment->bytes, 0, segment->handle, 0);
    if (error == 0) {
        segment->mapped = true;
        const struct memory_access access = {cuda.vmm_properties.location, access_read_write};
        error = cuda.vmm_access(address, segment->bytes, &access, 1);
    }
    if (error == 0 && clear) error = cuda.zero(address, 0, segment->bytes);
    if (error == 0) {
        buffer->bytes += segment->bytes;
        if (getenv("GOOSE_GRAPH_LOG")) {
            fprintf(stderr, "Goose CUDA VMM: mapped base=%#" PRIx64 " offset=%zu bytes=%zu\n",
                (uint64_t) buffer->address, segment->offset, segment->bytes);
        }
    } else if (clear) {
        // The graph must not consume an address whose mapping or zeroing failed.
        buffer->vmm_failed = true;
    }
    return error;
}

static int reserve(struct device_buffer * buffer, size_t bytes) {
    if (bytes <= buffer->bytes) return 0;
    if (cuda.scratch_vmm && (buffer == &cuda.attention_scratch || buffer == &cuda.mmq_packed) &&
        (!buffer->address || buffer->reservation_bytes)) {
        const int error = reserve_vmm(buffer, bytes, (size_t) fg143_vmm_capacity(), false);
        if (error == 0) return 0;
    }
    int error = release_buffer(buffer);
    if (error != 0) return error;
    error = cuda.alloc(&buffer->address, bytes);
    // A completed MMQ projection has no live users of its packing buffers.
    // Reclaim them under pressure before admitting the next operator.
    if (error == 2 && buffer != &cuda.mmq_packed && buffer != &cuda.mmq_tiles &&
        (cuda.mmq_packed.address || cuda.mmq_tiles.address)) {
        error = release_buffer(&cuda.mmq_packed);
        if (error == 0) error = release_buffer(&cuda.mmq_tiles);
        if (error == 0) error = cuda.alloc(&buffer->address, bytes);
    }
    if (error == 0) buffer->bytes = bytes;
    if (error == 2) {
        size_t available = 0, total = 0;
        if (cuda.memory_info(&available, &total) == 0) {
            fprintf(stderr, "Goose CUDA: %zu device bytes requested, %zu bytes available\n", bytes, available);
        }
    }
    return error;
}

static int finish_context(void) {
    Context context = NULL;
    const int status = cuda.pop(&context);
    pthread_mutex_unlock(&cuda_lock);
    return status;
}

static int push_context(void) {
    pthread_mutex_lock(&cuda_lock);
    int status = cuda.push(cuda.context);
    if (status != 0) {
        pthread_mutex_unlock(&cuda_lock);
        fprintf(stderr, "Goose CUDA: context switch failed (%d)\n", status);
        return -1;
    }
    return 1;
}

static int start_context(void) {
    const char * setting = getenv("GOOSE_CUDA");
    if (setting && setting[0] == '0' && !setting[1]) return 0;
    pthread_once(&cuda_once, init_cuda);
    return cuda.status == 1 ? push_context() : cuda.status;
}

static sl_i64 activation_ranges(void) {
    return (sl_i64) {activations.ranges, activations.count * ACTIVATION_WORDS};
}

static sl_i64 activation_large_range(void) {
    return (sl_i64) {activations.large_output, ACTIVATION_WORDS};
}

static int activation_download_record(int64_t * range, DevicePtr base) {
    if (range[1] == 0 || range[3] != 0) return 0;
    const int error = cuda.download((void *) (uintptr_t) range[0],
        base + (uint64_t) range[2], (size_t) range[1]);
    if (error == 0) range[3] = 1;
    return error;
}

static int activation_download_all(void) {
    for (int64_t i = 0; i < activations.count; ++i) {
        const int error = activation_download_record(activations.ranges + ACTIVATION_WORDS * i, cuda.activations.address);
        if (error != 0) return error;
    }
    return 0;
}

static int activation_evict_device_range(int64_t offset, int64_t bytes) {
    for (int64_t i = 0; i < activations.count;) {
        const int64_t found = fg143_activation_device_overlap(activation_ranges(), offset, bytes, i);
        if (found < 0) break;
        int64_t * range = activations.ranges + ACTIVATION_WORDS * found;
        const int error = activation_download_record(range, cuda.activations.address);
        if (error != 0) return error;
        range[1] = 0;
        i = found + 1;
    }
    return 0;
}

static int activation_sync_range(uint64_t host, int64_t bytes, bool invalidate) {
    if (activations.large_output[1] > 0 && fg143_activation_overlap(activation_large_range(), host, bytes, 0) >= 0) {
        const int error = activation_download_record(activations.large_output, cuda.output.address);
        if (error != 0) return error;
        if (invalidate) activations.large_output[1] = 0;
    }
    for (int64_t i = 0; activations.count > 0;) {
        const int64_t found = fg143_activation_overlap(activation_ranges(), host, bytes, i);
        if (found < 0) break;
        int64_t * range = activations.ranges + ACTIVATION_WORDS * found;
        const int error = activation_download_record(range, cuda.activations.address);
        if (error != 0) return error;
        if (invalidate) range[1] = 0;
        i = found + 1;
    }
    return 0;
}

int fg_cuda_activation_graph_begin(bool planned) {
    route_compact_pending.ids_host = 0;
    if (activations.active) return -1;
    const char * setting = getenv("GOOSE_CUDA");
    if (setting && setting[0] == '0' && !setting[1]) return 0;
    pthread_mutex_lock(&activation_lock);
    activations.active = true;
    activations.step = -1;
    shared_activation_pack.host = 0;
    activations.shared_pack_reuses = 0;
    activations.dense_pack_reuses = 0;
    activations.flat_vectors = 0;
    activations.flat4_vectors = 0;
    activations.q4_prepared = 0;
    activations.q4_pipelined = 0;
    activations.q6_pipelined = 0;
    activations.q6_routed_pipelined = 0;
    activations.expert_parallel_batches = 0;
    activations.unavailable = false;
    activations.defer_waits = planned && fg_standalone_option_enabled("GOOSE_CUDA_DEFER_WAITS");
    activations.retire = fg_standalone_option_enabled("GOOSE_ACTIVATION_LIFETIMES");
    activations.reuse = fg_standalone_option_enabled("GOOSE_CUDA_ARENA_REUSE");
    activations.selective_eviction = fg_standalone_option_enabled("GOOSE_CUDA_SELECTIVE_EVICTION");
    activations.pending = false;
    activations.hits = 0;
    activations.skipped_download_bytes = 0;
    activations.expired_bytes = 0;
    activations.packet_used = 0;
    memset(&decode_capture, 0, sizeof decode_capture);
    return 0;
}

int64_t fg_cuda_activation_read_address(uint64_t host, int64_t bytes) {
    bool pending = activations.large_output[1] > 0 && activations.large_output[3] == 0 &&
        fg143_activation_overlap(activation_large_range(), host, bytes, 0) >= 0;
    for (int64_t i = 0; !pending && i < activations.count;) {
        const int64_t found = fg143_activation_overlap(activation_ranges(), host, bytes, i);
        if (found < 0) break;
        pending = activations.ranges[found * ACTIVATION_WORDS + 3] == 0;
        i = found + 1;
    }
    if (!pending) return 0;
    if (push_context() != 1) return -1;
    const int error = activation_sync_range(host, bytes, false);
    return finish_context() == 0 && error == 0 ? 0 : -1;
}

int64_t fg_cuda_activation_read(sl_u8 data) {
    return fg_cuda_activation_read_address((uint64_t) (uintptr_t) data.data, data.len);
}

int64_t fg_cuda_activation_read_f32(sl_f32 data) {
    if (data.len < 0 || data.len > INT64_MAX / (int64_t) sizeof(float)) return -1;
    return fg_cuda_activation_read_address((uint64_t) (uintptr_t) data.data, data.len * (int64_t) sizeof(float));
}

static void activation_prepare_target(uint64_t host, int64_t bytes, bool fresh, int64_t step, int64_t deadline) {
    activations.target = fresh ? host : 0;
    activations.target_bytes = bytes;
    activations.target_deadline = deadline;
    activations.step = activations.retire ? step : -1;
    if (step >= 0 && activations.retire) {
        if (activations.count > 0) activations.expired_bytes += fg143_activation_expire(activation_ranges(), step);
        if (activations.large_output[1] > 0) activations.expired_bytes += fg143_activation_expire(activation_large_range(), step);
    }
}

int fg_cuda_activation_prepare(uint64_t host, int64_t bytes, bool fresh, bool overwrite, int64_t step, int64_t deadline) {
    if (!activations.active) return 0;
    activation_prepare_target(host, bytes, fresh, step, deadline);
    if (overwrite && activations.count > 0) {
        activations.skipped_download_bytes += fg143_activation_discard_overwritten(activation_ranges(), host, bytes);
    }
    if (overwrite && activations.large_output[1] > 0) {
        activations.skipped_download_bytes += fg143_activation_discard_overwritten(activation_large_range(), host, bytes);
    }
    if (fg143_activation_overlap(activation_ranges(), host, bytes, 0) < 0 &&
        (activations.large_output[1] == 0 || fg143_activation_overlap(activation_large_range(), host, bytes, 0) < 0)) return 0;
    if (push_context() != 1) return -1;
    const int error = activation_sync_range(host, bytes, true);
    return finish_context() == 0 && error == 0 ? 0 : -1;
}

int fg_cuda_activation_graph_end(bool success, bool all, uint64_t host, int64_t bytes, uint64_t * hits,
                                 uint64_t * skipped_download_bytes, uint64_t * expired_bytes) {
    *hits = activations.hits;
    activations.hits = 0;
    *skipped_download_bytes = activations.skipped_download_bytes;
    activations.skipped_download_bytes = 0;
    *expired_bytes = activations.expired_bytes;
    activations.expired_bytes = 0;
    if (!activations.active) return 0;
    if (activations.shared_pack_reuses && getenv("GOOSE_MMQ_SHARED_REUSE_REPORT")) {
        fprintf(stderr, "Goose CUDA MMQ: reused %" PRIu64 " shared Q8_1 packs\n", activations.shared_pack_reuses);
    }
    if (activations.dense_pack_reuses && getenv("GOOSE_MMQ_SHARED_REUSE_REPORT")) {
        fprintf(stderr, "Goose CUDA MMQ: reused %" PRIu64 " dense Q8_1 packs\n", activations.dense_pack_reuses);
    }
    if (activations.q4_prepared && getenv("GOOSE_MMQ_SHARED_REUSE_REPORT")) {
        fprintf(stderr, "Goose CUDA MMQ: prepared %" PRIu64 " dense Q4_K weights\n", activations.q4_prepared);
    }
    if (activations.q4_pipelined && getenv("GOOSE_MMQ_SHARED_REUSE_REPORT")) {
        fprintf(stderr, "Goose CUDA MMQ: pipelined %" PRIu64 " dense Q4_K launches\n", activations.q4_pipelined);
    }
    if (activations.q6_pipelined && getenv("GOOSE_MMQ_SHARED_REUSE_REPORT")) {
        fprintf(stderr, "Goose CUDA MMQ: pipelined %" PRIu64 " dense Q6_K launches\n", activations.q6_pipelined);
    }
    if (activations.q6_routed_pipelined && getenv("GOOSE_MMQ_SHARED_REUSE_REPORT")) {
        fprintf(stderr, "Goose CUDA MMQ: pipelined %" PRIu64 " routed Q6_K launches\n", activations.q6_routed_pipelined);
    }
    if (activations.expert_parallel_batches && getenv("GOOSE_MMQ_SHARED_REUSE_REPORT")) {
        fprintf(stderr, "Goose CUDA: prepared %" PRIu64 " expert batches in parallel\n", activations.expert_parallel_batches);
    }
    if (activations.flat_vectors && getenv("GOOSE_CUDA_VECTOR_FLAT_REPORT")) {
        fprintf(stderr, "Goose CUDA vector: %" PRIu64 " flat launches, %" PRIu64 " using float4\n",
            activations.flat_vectors, activations.flat4_vectors);
    }
    int error = 0;
    decode_capture.scope = false;
    if (activations.pending || decode_capture.capturing) {
        if (push_context() != 1) error = -1;
        else {
            error = !success && decode_capture.capturing ? decode_capture_discard() : cuda.sync();
            if (finish_context() != 0) error = -1;
            if (error != 0) fprintf(stderr, "Goose CUDA: graph completion failed (%d)\n", error);
        }
        if (error != 0 && activations.packet_used != 0) cuda.async_packets = false;
    }
    if (error == 0 && success && (activations.count > 0 || activations.large_output[1] > 0)) {
        if (all) {
            if (push_context() != 1) error = -1;
            else {
                error = activation_download_all();
                if (error == 0) error = activation_download_record(activations.large_output, cuda.output.address);
                if (finish_context() != 0) error = -1;
            }
        } else {
            error = (int) fg_cuda_activation_read_address(host, bytes);
        }
    }
    activations.count = 0;
    activations.used = 0;
    activations.large_output[1] = 0;
    activations.target = 0;
    activations.pending = false;
    activations.active = false;
    pthread_mutex_unlock(&activation_lock);
    if (decode_capture.replays && getenv("GOOSE_GRAPH_LOG")) {
        fprintf(stderr, "Goose CUDA decode regions: %zu replays, %zu updates, %zu kernels max\n",
            decode_capture.replays, decode_capture.updates, decode_capture.maximum);
        if (cuda.decode_reuse)
            fprintf(stderr, "Goose CUDA decode reuse: %zu regions, %zu changed kernel nodes\n",
                decode_capture.reused, decode_capture.changed_nodes);
    }
    return error;
}

static int activation_reserve_output(size_t bytes) {
    if (activations.active && activations.defer_waits) activations.pending = true;
    const int error = activation_download_record(activations.large_output, cuda.output.address);
    if (error != 0) return error;
    activations.large_output[1] = 0;
    return reserve(&cuda.output, bytes);
}

static int activation_widen(void) {
    cuda.adapt_activation_capacity = false;
    struct device_buffer larger = {0};
    int error = reserve(&larger, WIDE_ACTIVATION_BYTES);
    if (error == 2) return 0;
    if (error != 0) return error;
    error = activation_download_all();
    if (error == 0) error = release_buffer(&cuda.activations);
    if (error != 0) {
        release_buffer(&larger);
        return error;
    }
    activations.count = 0;
    activations.used = 0;
    cuda.activations = larger;
    cuda.activation_capacity = WIDE_ACTIVATION_BYTES;
    return 0;
}

static int stage_last_matrix_input(sl_u8 input, int64_t window, int64_t bytes, DevicePtr * staged) {
    const uint64_t address = (uint64_t) (uintptr_t) input.data;
    const int64_t index = fg143_activation_last_input(activation_ranges(), address, input.len,
        activations.step, window, bytes);
    if (index < 0) return 0;
    int64_t * range = activations.ranges + ACTIVATION_WORDS * index;
    int error = reserve(&cuda.input, (size_t) input.len);
    if (error == 2) return 0;
    if (error == 0) error = cuda.copy_device(cuda.input.address,
        cuda.activations.address + (uint64_t) range[2], (size_t) input.len);
    if (error != 0) return error;
    range[1] = 0;
    *staged = cuda.input.address;
    ++activations.hits;
    return 0;
}

// Called with the CUDA context bound. Reserve output before resolving input
// addresses so a flush never invalidates a pointer passed to a launch.
static int activation_output_for_input(uint64_t host, int64_t bytes, DevicePtr * device,
                                       sl_u8 input, DevicePtr * staged) {
    // All kernels and synchronous copies use the same legacy default stream.
    // Check its completion at graph exit even if a later operation fails.
    if (activations.active && activations.defer_waits) activations.pending = true;
    const bool eligible = activations.active && !activations.unavailable && host != 0 &&
        host == activations.target && bytes > 0 && bytes <= activations.target_bytes;
    if (eligible && cuda.adapt_activation_capacity &&
        bytes > DEFAULT_ACTIVATION_BYTES && bytes <= WIDE_ACTIVATION_BYTES) {
        const int error = activation_widen();
        if (error != 0) return error;
    }
    if (eligible && (uint64_t) bytes <= cuda.activation_capacity) {
        if (!cuda.activations.address) {
            const int error = reserve(&cuda.activations, cuda.activation_capacity);
            if (error == 2 && cuda.activation_capacity > DEFAULT_ACTIVATION_BYTES) {
                fprintf(stderr, "Goose CUDA: cannot allocate the requested %zu MiB activation arena\n",
                    cuda.activation_capacity / MIB_BYTES);
                return error;
            }
            if (error == 2) activations.unavailable = true; // CUDA_ERROR_OUT_OF_MEMORY
            else if (error != 0) return error;
        }
        if (cuda.activations.address && (uint64_t) bytes <= cuda.activations.bytes) {
            const int64_t capacity = (int64_t) cuda.activations.bytes;
            int64_t pos;
            if (activations.reuse) {
                pos = fg143_activation_slot(activation_ranges(), capacity, bytes);
            } else {
                const int64_t aligned = (activations.used + 127) & ~INT64_C(127);
                pos = aligned <= capacity - bytes ? aligned : -2;
            }
            if (pos == -2 && activations.reuse && activations.selective_eviction) {
                pos = fg143_activation_eviction_slot(activation_ranges(), capacity, bytes);
                if (pos < 0) return -1;
                if (staged) {
                    const int error = stage_last_matrix_input(input, pos, bytes, staged);
                    if (error != 0) return error;
                }
                const int error = activation_evict_device_range(pos, bytes);
                if (error != 0) return error;
            }
            if (pos == -2 || (activations.count == ACTIVATION_RECORDS &&
                (!activations.reuse || fg143_activation_record_slot(activation_ranges()) == ACTIVATION_RECORDS))) {
                const int error = activation_download_all();
                if (error != 0) return error;
                activations.count = 0;
                activations.used = 0;
                pos = 0;
            }
            if (pos >= 0) {
                *device = cuda.activations.address + (uint64_t) pos;
                return 0;
            }
        }
    }
    const int error = activation_reserve_output((size_t) bytes);
    if (error == 0) *device = cuda.output.address;
    return error;
}

static int activation_output(uint64_t host, int64_t bytes, DevicePtr * device) {
    return activation_output_for_input(host, bytes, device, (sl_u8) {0}, NULL);
}

static int activation_publish(uint64_t host, int64_t bytes, DevicePtr device, bool complete) {
    const bool large = device == cuda.output.address;
    // The fallback allocation is already sized for this result. Let readers
    // use it directly until the next writer needs the allocation.
    const bool keep_large = large && bytes > DEFAULT_ACTIVATION_BYTES &&
        host == activations.target && bytes == activations.target_bytes;
    if (!activations.active || (large && !keep_large)) {
        return cuda.download((void *) (uintptr_t) host, device, (size_t) bytes);
    }
    const int error = complete || activations.defer_waits ? 0 : cuda.sync();
    if (error != 0) return error;
    int64_t * next = activations.large_output;
    if (!large) {
        const int64_t slot = activations.reuse ? fg143_activation_record_slot(activation_ranges()) : activations.count;
        if (slot >= ACTIVATION_RECORDS) return -1;
        if (slot == activations.count) ++activations.count;
        next = activations.ranges + ACTIVATION_WORDS * slot;
    }
    next[0] = (int64_t) host; next[1] = bytes;
    next[2] = large ? 0 : (int64_t) (device - cuda.activations.address); next[3] = 0;
    next[4] = host == activations.target ? activations.target_deadline : -1;
    if (!large && !activations.reuse) activations.used = next[2] + bytes;
    return 0;
}

static DevicePtr activation_device(uint64_t host, int64_t bytes) {
    if (activations.large_output[1] > 0) {
        const int64_t offset = fg143_activation_lookup(activation_large_range(), host, bytes);
        if (offset >= 0) return cuda.output.address + (uint64_t) offset;
    }
    if (activations.count == 0) return 0;
    const int64_t offset = fg143_activation_lookup(activation_ranges(), host, bytes);
    return offset >= 0 ? cuda.activations.address + (uint64_t) offset : 0;
}

bool fg_cuda_wide_vector_ready(uint64_t x, uint64_t y, uint64_t output, int64_t bytes) {
    if (!cuda.direct_wide_vectors || !activations.active || !activations.reuse ||
        activations.unavailable || !cuda.activations.address || output != activations.target ||
        bytes <= 0 || bytes > activations.target_bytes || (uint64_t) bytes > cuda.activations.bytes ||
        !activation_device(x, bytes) || !activation_device(y, bytes)) return false;
    return fg143_activation_slot(activation_ranges(), (int64_t) cuda.activations.bytes, bytes) >= 0 &&
        fg143_activation_record_slot(activation_ranges()) < ACTIVATION_RECORDS;
}

enum { JOBS_RING = 4 };
static struct {
    void * host;
    size_t bytes;
    Event done;
    int busy;
} jobs_ring[JOBS_RING];
static unsigned jobs_ring_next;
static int jobs_ring_state;

static int upload_jobs_async(DevicePtr device, const void * data, size_t bytes) {
    const char * disable = getenv("GOOSE_JOBS_ASYNC");
    if (disable && strcmp(disable, "0") == 0) return -2;
    if (jobs_ring_state == 2) return -2;
    if (jobs_ring_state == 0) {
        if (!cuda.host_alloc || !cuda.upload_async || !cuda.event_create ||
            !cuda.event_record || !cuda.event_sync) { jobs_ring_state = 2; return -2; }
        for (unsigned i = 0; i < JOBS_RING; ++i)
            if (cuda.event_create(&jobs_ring[i].done, 0) != 0) { jobs_ring_state = 2; return -2; }
        jobs_ring_state = 1;
    }
    const unsigned slot = jobs_ring_next;
    if (jobs_ring[slot].bytes < bytes) {
        if (jobs_ring[slot].busy) { if (cuda.event_sync(jobs_ring[slot].done) != 0) return -1; jobs_ring[slot].busy = 0; }
        if (jobs_ring[slot].host && cuda.host_free) cuda.host_free(jobs_ring[slot].host);
        void * host = NULL;
        const size_t want = (bytes + MIB_BYTES - 1) / MIB_BYTES * MIB_BYTES;
        if (cuda.host_alloc(&host, want, 0) != 0 || !host) { jobs_ring[slot].host = NULL; jobs_ring[slot].bytes = 0; return -1; }
        jobs_ring[slot].host = host;
        jobs_ring[slot].bytes = want;
    }
    if (jobs_ring[slot].busy) {
        if (cuda.event_sync(jobs_ring[slot].done) != 0) return -1;
        jobs_ring[slot].busy = 0;
    }
    memcpy(jobs_ring[slot].host, data, bytes);
    if (cuda.upload_async(device, jobs_ring[slot].host, bytes, NULL) != 0) return -1;
    if (cuda.event_record(jobs_ring[slot].done, NULL) != 0) return -1;
    jobs_ring[slot].busy = 1;
    jobs_ring_next = (slot + 1) % JOBS_RING;
    return 0;
}

static int upload_packet(DevicePtr device, const void * data, size_t bytes) {
    if (!activations.active || !activations.defer_waits || !cuda.async_packets || bytes > 4096) {
        return cuda.upload(device, data, bytes);
    }
    if (!cuda.packet_host) {
        void * host = NULL;
        if (cuda.host_alloc(&host, MIB_BYTES, 0) != 0) {
            cuda.async_packets = false;
            return cuda.upload(device, data, bytes);
        }
        cuda.packet_host = host;
    }
    const void * host = snapshot_packet(data, bytes);
    return host ? cuda.upload_async(device, host, bytes, NULL) : cuda.upload(device, data, bytes);
}

static int activation_input(sl_u8 data, struct device_buffer * staging, DevicePtr * device) {
    if (activations.count > 0 || activations.large_output[1] > 0) {
        const uint64_t host = (uint64_t) (uintptr_t) data.data;
        const DevicePtr current = activation_device(host, data.len);
        if (current) {
            *device = current;
            ++activations.hits;
            return 0;
        }
        const int error = activation_sync_range(host, data.len, false);
        if (error != 0) return error;
    }
    int error = reserve(staging, (size_t) data.len);
    if (error == 0) error = upload_packet(staging->address, data.data, (size_t) data.len);
    if (error == 0) *device = staging->address;
    return error;
}

static int upload_inputs(const sl_u8 * data, struct device_buffer *const * buffers, DevicePtr * addresses, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (data[i].len == 0) continue;
        const int error = activation_input(data[i], buffers[i], &addresses[i]);
        if (error != 0) return error;
    }
    return 0;
}

int64_t fg_cuda_free_bytes(void) {
    const int active = start_context();
    if (active != 1) return active;
    size_t bytes = 0, total = 0;
    const int status = cuda.memory_info(&bytes, &total);
    return finish_context() == 0 && status == 0 && bytes <= INT64_MAX ? (int64_t) bytes : -1;
}

static int prepare_lazy_state(const int64_t * ranges, size_t count, size_t logical,
    struct device_buffer * eager, struct device_buffer ** views) {
    size_t recurrent = 0, total = 0;
    for (size_t i = 0; i < count; ++i) {
        const int64_t * range = ranges + 4 * i;
        if (range[1] <= 0 || (uint64_t) range[1] > logical - total || range[3] < 0 || range[3] > 2) return -1;
        total += (size_t) range[1];
        if (range[3] != 0) {
            if (range[1] % 16 != 0) return -1;
            recurrent += (size_t) range[1];
        }
    }
    if (total != logical || total == recurrent) return -1;
    struct device_buffer * buffers = calloc(count, sizeof(*buffers));
    if (!buffers) return 2;
    int error = recurrent ? reserve(eager, recurrent) : 0;
    if (error == 0 && recurrent) error = cuda.zero(eager->address, 0, recurrent);
    if (error != 0) {
        free(buffers);
        return error;
    }
    size_t offset = 0;
    for (size_t i = 0; i < count; ++i) {
        const int64_t * range = ranges + 4 * i;
        if (range[3] == 0) continue;
        buffers[i].address = eager->address + offset;
        buffers[i].bytes = (size_t) range[1];
        offset += buffers[i].bytes;
    }
    *views = buffers;
    return 0;
}

int64_t fg_cuda_load_model_state(sl_i64 ranges, int64_t bytes) {
    if (!ranges.data || ranges.len <= 0 || ranges.len % 4 != 0 || bytes <= 0 ||
        (uint64_t) ranges.len > SIZE_MAX / sizeof(*ranges.data)) return -1;
    const size_t metadata_bytes = (size_t) ranges.len * sizeof(*ranges.data);
    int64_t * records = malloc(metadata_bytes);
    if (!records) return -1;
    memcpy(records, ranges.data, metadata_bytes);
    if (start_context() != 1) { free(records); return -1; }
    struct device_buffer staging = {0};
    struct device_buffer * views = NULL;
    int error = cuda.state_logical_bytes ? -1 : 0;
    if (error == 0 && cuda.lazy_kv) {
        error = prepare_lazy_state(records, (size_t) ranges.len / 4, (size_t) bytes, &staging, &views);
        if (error != 0) error = release_buffer(&staging);
    }
    if (error == 0 && !views) {
        error = reserve(&staging, (size_t) bytes);
        if (error == 0) error = cuda.zero(staging.address, 0, staging.bytes);
    }
    if (error == 0) {
        cuda.state = staging;
        cuda.state_ranges = (sl_i64) {records, ranges.len};
        cuda.state_views = views;
        cuda.state_logical_bytes = (size_t) bytes;
    } else {
        if (staging.address) cuda.release(staging.address);
        free(records);
    }
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: model state allocation failed (%d)\n", error ? error : popped);
        return -1;
    }
    return bytes;
}

int64_t fg_cuda_model_state_bytes(void) {
    return (int64_t) cuda.state_logical_bytes;
}

int64_t fg_cuda_model_state_startup_bytes(void) {
    return (int64_t) cuda.state.bytes;
}

static struct device_buffer * state_region(uint64_t host, int64_t bytes, int64_t kind, size_t * at) {
    if (!cuda.state_logical_bytes) return NULL;
    if (cuda.state_views) {
        const int64_t index = qr_state_index(cuda.state_ranges, host, bytes, kind);
        if (index < 0) return NULL;
        *at = (size_t) (host - (uint64_t) cuda.state_ranges.data[index]);
        return &cuda.state_views[index / 4];
    }
    const int64_t offset = qr_state_offset(cuda.state_ranges, host, bytes, kind);
    if (offset < 0 || (uint64_t) offset > cuda.state.bytes ||
        (uint64_t) bytes > cuda.state.bytes - (uint64_t) offset) return NULL;
    *at = (size_t) offset;
    return &cuda.state;
}

static DevicePtr state_address(uint64_t host, int64_t bytes, int64_t kind) {
    size_t offset;
    struct device_buffer * buffer = state_region(host, bytes, kind, &offset);
    return buffer ? buffer->address + offset : 0;
}

// Bound, map and zero a lazy state span while the CUDA context is held.
static int map_state(struct device_buffer * buffer, size_t offset, size_t bytes, DevicePtr * address) {
    *address = 0;
    if (!buffer) return 0;
    if (buffer->vmm_failed) return -1;
    if (offset + bytes > buffer->bytes) {
        const int64_t * range = cuda.state_ranges.data + 4 * (buffer - cuda.state_views);
        const int error = reserve_vmm(buffer, offset + bytes, (size_t) range[1], true);
        if (error != 0) return error;
    }
    *address = buffer->address + offset;
    return 0;
}

static int map_state_address(uint64_t host, int64_t bytes, int64_t kind, DevicePtr * address) {
    size_t offset = 0;
    struct device_buffer * buffer = state_region(host, bytes, kind, &offset);
    return map_state(buffer, offset, (size_t) bytes, address);
}

int64_t fg_cuda_reset_model_state(void) {
    published_state.bytes = 0;
    if (!cuda.state_logical_bytes) return 0;
    if (push_context() != 1) return -1;
    int status = cuda.state.address ? cuda.zero(cuda.state.address, 0, cuda.state.bytes) : 0;
    for (int64_t i = 0; status == 0 && cuda.state_views && i < cuda.state_ranges.len / 4; ++i) {
        const struct device_buffer * buffer = &cuda.state_views[i];
        if (buffer->vmm_failed) status = -1;
        else if (buffer->reservation_bytes && buffer->bytes) status = cuda.zero(buffer->address, 0, buffer->bytes);
    }
    return finish_context() == 0 && status == 0 ? 0 : -1;
}

int64_t fg_cuda_state_write(uint64_t host, int64_t bytes) {
    size_t offset = 0;
    struct device_buffer * buffer = state_region(host, bytes, -1, &offset);
    if (!buffer) return 0;
    if (push_context() != 1) return -1;
    DevicePtr target;
    int status = map_state(buffer, offset, (size_t) bytes, &target);
    if (status == 0) status = cuda.upload(target, (const void *) (uintptr_t) host, (size_t) bytes);
    return finish_context() == 0 && status == 0 ? 0 : -1;
}

static int64_t read_state(uint64_t host, int64_t bytes, int64_t kind) {
    size_t offset = 0;
    struct device_buffer * buffer = state_region(host, bytes, kind, &offset);
    if (!buffer) return 0;
    if (push_context() != 1) return -1;
    DevicePtr source;
    int status = map_state(buffer, offset, (size_t) bytes, &source);
    if (status == 0) status = cuda.download((void *) (uintptr_t) host, source, (size_t) bytes);
    return finish_context() == 0 && status == 0 ? 0 : -1;
}

int64_t fg_cuda_state_read(uint64_t host, int64_t bytes) {
    return read_state(host, bytes, -1);
}

int64_t fg_cuda_conv_read(uint64_t host, int64_t bytes) {
    return read_state(host, bytes, 2);
}

int64_t fg_cuda_gdn_state_read(sl_f32 state) {
    if (state.len < 0 || state.len > INT64_MAX / (int64_t) sizeof(float)) return -1;
    return fg_cuda_state_read((uint64_t) (uintptr_t) state.data, state.len * (int64_t) sizeof(float));
}

int64_t fg_cuda_kv_read(sl_u8 k, sl_u8 v) {
    size_t key_offset = 0, value_offset = 0;
    struct device_buffer * key_buffer = state_region((uint64_t) (uintptr_t) k.data, k.len, 0, &key_offset);
    struct device_buffer * value_buffer = state_region((uint64_t) (uintptr_t) v.data, v.len, 0, &value_offset);
    if (!key_buffer && !value_buffer) return 0;
    if (push_context() != 1) return -1;
    DevicePtr key = 0, value = 0;
    int status = map_state(key_buffer, key_offset, (size_t) k.len, &key);
    if (status == 0) status = map_state(value_buffer, value_offset, (size_t) v.len, &value);
    if (status == 0 && key) status = cuda.download(k.data, key, (size_t) k.len);
    if (status == 0 && value) status = cuda.download(v.data, value, (size_t) v.len);
    return finish_context() == 0 && status == 0 ? 0 : -1;
}

static int64_t set_q4_rows(uint64_t source, int64_t source_bytes, int64_t stride,
                           uint64_t target, int64_t target_bytes, int64_t width, int64_t rows, int32_t rotation, float scale) {
    if (!source || source % sizeof(float) || width <= 0 || width > INT_MAX || width % 32 ||
        stride < 4 * width || stride % sizeof(float) || source_bytes < 4 * width ||
        rows <= 0 || rows > INT_MAX ||
        (rows > 1 && stride > (source_bytes - 4 * width) / (rows - 1))) return -1;
    uint32_t blocks = (uint32_t) (width / 32);
    uint64_t tasks = (uint64_t) rows * blocks;
    if (tasks > (uint64_t) INT_MAX * 128 || target_bytes != (int64_t) (18 * tasks)) return -1;
    size_t offset = 0;
    struct device_buffer * buffer = state_region(target, target_bytes, 0, &offset);
    if (!buffer) return 0;
    const int active = start_context();
    if (active != 1) return active;

    DevicePtr input = 0, destination = 0;
    int error = map_state(buffer, offset, (size_t) target_bytes, &destination);
    if (error == 0) error = activation_input((sl_u8) {(void *) (uintptr_t) source, source_bytes}, &cuda.input, &input);
    uint64_t row_stride = (uint64_t) stride;
    void * args[] = {&input, &destination, &row_stride, &blocks, &tasks};
    if (error == 0) {
        if (rotation > 0) {
            int64_t groups = rows * (width / rotation);
            void * fused[] = {&input, &destination, &rotation, &groups, &scale};
            error = cuda.launch(cuda.fwht_q4, (unsigned) ((groups + 3) / 4), 1, 1, 128, 1, 1, 0, NULL, fused, NULL);
        } else {
            error = cuda.launch(cuda.q4_rows, (unsigned) ((tasks + 127) / 128), 1, 1, 128, 1, 1, 0, NULL, args, NULL);
        }
        if (error == 0) error = finish_device_state_write();
    }
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: Q4 cache update failed (%d)\n", error ? error : popped);
        return -1;
    }
    ++cuda_q4_writes;
    return 1;
}

int64_t fg_cuda_set_q4_rows(uint64_t source, int64_t source_bytes, int64_t stride,
                            uint64_t target, int64_t target_bytes, int64_t width, int64_t rows) {
    return set_q4_rows(source, source_bytes, stride, target, target_bytes, width, rows, 0, 0.0f);
}

bool fg_cuda_fwht_q4_enabled(void) { return cuda.fwht_q4 && activations.active; }

int64_t fg_cuda_fwht_q4_rows(uint64_t input, uint64_t target, int64_t width, int64_t rows, int32_t rotation, float scale) {
    if (!fg_cuda_fwht_q4_enabled() || (rotation != 64 && rotation != 256) ||
        width <= 0 || width > 65536 || rows <= 0 || rows > 4096 || width % rotation) return 0;
    const int64_t result = set_q4_rows(input, 4 * width * rows, 4 * width, target,
        18 * (width / 32) * rows, width, rows, rotation, scale);
    if (result > 0 && getenv("GOOSE_GRAPH_LOG")) fprintf(stderr, "Goose CUDA: fused FWHT and Q4 cache write\n");
    return result;
}

uint8_t fg_cuda_match_published_state(uint64_t dst, uint64_t src, int64_t bytes) {
    if (published_state.bytes <= 0 || published_state.bytes != bytes ||
        published_state.source != src || published_state.destination != dst) return 0;
    published_state.bytes = 0;
    return 1;
}

int fg_cuda_state_graph_done(void) {
    if (published_state.bytes == 0) return 0;
    fprintf(stderr, "Goose CUDA: GDN state copy was not consumed by the graph\n");
    return -1;
}

int64_t fg_cuda_load_dense(sl_i64 ranges, int64_t bytes) {
    if (!ranges.data || ranges.len <= 0 || ranges.len % 3 != 0 || bytes <= 0 ||
        (uint64_t) ranges.len > SIZE_MAX / sizeof(*ranges.data)) return -1;
    const size_t metadata_bytes = (size_t) ranges.len * sizeof(*ranges.data);
    int64_t * records = malloc(metadata_bytes);
    if (!records) return -1;
    memcpy(records, ranges.data, metadata_bytes);
    if (start_context() != 1) { free(records); return -1; }
    if (cuda.dense.address) {
        finish_context();
        free(records);
        return -1;
    }
    struct device_buffer staging = {0};
    int status = reserve(&staging, (size_t) bytes);
    for (int64_t i = 0; status == 0 && i < ranges.len; i += 3) {
        const int64_t span = records[i + 1], at = records[i + 2];
        if (records[i] == 0 || span <= 0 || at < 0 || at > bytes || span > bytes - at) {
            status = -1;
            break;
        }
        status = cuda.upload(staging.address + (uint64_t) at,
            (const void *) (uintptr_t) records[i], (size_t) span);
    }
    if (status == 0) {
        cuda.dense = staging;
        cuda.dense_ranges = (sl_i64) { records, ranges.len };
    } else {
        if (staging.address) cuda.release(staging.address);
        free(records);
    }
    const int popped = finish_context();
    if (status != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: dense weight upload failed (%d)\n", status ? status : popped);
        return -1;
    }
    return bytes;
}

int64_t fg_cuda_dense_bytes(void) {
    return (int64_t) cuda.dense.bytes;
}

enum { EXPERT_LAYOUT_GROUP = 9, EXPERT_LAYOUT_LAYER = 4, TEMP_PREFILL_MIN_EXPERTS = 64 };

static bool expert_prefill_layout(struct expert_cache * cache, sl_i64 groups, size_t bank_bytes) {
    cache->bank_bytes = bank_bytes;
    if (!fg_standalone_option_enabled("GOOSE_CUDA_PREFILL_TEMP_WEIGHTS")) return true;
    cache->prefill_wait_decode = fg_standalone_option_enabled("GOOSE_CUDA_PREFILL_TEMP_WARM_ONLY");
    cache->prefill_double = fg_standalone_option_enabled("GOOSE_CUDA_PREFILL_TEMP_DOUBLE");
    cache->prefill_compact = cache->prefill_double && fg_standalone_option_enabled("GOOSE_CUDA_PREFILL_TEMP_COMPACT");
    if (groups.len % EXPERT_LAYOUT_GROUP != 0 || bank_bytes > SIZE_MAX - 255) return false;
    for (int64_t i = 0; i < groups.len; i += EXPERT_LAYOUT_GROUP) {
        const int64_t * group = groups.data + i;
        if (group[5] <= 0 || group[3] < TEMP_PREFILL_MIN_EXPERTS) continue;
        for (int p = 0; p < 3; ++p) {
            if (group[p] <= 0 || (uint64_t) group[p] > SIZE_MAX / (uint64_t) group[3]) return false;
            const size_t bytes = (size_t) group[p] * (size_t) group[3];
            if (bytes > cache->prefill_bytes) cache->prefill_bytes = bytes;
        }
    }
    if (cache->prefill_bytes == 0) return true;
    cache->prefill_offset = (bank_bytes + 255) & ~(size_t) 255;
    const size_t planes = cache->prefill_double && !cache->prefill_compact ? 2 : 1;
    if (cache->prefill_offset > INT64_MAX ||
        cache->prefill_bytes > (INT64_MAX - cache->prefill_offset) / planes) return false;
    cache->prefill_workspace = cache->prefill_bytes * planes;
    const char * budget = getenv("GOOSE_CUDA_PREFILL_TEMP_MIB");
    if (cache->prefill_compact && budget) {
        char * end;
        const intmax_t mib = strtoimax(budget, &end, 10);
        if (end == budget || *end || mib <= 0 || mib > (INT64_MAX - cache->prefill_offset) / MIB_BYTES)
            return false;
        const size_t requested = (size_t) mib * MIB_BYTES;
        if (requested < cache->prefill_bytes) return false;
        cache->prefill_workspace = requested;
    }
    return true;
}

static bool rebalance_apex_cache(sl_i64 groups, int64_t bytes, int64_t layer_words) {
    enum { group_words = 9, q6_slots = 1904, q4_slots = 1531 };
    const char *disabled = getenv("GOOSE_CUDA_DISABLE_CACHE_BALANCE");
    if (disabled && strcmp(disabled, "1") == 0) return false;
    if (groups.len != 18 || layer_words != 160 || bytes != INT64_C(6592856064)) return false;
    int64_t *q6 = groups.data, *q4 = groups.data + group_words;
    if (q6[0] != 589824 || q6[1] != 589824 || q6[2] != 860160 ||
        q4[0] != 589824 || q4[1] != 589824 || q4[2] != 589824 ||
        q6[3] != 256 || q4[3] != 256 || q6[4] != 20 || q4[4] != 20 ||
        q6[5] != 1616 || q4[5] != 1863 || q6[6] != 0) return false;
    const int64_t q6_span = q6[0] + q6[1] + q6[2];
    const int64_t q4_span = q4[0] + q4[1] + q4[2];
    const int64_t q6_bytes = q6_slots * q6_span;
    if (q6_bytes + q4_slots * q4_span > bytes) return false;
    q4[7] += 4 * (q6_slots - q6[5]);
    q6[5] = q6_slots;
    q4[5] = q4_slots;
    q4[6] = q6_bytes;
    return true;
}

int64_t fg_cuda_load_experts(sl_i64 groups, sl_i64 layers, int64_t bytes, int64_t words32, int64_t words64) {
    if (!groups.data || !layers.data || groups.len <= 0 || layers.len <= 0 ||
        (uint64_t) groups.len > SIZE_MAX / sizeof(*groups.data) ||
        (uint64_t) layers.len > SIZE_MAX / sizeof(*layers.data) ||
        bytes <= 0 || words32 <= 0 || words64 <= 0 ||
        (uint64_t) words32 > SIZE_MAX / sizeof(int32_t) ||
        (uint64_t) words64 > SIZE_MAX / sizeof(int64_t)) return -1;
    struct expert_cache cache = {
        .groups = { malloc((size_t) groups.len * sizeof(*groups.data)), groups.len },
        .layers = { malloc((size_t) layers.len * sizeof(*layers.data)), layers.len },
        .work = { calloc((size_t) words64, sizeof(int64_t)), words64 },
        .state = calloc((size_t) words32, sizeof(int32_t)),
    };
    if (!cache.groups.data || !cache.layers.data || !cache.work.data ||
        !cache.state || !expert_prefill_layout(&cache, groups, (size_t) bytes) ||
        start_context() != 1) {
        free_expert_metadata(&cache);
        return -1;
    }
    const size_t total_bytes = cache.prefill_bytes ?
        cache.prefill_offset + cache.prefill_workspace : cache.bank_bytes;
    size_t available = 0, total = 0;
    int status = cuda.experts.weights.address ? -1 : 0;
    if (status == 0 && cache.prefill_bytes &&
        (cuda.memory_info(&available, &total) != 0 || total_bytes > available)) status = -1;
    if (status == 0 && cache.prefill_bytes && cache.prefill_double) {
        enum { disable_timing = 2 };
        status = cuda.expert_stream ? cuda.event_create(&cache.prefill_release, disable_timing) : -1;
        if (status == 0) status = cuda.event_create(&cache.prefill_ready, disable_timing);
    }
    if (status == 0) status = reserve(&cache.weights, total_bytes);
    if (status == 0 && cache.prefill_bytes) {
        fprintf(stderr, "Goose CUDA: %zu byte expert bank, %zu byte temporary prefill workspace, %u buffers\n",
            cache.bank_bytes, total_bytes - cache.prefill_offset, cache.prefill_double ? 2u : 1u);
    }
    if (status == 0) {
        memcpy(cache.groups.data, groups.data, (size_t) groups.len * sizeof(*groups.data));
        memcpy(cache.layers.data, layers.data, (size_t) layers.len * sizeof(*layers.data));
        if (rebalance_apex_cache(cache.groups, bytes, layers.len)) {
            fputs("Goose CUDA: expert cache 1904/1531 rebalance selected\n", stderr);
        }
        ec_reset(cache.groups, cache.layers, (uint64_t) (uintptr_t) cache.state, cache.work);
        const char *disable_one_touch = getenv("GOOSE_CUDA_DISABLE_ONE_TOUCH_SLRU");
        cache.one_touch = bytes == INT64_C(6592856064) && groups.len == 18 && layers.len == 160 &&
            (!disable_one_touch || strcmp(disable_one_touch, "1") != 0);
        if (cache.one_touch) {
            exp1744_init(0, NULL);
            fputs("Goose CUDA: one-touch SLRU enabled at 6290 MiB\n", stderr);
        }
        cuda.experts = cache;
    } else {
        if (cache.prefill_ready) cuda.event_destroy(cache.prefill_ready);
        if (cache.prefill_release) cuda.event_destroy(cache.prefill_release);
        free_expert_metadata(&cache);
    }
    const int popped = finish_context();
    if (status != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: expert cache allocation failed (%d)\n", status ? status : popped);
        return -1;
    }
    return bytes;
}

int64_t fg_cuda_expert_bytes(void) {
    return (int64_t) cuda.experts.bank_bytes;
}

int64_t fg_cuda_expert_pin_info(sl_i64 info) {
    const char * enable = getenv("GGML_CUDA_REGISTER_HOST");
    const char * disable = getenv("GGML_CUDA_NO_PINNED");
    const bool requested = enable ? strcmp(enable, "1") == 0 : !disable || strcmp(disable, "0") == 0;
    if (!requested || !cuda.host_register || !cuda.host_unregister || info.len != 4) return 0;
    enum { mib = 1024 * 1024 };
    const char * setting = getenv("GOOSE_EXPERT_PIN_MIB");
    int64_t budget = INT64_C(4096) * mib;
    if (setting) {
        char * end;
        const intmax_t value = strtoimax(setting, &end, 10);
        if (end == setting || *end || value <= 0 || value > INT64_MAX / mib) {
            fputs("Goose CUDA: GOOSE_EXPERT_PIN_MIB requires a positive MiB budget\n", stderr);
            return 0;
        }
        budget = (int64_t) value * mib;
    }
    const long page = sysconf(_SC_PAGESIZE);
    const long pages = sysconf(_SC_PHYS_PAGES);
    const uint64_t available = fg143_storage_ram_available();
    if (page <= 0 || pages <= 0 || pages > INT64_MAX / page || available > INT64_MAX) return 0;
    info.data[0] = page;
    info.data[1] = pages * page;
    info.data[2] = (int64_t) available;
    info.data[3] = budget;
    return 1;
}

void fg_cuda_expert_pin_reason(int64_t reason, int64_t bytes, int64_t available) {
    fprintf(stderr, "Goose CUDA: keep expert weights pageable (%s, planned %" PRId64
            " bytes, available %" PRId64 " bytes)\n",
        reason == 1 ? "host RAM headroom" : "nonresident mmap pages", bytes, available);
}

int64_t fg_cuda_host_register(uint64_t address, int64_t bytes) {
    enum { portable = 1, read_only = 8 };
    if (!address || bytes <= 0 || !cuda.host_register) return -1;
    const int error = cuda.host_register((void *) (uintptr_t) address, (size_t) bytes, portable | read_only);
    if (error != 0) fprintf(stderr, "Goose CUDA: mmap expert registration failed (%d)\n", error);
    return error;
}

void fg_cuda_host_unregister(uint64_t address) {
    const int error = cuda.host_unregister((void *) (uintptr_t) address);
    if (error != 0) fprintf(stderr, "Goose CUDA: mmap expert unregistration failed (%d)\n", error);
}

void fg_cuda_expert_pin_register(sl_i64 spans, int64_t bytes) {
    if (spans.len <= 0 || spans.len % 2 != 0 || (uint64_t) spans.len > SIZE_MAX / sizeof(*spans.data) ||
        cuda.expert_pins.len > 0 || !cuda.host_register || !cuda.host_unregister) return;
    const size_t length = (size_t) spans.len * sizeof(*spans.data);
    int64_t * copy = malloc(length);
    if (!copy) return;
    memcpy(copy, spans.data, length);
    const sl_i64 pinned = { copy, spans.len };
    if (start_context() == 1) {
        const int64_t count = ec_manage_pins(pinned, 1);
        if (count > 0) {
            cuda.expert_pins = pinned;
            cuda.expert_pinned_bytes = bytes;
            fprintf(stderr, "Goose CUDA: registered %" PRId64 " expert mmap ranges (%" PRId64 " bytes)\n",
                count, bytes);
        }
        finish_context();
    }
    if (!cuda.expert_pins.len) free(copy);
}

void fg_cuda_expert_record(int64_t hits, int64_t fills, int64_t evictions, int64_t bytes) {
    cuda.experts.hits += (uint64_t) hits;
    cuda.experts.fills += (uint64_t) fills;
    cuda.experts.evictions += (uint64_t) evictions;
    cuda.experts.uploaded_bytes += (uint64_t) bytes;
}

void fg_cuda_expert_stats(sl_i64 stats) {
    if (stats.len != 6) return;
    pthread_mutex_lock(&cuda_lock);
    stats.data[0] = (int64_t) cuda.experts.hits;
    stats.data[1] = (int64_t) cuda.experts.fills;
    stats.data[2] = (int64_t) cuda.experts.evictions;
    stats.data[3] = (int64_t) cuda.experts.uploaded_bytes;
    stats.data[4] = cuda.expert_pinned_bytes;
    stats.data[5] = cuda.expert_pins.len / 2;
    if (cuda.experts.prefill_bytes) {
        fprintf(stderr, "Goose CUDA prefill temporary: %" PRIu64 " projections, %" PRIu64
                    " resident planes, %" PRIu64 " uploaded bytes, %" PRIu64 " prefetched, %" PRIu64
            " discarded\n", cuda.experts.prefill_projections, cuda.experts.prefill_hits,
            cuda.experts.prefill_upload_bytes, cuda.experts.prefill_prefetch_used,
            cuda.experts.prefill_prefetch_discarded);
        if (cuda.experts.prefill_compact) {
            fprintf(stderr, "Goose CUDA prefill compact: %" PRIu64 " full-buffer projections, %" PRIu64
                " prefetch skips for capacity\n", cuda.experts.prefill_full_buffer, cuda.experts.prefill_skipped_capacity);
        }
    }
    pthread_mutex_unlock(&cuda_lock);
}

static void init_expert_host_ring(void) {
    struct expert_host_ring * ring = &cuda.expert_host;
    if (!ring->enabled || ring->storage) return;
    enum { disable_timing = 2, bytes = EXPERT_HOST_SLOTS * EXPERT_HOST_SLOT_BYTES };
    int64_t info[4];
    const int64_t headroom = INT64_C(8) * 1024 * MIB_BYTES;
    if (!fg_cuda_expert_pin_info((sl_i64) {info, 4}) ||
        info[3] < cuda.expert_pinned_bytes || info[3] - cuda.expert_pinned_bytes < bytes ||
        info[2] < headroom + bytes) {
        ring->enabled = false;
        return;
    }
    for (size_t i = 0; i < EXPERT_HOST_SLOTS; ++i) {
        if (cuda.event_create(&ring->ready[i], disable_timing) != 0) {
            ring->enabled = false;
            return;
        }
    }
    void * storage = NULL;
    if (cuda.host_alloc(&storage, bytes, 0) != 0) {
        ring->enabled = false;
        return;
    }
    ring->storage = storage;
}

uint8_t fg_cuda_expert_stage_enabled(int64_t active_experts, uint8_t stage_all) {
    const size_t planes = stage_all ? 3 : 2;
    if (active_experts <= 0 || (!stage_all && active_experts <= 8) ||
        !activations.active || !activations.defer_waits ||
        !cuda.defer_matrix_waits || !cuda.expert_stream ||
        (uint64_t) active_experts > SIZE_MAX / planes / sizeof(*cuda.expert_upload_jobs)) return false;
    init_expert_host_ring();
    if (cuda.expert_pins.len <= 0 && !cuda.expert_host.enabled) return false;
    const size_t capacity = (size_t) active_experts * planes;
    if (capacity > cuda.expert_upload_capacity) {
        struct expert_upload_job * jobs = realloc(cuda.expert_upload_jobs, capacity * sizeof(*jobs));
        if (!jobs) return false;
        cuda.expert_upload_jobs = jobs;
        cuda.expert_upload_capacity = capacity;
    }
    cuda.expert_current_staging = !stage_all && cuda.expert_prefill_staging &&
        (cuda.expert_host.enabled || cuda.expert_pins.len > 0);
    return true;
}

static bool expert_source_pinned(uint64_t source, uint64_t bytes) {
    for (int64_t i = 0; i < cuda.expert_pins.len; i += 2) {
        const uint64_t base = (uint64_t) cuda.expert_pins.data[i];
        const uint64_t span = (uint64_t) cuda.expert_pins.data[i + 1];
        if (source < base) break;
        if (bytes <= span && source - base <= span - bytes) return true;
    }
    return false;
}

static int submit_pageable_expert(const struct expert_upload_job * job, Stream stream);

static int64_t expert_upload(int64_t offset, uint64_t source, int64_t bytes, bool staged) {
    const struct device_buffer * dest = &cuda.experts.weights;
    if (!dest->address || !source || offset < 0 || bytes <= 0 ||
        (uint64_t) offset > dest->bytes || (uint64_t) bytes > dest->bytes - (uint64_t) offset) return -1;
    const DevicePtr address = dest->address + (uint64_t) offset;
    const void * host = (const void *) (uintptr_t) source;
    int status;
    const bool pinned = (staged || cuda.expert_current_staging) && expert_source_pinned(source, (uint64_t) bytes);
    if (staged && (pinned || (cuda.expert_host.enabled && bytes <= EXPERT_HOST_SLOT_BYTES))) {
        if (cuda.expert_upload_count == cuda.expert_upload_capacity) return -1;
        status = 0;
        if (cuda.expert_upload_count == 0) {
            // The chosen slots may still be read by an earlier projection. The
            // release point is rotated so the transfer stream can wait for a
            // wave further back and overlap with the layer that is computing.
            cuda.expert_upload_split = 0;
            cuda.expert_slots_ready = cuda.expert_slots_ring[cuda.expert_slots_index];
            cuda.expert_slots_index = (cuda.expert_slots_index + 1) % EXPERT_SLOT_EVENTS;
            if (cuda.expert_slots_records < EXPERT_SLOT_EVENTS) ++cuda.expert_slots_records;
            status = cuda.event_record(cuda.expert_slots_ready, NULL);
        }
        if (status == 0) {
            cuda.expert_upload_jobs[cuda.expert_upload_count++] =
                (struct expert_upload_job) {address, host, (size_t) bytes, !pinned};
        }
    } else if (!staged && cuda.expert_current_staging && pinned) {
        // Immutable registered mappings need no pageable staging slot.
        activations.pending = true;
        status = cuda.upload_async(address, host, (size_t) bytes, NULL);
    } else if (!staged && cuda.expert_current_staging && cuda.expert_host.enabled &&
               bytes <= EXPERT_HOST_SLOT_BYTES) {
        // The legacy stream orders the upload between the old slot reader
        // and the current projection. Event fences protect the host ring.
        const struct expert_upload_job job = {address, host, (size_t) bytes, true};
        activations.pending = true;
        status = submit_pageable_expert(&job, NULL);
    } else if (cuda.expert_async_copies && cuda.expert_host.enabled && bytes <= EXPERT_HOST_SLOT_BYTES) {
        // A blocking pageable copy costs about 245 us of host time per expert
        // plane beyond the transfer itself, and the host cannot submit the next
        // dependent kernel until it returns. The ring keeps the same legacy
        // stream order at a fraction of that cost.
        const struct expert_upload_job job = {address, host, (size_t) bytes, !pinned};
        activations.pending = true;
        status = pinned ? cuda.upload_async(address, host, (size_t) bytes, NULL)
                        : submit_pageable_expert(&job, NULL);
    } else {
        status = cuda.upload(address, host, (size_t) bytes);
    }
    if (status) fprintf(stderr, "Goose CUDA: expert upload failed (%d)\n", status);
    return status == 0 ? 0 : -1;
}

int64_t fg_cuda_expert_upload(int64_t offset, uint64_t source, int64_t bytes) {
    return expert_upload(offset, source, bytes, false);
}

int64_t fg_cuda_expert_stage_upload(int64_t offset, uint64_t source, int64_t bytes) {
    return expert_upload(offset, source, bytes, true);
}

static void reset_expert_stage(void) {
    struct expert_cache * cache = &cuda.experts;
    cuda.expert_upload_count = 0;
    ec_reset(cache->groups, cache->layers, (uint64_t) (uintptr_t) cache->state, cache->work);
}

static int seal_expert_host_slot(Stream stream) {
    struct expert_host_ring * ring = &cuda.expert_host;
    if (ring->used == 0) return 0;
    const int error = cuda.event_record(ring->ready[ring->index], stream);
    if (error == 0) {
        ring->busy[ring->index] = true;
        ring->index = (ring->index + 1) % EXPERT_HOST_SLOTS;
        ring->used = 0;
    }
    return error;
}

static int reserve_expert_host_slot(size_t bytes, Stream stream, uint8_t ** storage) {
    struct expert_host_ring * ring = &cuda.expert_host;
    int error = 0;
    if (bytes > EXPERT_HOST_SLOT_BYTES - ring->used) error = seal_expert_host_slot(stream);
    if (error == 0 && ring->busy[ring->index]) {
        error = cuda.event_sync(ring->ready[ring->index]);
        if (error == 0) ring->busy[ring->index] = false;
    }
    if (error != 0) return error;
    *storage = ring->storage + ring->index * EXPERT_HOST_SLOT_BYTES + ring->used;
    ring->used += bytes;
    return 0;
}

static int submit_pageable_expert(const struct expert_upload_job * job, Stream stream) {
    uint8_t * storage;
    const int error = reserve_expert_host_slot(job->bytes, stream, &storage);
    if (error != 0) return error;
    memcpy(storage, job->source, job->bytes);
    return cuda.upload_async(job->destination, storage, job->bytes, stream);
}

static int submit_parallel_experts(const struct expert_upload_job * jobs, size_t count, size_t bytes) {
    uint8_t * storage;
    int error = reserve_expert_host_slot(bytes, cuda.expert_stream, &storage);
    if (error != 0) return error;
    size_t split = 0, first_bytes = 0;
    while (split + 1 < count && first_bytes < bytes / 2) {
        first_bytes += jobs[split++].bytes;
    }
    pthread_mutex_lock(&expert_copy_worker.lock);
    expert_copy_worker.task = (struct expert_copy_task) {jobs, split, storage};
    pthread_cond_signal(&expert_copy_worker.ready);
    pthread_mutex_unlock(&expert_copy_worker.lock);
    copy_expert_payloads((struct expert_copy_task) {jobs + split, count - split, storage + first_bytes});
    pthread_mutex_lock(&expert_copy_worker.lock);
    while (expert_copy_worker.task.count != 0) pthread_cond_wait(&expert_copy_worker.done, &expert_copy_worker.lock);
    pthread_mutex_unlock(&expert_copy_worker.lock);
    for (size_t i = 0; error == 0 && i < count; ++i) {
        error = cuda.upload_async(jobs[i].destination, storage, jobs[i].bytes, cuda.expert_stream);
        storage += jobs[i].bytes;
    }
    if (error == 0) ++activations.expert_parallel_batches;
    return error;
}

static bool valid_pinned_batch(const struct expert_upload_job *jobs, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (jobs[i].pageable || !jobs[i].bytes) return false;
        for (size_t j = 0; j < i; ++j) {
            const DevicePtr a = jobs[i].destination, b = jobs[j].destination;
            if (a < b ? b - a < jobs[i].bytes : a - b < jobs[j].bytes) return false;
        }
    }
    return true;
}

static int submit_expert_stage_range(size_t begin, size_t end, bool batch_enabled) {
    enum { max_batch = 24, stream_order = 1 };
    const size_t n = end - begin;
    const struct expert_upload_job *jobs = cuda.expert_upload_jobs + begin;
    if (batch_enabled && cuda.upload_batch && n > 1 && n <= max_batch && valid_pinned_batch(jobs, n)) {
        DevicePtr dst[max_batch], src[max_batch];
        size_t bytes[max_batch];
        for (size_t i = 0; i < n; ++i) {
            dst[i] = jobs[i].destination;
            src[i] = (DevicePtr)(uintptr_t)jobs[i].source;
            bytes[i] = jobs[i].bytes;
        }
        struct memcpy_batch_attributes attributes = {.source_access_order = stream_order};
        size_t first = 0;
        activations.pending = true;
        const int status = cuda.upload_batch(dst, src, bytes, n, &attributes, &first, 1, cuda.expert_stream);
        static bool reported;
        if (status == 0 && !reported) {
            reported = true;
            fputs("Goose CUDA: batch DMA for pinned decode experts selected\n", stderr);
        }
        return status;
    }
    const bool parallel = expert_copy_worker.running && end - begin >= 32;
    int status = 0;
    for (size_t i = begin; status == 0 && i < end;) {
        const struct expert_upload_job * job = &cuda.expert_upload_jobs[i];
        activations.pending = true;
        size_t count = 0, bytes = 0;
        if (parallel && job->pageable) {
            while (i + count < end && job[count].pageable &&
                job[count].bytes <= EXPERT_HOST_SLOT_BYTES - bytes) bytes += job[count++].bytes;
        }
        if (count >= 4) {
            status = submit_parallel_experts(job, count, bytes);
            i += count;
        } else {
            status = job->pageable ? submit_pageable_expert(job, cuda.expert_stream) :
                cuda.upload_async(job->destination, job->source, job->bytes, cuda.expert_stream);
            ++i;
        }
    }
    return status;
}

static Event expert_slot_release(void) {
    // The newest release fences the immediately preceding wave. Using the one
    // recorded before it lets the next wave's transfers run while the current
    // layer computes; a wave's destination slots were last read by an evicted
    // wave many layers earlier, so one wave of slack keeps the order intact.
    if (cuda.expert_slots_records < 2) return cuda.expert_slots_ready;
    return cuda.expert_slots_ring[(cuda.expert_slots_index + EXPERT_SLOT_EVENTS - 2) % EXPERT_SLOT_EVENTS];
}

// Find the down-projection suffix in the queued copies. The SLRU may plan
// all three planes during its first lookup, before the other tensor lookups.
static size_t expert_down_split(const struct expert_upload_job * jobs, size_t count,
        uint64_t tensor, size_t plane_bytes, size_t experts) {
    size_t split = count;
    while (split > 0) {
        const struct expert_upload_job * job = &jobs[split - 1];
        const uint64_t source = (uintptr_t) job->source;
        if (source < tensor || job->bytes != plane_bytes) break;
        const uint64_t offset = source - tensor;
        if (offset % plane_bytes != 0 || offset / plane_bytes >= experts) break;
        --split;
    }
    return split;
}

static int submit_expert_stage(bool batch_enabled) {
    if (cuda.expert_upload_count == 0) return 0;
    // Existing readers release cache slots before this stream writes them.
    // Consumers wait on the recorded completion events.
    int status = cuda.stream_wait_event(cuda.expert_stream, expert_slot_release(), 0);
    // The routed gate and up projections read the first two planes, so the
    // third plane may keep transferring while they run. Split the wave only
    // when both parts are nonempty and the caller asked for it.
    const size_t total = cuda.expert_upload_count;
    const size_t split = cuda.expert_split_planes && cuda.expert_upload_split > 0 &&
        cuda.expert_upload_split < total ? cuda.expert_upload_split : total;
    if (status == 0) status = submit_expert_stage_range(0, split, batch_enabled);
    if (status == 0 && split < total) {
        status = cuda.event_record(cuda.expert_planes_ready, cuda.expert_stream);
        if (status == 0) status = submit_expert_stage_range(split, total, batch_enabled);
    }
    if (status == 0) status = seal_expert_host_slot(cuda.expert_stream);
    if (status == 0) status = cuda.event_record(cuda.expert_uploads_ready, cuda.expert_stream);
    if (status == 0) {
        cuda.expert_planes_split = split < total;
        cuda.expert_upload_count = 0;
        cuda.expert_upload_split = 0;
        cuda.expert_upload_queued = true;
        return 0;
    }
    if (cuda.sync() != 0) cuda.expert_host.enabled = false;
    cuda.expert_host.used = 0;
    reset_expert_stage();
    fprintf(stderr, "Goose CUDA: staged expert upload failed (%d)\n", status);
    return -1;
}

static int begin_expert_prepare(void) {
    decode_capture.scope = false;
    if (decode_capture_submit() != 0) return -1;
    if (cuda.expert_upload_count != 0) {
        reset_expert_stage();
        return -1;
    }
    if (cuda.expert_upload_queued) {
        // The current projection may consume a plane staged by the last one.
        if (cuda.stream_wait_event(NULL, cuda.expert_uploads_ready, 0) != 0) return -1;
        cuda.expert_upload_queued = false;
    }
    return 0;
}

static int64_t prepare_expert_offsets(uint64_t tensor, int64_t bytes, sl_i64 active, sl_i64 offsets,
                                      bool stage_all, bool compact, bool next_plane) {
    struct expert_cache * cache = &cuda.experts;
    const uint64_t state = (uint64_t) (uintptr_t) cache->state;
    const int64_t policy = compact ? (next_plane ? 2 : 1) : 0;
    const int64_t status = cache->one_touch ?
        exp1744_prepare(cache->groups, cache->layers, state, cache->work, tensor, bytes,
            active, offsets, stage_all, compact, policy) :
        compact ? ec_prepare_compact(cache->groups, cache->layers, state, cache->work, tensor, bytes, active, offsets, stage_all) :
        ec_prepare(cache->groups, cache->layers, state, cache->work, tensor, bytes, active, offsets, stage_all);
    if (cuda.expert_current_staging) {
        cuda.expert_current_staging = false;
        if (seal_expert_host_slot(NULL) != 0) {
            cuda.sync();
            cuda.expert_host.used = 0;
            cuda.expert_host.enabled = false;
            reset_expert_stage();
            return -1;
        }
    }
    if (status == 1 && cache->prefill_wait_decode && !fg143_cpu_prefill()) cache->decode_seen = true;
    if (status < 0) reset_expert_stage();
    return status;
}

static int prefill_upload_misses(struct expert_cache * cache, uint64_t tensor, size_t bytes,
        sl_i64 active, const int32_t * slots, size_t scratch, Stream stream) {
    size_t packed = 0;
    for (int64_t e = 0; e < active.len;) {
        if (active.data[e] < 0 || slots[e] >= 0) {
            ++e;
            continue;
        }
        int64_t last = e + 1;
        while (last < active.len && active.data[last] >= 0 && slots[last] < 0) ++last;
        const size_t size = (size_t) (last - e) * bytes;
        const uint64_t source = tensor + (uint64_t) e * bytes;
        const void * host = (const void *) (uintptr_t) source;
        const size_t offset = cache->prefill_compact ? packed : (size_t) e * bytes;
        const DevicePtr dst = cache->weights.address + scratch + offset;
        packed += size;
        activations.pending = true;
        int status;
        if (expert_source_pinned(source, size)) {
            status = cuda.upload_async(dst, host, size, stream);
        } else {
            // The speculative stream is only used for entirely registered planes.
            if (stream) return -1;
            status = cuda.upload(dst, host, size);
        }
        if (status != 0) return -1;
        cache->prefill_upload_bytes += size;
        e = last;
    }
    return 0;
}

// The temporary bytes are separate from the stable SLRU bank. The release
// event fences all earlier scratch readers before the transfer stream writes
// its alternate plane. A readiness event fences the consumer against DMA.
static int64_t prepare_prefill_plane(uint64_t tensor, int64_t bytes, sl_i64 active, sl_i64 offsets) {
    struct expert_cache * cache = &cuda.experts;
    const uint64_t next = cache->prefill_next_tensor;
    cache->prefill_next_tensor = 0;
    if (next && cuda.stream_wait_event(NULL, cache->prefill_ready, 0) != 0) return -1;
    if (!cache->prefill_bytes || !fg143_cpu_prefill() || !activations.active ||
        active.len < TEMP_PREFILL_MIN_EXPERTS ||
        (cache->prefill_wait_decode && !cache->decode_seen)) return 0;
    if (bytes <= 0 || offsets.len < active.len) return -1;

    uint64_t active_bits[64] = {0};
    const bool pipeline = cache->prefill_double && active.len <= 4096;
    size_t selected = 0;
    for (int64_t i = 0; i < active.len; ++i) {
        if (active.data[i] < 0) continue;
        ++selected;
        if (pipeline) active_bits[i / 64] |= UINT64_C(1) << (i % 64);
    }
    if (selected < TEMP_PREFILL_MIN_EXPERTS) return 0;

    for (int64_t i = 0; i < cache->layers.len / EXPERT_LAYOUT_LAYER; ++i) {
        const int64_t * layer = cache->layers.data + i * EXPERT_LAYOUT_LAYER;
        if (layer[0] < 0) continue;
        const int64_t * group = cache->groups.data + layer[0] * EXPERT_LAYOUT_GROUP;
        if (group[3] != active.len || group[5] <= 0) continue;
        size_t plane_offset = 0;
        int plane = 0;
        while (plane < 3 && (uint64_t) layer[1 + plane] != tensor) {
            plane_offset += (size_t) group[plane];
            ++plane;
        }
        if (plane == 3 || group[plane] != bytes) continue;
        if ((uint64_t) bytes > cache->prefill_bytes / (uint64_t) active.len) return -1;

        const int32_t * slots = (const int32_t *) (uintptr_t) cache->work.data[i];
        const size_t slab_bytes = cache->prefill_compact ?
            (cache->prefill_workspace / 2) & ~(size_t) 255 : cache->prefill_bytes;
        size_t misses = 0;
        if (cache->prefill_compact) {
            for (int64_t e = 0; e < active.len; ++e) misses += active.data[e] >= 0 && slots[e] < 0;
        }
        const bool fits = !cache->prefill_compact || misses * (size_t) bytes <= slab_bytes;
        if (!fits) ++cache->prefill_full_buffer;
        const bool prefetched = pipeline && fits && next == tensor && cache->prefill_next_bytes == bytes &&
            cache->prefill_next_count == active.len && cache->prefill_next_fills == cache->fills &&
            memcmp(active_bits, cache->prefill_next_active, sizeof(active_bits)) == 0;
        if (next && !prefetched) ++cache->prefill_prefetch_discarded;
        if (prefetched) ++cache->prefill_prefetch_used;
        const unsigned slab = prefetched ? cache->prefill_next_slot : 0;
        const size_t scratch = cache->prefill_offset + slab * slab_bytes;
        const size_t span = (size_t) group[0] + (size_t) group[1] + (size_t) group[2];
        uint64_t hits = 0;
        size_t packed = 0;
        for (int64_t e = 0; e < active.len; ++e) {
            offsets.data[e] = -1;
            if (active.data[e] < 0) continue;
            const int32_t slot = slots[e];
            if (slot < 0) {
                offsets.data[e] = (int64_t) (scratch + (cache->prefill_compact ? packed : (size_t) e * (size_t) bytes));
                packed += (size_t) bytes;
            } else {
                if (slot >= group[5]) return -1;
                const size_t offset = (size_t) group[6] + (size_t) slot * span + plane_offset;
                if (offset > cache->bank_bytes || (size_t) bytes > cache->bank_bytes - offset) return -1;
                offsets.data[e] = (int64_t) offset;
                ++hits;
            }
        }
        if (!prefetched && prefill_upload_misses(cache, tensor, (size_t) bytes, active, slots, scratch, NULL) != 0)
            return -1;
        // Gate and up can be visited in either order. Once both are prepared,
        // the graph next consumes the down plane with the same route set.
        if (pipeline && plane != 2 && hits < selected) {
            const int next_plane = prefetched ? 2 : 1 - plane;
            const uint64_t source = (uint64_t) layer[1 + next_plane];
            const size_t next_bytes = (size_t) group[next_plane];
            const unsigned next_slab = 1 - slab;
            const size_t next_scratch = cache->prefill_offset + next_slab * slab_bytes;
            const bool next_fits = !cache->prefill_compact || misses * next_bytes <= slab_bytes;
            if (!fits || !next_fits) ++cache->prefill_skipped_capacity;
            if (fits && next_fits && expert_source_pinned(source, next_bytes * (size_t) active.len)) {
                int status = cuda.event_record(cache->prefill_release, NULL);
                if (status == 0) status = cuda.stream_wait_event(cuda.expert_stream, cache->prefill_release, 0);
                if (status == 0) status = prefill_upload_misses(cache, source, next_bytes, active, slots,
                    next_scratch, cuda.expert_stream);
                if (status == 0) status = cuda.event_record(cache->prefill_ready, cuda.expert_stream);
                if (status != 0) {
                    (void) cuda.sync();
                    return -1;
                }
                cache->prefill_next_tensor = source;
                cache->prefill_next_slot = next_slab;
                cache->prefill_next_bytes = (int64_t) next_bytes;
                cache->prefill_next_count = active.len;
                cache->prefill_next_fills = cache->fills;
                memcpy(cache->prefill_next_active, active_bits, sizeof(active_bits));
            }
        }
        ++cache->prefill_projections;
        cache->prefill_hits += hits;
        return 1;
    }
    return 0;
}

int64_t fg_cuda_prepare_experts(uint64_t tensor, int64_t bytes, sl_i64 active, sl_i64 offsets) {
    if (begin_expert_prepare() != 0) return -1;
    if (!cuda.experts.weights.address) return 0;
    const int64_t temporary = prepare_prefill_plane(tensor, bytes, active, offsets);
    if (temporary != 0) return temporary;
    return prepare_expert_offsets(tensor, bytes, active, offsets, false, false, false);
}

bool fg_cuda_glu_pack_offer(sl_i64 plan) {
    if (!cuda.mmq_pack_glu || !cuda.mmq_matrix || !cuda.activation_reference ||
        !activations.active || !activations.defer_waits || plan.len != 8 || !cuda.mmq_shared ||
        !cuda.experts.weights.address ||
        !fg143_routed_layer_matches(cuda.experts.layers, (sl_i64) {plan.data, 3})) return false;
    const uint64_t gate = (uint64_t) plan.data[3], up = (uint64_t) plan.data[4];
    const int64_t bytes = plan.data[6];
    DevicePtr a = activation_device(gate, bytes), b = activation_device(up, bytes);
    if (!a || !b || start_context() != 1) return false;
    enum { extent_value = 2 };
    int32_t extent = extent_value, layout = plan.data[7] == 12 ? 1 : cuda.mmq_q6_matrix ? 3 : 0;
    int32_t first = 0, words = 3;
    DevicePtr jobs = 0;
    const size_t rows = (size_t) bytes / 2048;
    int error = reserve(&cuda.mmq_packed, rows * 288 * extent);
    if (error == 0) {
        void * args[] = {&a, &b, &jobs, &cuda.mmq_packed.address, &extent, &layout, &first, &words};
        activations.pending = true;
        error = cuda.launch(cuda.mmq_pack_glu, 2 * extent, (unsigned) rows, 1, 32, 1, 1, 0, NULL, args, NULL);
    }
    const int popped = finish_context();
    if (error != 0 || popped != 0) return false;
    fg143_activation_discard_overwritten(activation_ranges(), gate, bytes);
    fg143_activation_discard_overwritten(activation_ranges(), up, bytes);
    fg143_activation_discard_overwritten(activation_large_range(), gate, bytes);
    fg143_activation_discard_overwritten(activation_large_range(), up, bytes);
    matrix_glu.target = (uint64_t) plan.data[5];
    matrix_glu.bytes = bytes;
    matrix_glu.packed = false;
    return true;
}

bool fg_cuda_glu_pack_close(void) {
    const bool packed = matrix_glu.packed;
    matrix_glu.target = 0;
    matrix_glu_active = false;
    return packed;
}

int64_t fg_cuda_begin(uint64_t input, int64_t input_bytes, uint64_t output, int64_t output_bytes) {
    const int active = start_context();
    if (active != 1) return active;
    cuda_matrix_kind = -1;
    matrix_input = 0;
    matrix_glu_active = false;
    matrix_reusable_host = 0;
    if (matrix_glu.target != 0) {
        int status = input == matrix_glu.target && input_bytes == matrix_glu.bytes ?
            activation_output(output, output_bytes, &matrix_output) : -1;
        if (status == 0) {
            matrix_glu_active = true;
            return 1;
        }
        (void) finish_context();
        return -1;
    }
    const sl_u8 source = {(void *) (uintptr_t) input, input_bytes};
    DevicePtr * staged = cuda.stage_matrix_input ? &matrix_input : NULL;
    int status = activation_output_for_input(output, output_bytes, &matrix_output, source, staged);
    if (status == 0 && !matrix_input) status = activation_input(source, &cuda.input, &matrix_input);
    if (status == 0) {
        if (cuda.mmq_shared_reuse && activations.active && activations.step >= 0 &&
            input_bytes > 0 && output_bytes > 0 &&
            (input < output ? output - input >= (uint64_t) input_bytes : input - output >= (uint64_t) output_bytes) &&
            matrix_input == activation_device(input, input_bytes)) {
            matrix_reusable_host = input;
            matrix_input_bytes = input_bytes;
        }
        return 1;
    }
    finish_context();
    fprintf(stderr, "Goose CUDA: input transfer failed (%d)\n", status);
    return -1;
}

static int launch_mmq_stream(DevicePtr weight, uint64_t stride, int extent, int kind, int rows, int first,
                            int first_route, int job_words, int batch, int workers, int64_t input_stride) {
    const int64_t outputs = (int64_t) (1 + (rows - 1) / 64) * batch;
    const bool fixup = outputs % workers != 0;
    int error = fixup ? reserve(&cuda.mmq_partials, (size_t) workers * 64 * 8 * sizeof(float)) : 0;
    if (error != 0) return error;
    void * args[] = {&weight, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address, &cuda.mmq_tiles.address,
        &cuda.mmq_partials.address, &stride, &extent, &kind, &rows, &first, &first_route, &job_words, &batch, &input_stride};
    error = cuda.launch(cuda.mmq_stream, (unsigned) workers, 1, 1, 128, 1, 1, 0, NULL, args, NULL);
    if (error != 0) return error;
    ++cuda_mmq_stream_windows;
    if (fixup) {
        void * merge[] = {&matrix_output, &cuda.jobs.address, &cuda.mmq_tiles.address, &cuda.mmq_partials.address,
            &extent, &rows, &first, &job_words, &batch};
        error = cuda.launch(cuda.mmq_stream_fixup, (unsigned) workers, 1, 1, 128, 1, 1, 0, NULL, merge, NULL);
        if (error == 0) ++cuda_mmq_fixups;
    }
    return error;
}

static int finish_matrix_launches(int error) {
    // Jobs and intermediate buffers are ordered on the legacy stream.
    // Host publication and graph completion already wait for their writes.
    if (error == 0 && cuda.defer_matrix_waits && activations.defer_waits) {
        activations.pending = true;
        return 0;
    }
    const int sync_error = cuda.sync();
    return error == 0 ? sync_error : error;
}

static int launch_mmq(DevicePtr weight, uint64_t stride, int extent, int kind,
                      int rows, sl_i64 jobs, int job_words, int64_t shared_rows, int64_t input_stride) {
    enum { staging_budget = 16 * 1024 * 1024, q4_kind = 12, q6_kind = 14,
        mmq_d4 = 0, mmq_ds4 = 1, mmq_d4_reciprocal = 3, mmq_ds4_reciprocal = 4,
        wide_reference_columns = 4096, reference_tile_width = 128 };
    const int count = (int) (jobs.len / job_words);
    if (extent > INT_MAX / 256 || (size_t) count > SIZE_MAX / (3 * sizeof(int64_t))) return -1;
    Function reference_matrix = kind == q6_kind && rows % 128 == 0 ? cuda.mmq_q6_matrix :
        kind == q4_kind ? cuda.mmq_q4_matrix : NULL;
    const bool dense_reference = reference_matrix && job_words == 2 &&
        (int64_t) (1 + (rows - 1) / 128) * (1 + (count - 1) / 128) <= 1073741823 / extent;
    const bool indexed_reference = reference_matrix && job_words == 3 &&
        ((kind == q4_kind && rows == 512 && extent == 8) ||
         (kind == q6_kind && rows == 2048 && extent == 2));
    const bool reference = dense_reference || indexed_reference;
    const Function routed_x64 = kind == q4_kind ? cuda.mmq_q4_routed_x64 : cuda.mmq_q6_routed_x64;
    const Function routed_shared = routed_x64 ? routed_x64 :
        kind == q4_kind ? cuda.mmq_q4_routed_shared : cuda.mmq_q6_routed_shared;
    const Function dense_shared = kind == q4_kind ? cuda.mmq_q4_dense_shared : cuda.mmq_q6_dense_shared;
    const Function dense_x64 = kind == q4_kind ? cuda.mmq_q4_dense_x64 : cuda.mmq_q6_dense_x64;
    const Function dense_x128 = kind == q4_kind ? cuda.mmq_q4_dense_x128 : cuda.mmq_q6_dense_x128;
    // These column counts have the same reference tile width, grid and K
    // partitions as the geometry baked into the cooperative kernels.
    const bool wide_geometry = count == wide_reference_columns || (cuda.mmq_dense_partial &&
        (count - 1) / reference_tile_width == (wide_reference_columns - 1) / reference_tile_width);
    const bool dense_wide = dense_shared && dense_reference && wide_geometry && rows % 128 == 0;
    const int64_t reference_tiles = (int64_t) (rows / 128) * (wide_reference_columns / reference_tile_width);
    const int64_t sms = cuda.multiprocessors;
    // A complete reference tile has a single K interval at this occupancy.
    const bool unsplit = dense_wide && sms > 0 &&
        100 * reference_tiles / (((reference_tiles - 1) / sms + 1) * sms) >= 90;
    const bool dense_m64 = kind == q4_kind && cuda.mmq_q4_dense_m64 && unsplit;
    const bool dense_q6_half_k = kind == q6_kind && cuda.mmq_q6_dense_half_k && unsplit;
    const bool routed_wide = indexed_reference && routed_shared;
    const Function routed_x128 = !routed_wide ? NULL : kind == q4_kind ? cuda.mmq_q4_routed_x128 : cuda.mmq_q6_routed_x128;
    const bool routed_down = kind == q4_kind && job_words == 3 && rows == 2048 && extent == 2 &&
        count >= wide_reference_columns && cuda.mmq_q4_routed_down_m128;
    const int routes_per_tile = routed_down || routed_x128 ? 128 : routed_wide ? (routed_x64 ? 64 : 32) : 8;
    int32_t reference_columns = indexed_reference ? -1 : dense_reference ? count : 0;
    int32_t layout = kind == q4_kind ? (reference ? mmq_ds4_reciprocal : mmq_ds4) :
        (reference ? mmq_d4_reciprocal : mmq_d4);
    int64_t * tiles = malloc((size_t) count * 3 * sizeof(*tiles));
    if (!tiles) return -1;
    const int64_t tile_count = fg143_mmq_tiles(jobs, job_words, routes_per_tile, (sl_i64) {tiles, 3ll * count});
    int64_t window_tiles = fg143_mmq_window(extent, tile_count, routes_per_tile, staging_budget);
    if (!cuda.mmq_window && window_tiles > 64) window_tiles = 64;
    if (tile_count <= 0 || tile_count > count || window_tiles <= 0) {
        free(tiles);
        return -1;
    }
    const size_t tile_bytes = (size_t) tile_count * 3 * sizeof(*tiles);
    if (matrix_glu_active) {
        shared_rows = matrix_glu.bytes / 2048;
        input_stride = 2048;
    }
    bool shared = matrix_glu_active || (cuda.mmq_shared && shared_rows > 0 && shared_rows < count && shared_rows <= 65535 &&
        input_stride >= (int64_t) extent * 256 * (int64_t) sizeof(float) &&
        (uint64_t) shared_rows * (uint64_t) extent * 288 <= staging_budget);
    size_t packed_bytes;
    int error;
    for (;;) {
        if (cuda.mmq_align_window && dense_wide && !shared) {
            const int group = dense_x128 && window_tiles >= 16 ? 16 : dense_x64 && window_tiles >= 8 ? 8 : 4;
            window_tiles = fg143_mmq_dense_window(tile_count, window_tiles, group);
        }
        const int64_t window_routes = window_tiles * routes_per_tile;
        const size_t routes = (size_t) (shared ? shared_rows : count < window_routes ? count : window_routes);
        if ((size_t) extent > SIZE_MAX / 288 / routes) {
            free(tiles);
            return -1;
        }
        packed_bytes = routes * (size_t) extent * 288;
        error = reserve(&cuda.mmq_packed, packed_bytes);
        if (error == 2 && shared && !matrix_glu_active) {
            shared = false;
            continue;
        }
        if (error != 2 || window_tiles == 1) break;
        window_tiles /= 2;
    }
    if (error == 0) error = reserve(&cuda.mmq_tiles, tile_bytes);
    if (error == 0 && !routed_x128) error = cuda.upload(cuda.mmq_tiles.address, tiles, tile_bytes);
    if (!shared) input_stride = 0;
    // Only reuse a pack from the immediately preceding graph step, with
    // both projections reading the same live, unaliased activation.
    const bool cache_shared = shared && job_words == 3 && matrix_reusable_host != 0;
    const int64_t dense_stride = (int64_t) extent * 256 * sizeof(float);
    const bool cache_dense = cuda.mmq_dense_reuse && !shared && job_words == 2 && kind == q4_kind &&
        window_tiles >= tile_count && matrix_reusable_host != 0 &&
        fg143_mmq_linear_jobs(jobs, dense_stride, matrix_input_bytes);
    const int64_t cache_rows = cache_dense ? count : shared_rows;
    const int64_t cache_stride = cache_dense ? dense_stride : input_stride;
    const bool reuse_pack = (cache_shared || cache_dense) &&
        shared_activation_pack.dense == cache_dense && shared_activation_pack.host == matrix_reusable_host &&
        shared_activation_pack.step == activations.step - 1 &&
        shared_activation_pack.input == matrix_input && shared_activation_pack.packed == cuda.mmq_packed.address &&
        shared_activation_pack.bytes == matrix_input_bytes && shared_activation_pack.rows == cache_rows &&
        shared_activation_pack.stride == cache_stride && shared_activation_pack.extent == extent &&
        shared_activation_pack.layout == layout;
    shared_activation_pack.host = 0;
    if (error == 0 && shared) {
        if (matrix_glu_active) matrix_glu.packed = true;
        if (cuda.mmq_window) window_tiles = tile_count < 8191 ? tile_count : 8191;
        if (matrix_glu_active) {
            // SwiGLU has already been packed while both operands were live.
        } else if (!reuse_pack) {
            void * args[] = {&matrix_input, &cuda.mmq_packed.address, &extent, &layout, &input_stride};
            error = cuda.launch(cuda.mmq_pack_shared, (unsigned) (2 * extent), (unsigned) shared_rows, 1,
                32, 1, 1, 0, NULL, args, NULL);
        } else {
            ++activations.shared_pack_reuses;
        }
    }
    int first = 0, first_route = 0;
    bool weights_prepared = false;
    void * pack_args[] = {&matrix_input, &cuda.jobs.address, &cuda.mmq_packed.address,
                         &extent, &layout, &first_route, &job_words};
    void * matrix_args[] = {&weight, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address,
        &cuda.mmq_tiles.address, &stride, &extent, &kind, &rows, &first, &first_route, &job_words, &input_stride,
        &reference_columns, &cuda.multiprocessors};
    while (error == 0 && first < tile_count) {
        const int batch = (int) (tile_count - first > window_tiles ? window_tiles : tile_count - first);
        const int end = first + batch;
        const int64_t last = 3ll * (end - 1);
        first_route = (int) tiles[3ll * first];
        const unsigned routes = (unsigned) (tiles[last] + tiles[last + 1] - first_route);
        const bool q6_half_k = dense_q6_half_k && !shared && first % 16 == 0 && first_route == 8 * first;
        int32_t columns = (int32_t) routes;
        int wide = 0;
        if (routed_x128) {
            int64_t * window = tiles + 3ll * first;
            wide = (int) fg143_mmq_wide_first((sl_i64) {window, 3ll * batch});
            error = upload_packet(cuda.mmq_tiles.address + 3ull * first * sizeof(*tiles),
                                  window, 3ull * batch * sizeof(*tiles));
        }
        if (error == 0 && !shared && !reuse_pack) {
            if (q6_half_k) {
                void * args[] = {&matrix_input, &cuda.jobs.address, &cuda.mmq_packed.address,
                    &extent, &layout, &first_route, &columns};
                error = cuda.launch(cuda.mmq_pack_k_major, (unsigned) (2 * extent), routes, 1,
                    32, 1, 1, 0, NULL, args, NULL);
            } else {
                error = cuda.launch(cuda.mmq_pack, (unsigned) (2 * extent), routes, 1,
                    32, 1, 1, 0, NULL, pack_args, NULL);
            }
        }
        if (error == 0) {
            const int workers = cuda.mmq_stream && !reference ?
                (int) fg143_mmq_stream_workers(rows, batch, extent, cuda.multiprocessors) : 0;
            if (routed_wide) {
                int tile = first;
                void * args[] = {&weight, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address,
                    &cuda.mmq_tiles.address, &stride, &extent, &rows, &tile, &first_route, &input_stride};
                if (wide > 0) {
                    const Function tall = rows % 128 != 0 ? NULL :
                        kind == q4_kind ? cuda.mmq_q4_routed_m128 : cuda.mmq_q6_routed_pipeline;
                    const bool q6_pipeline = tall && kind == q6_kind;
                    error = cuda.launch(tall ? tall : routed_x128, (unsigned) rows / (tall ? 128 : 16),
                        (unsigned) wide, 1, 256, 1, 1, q6_pipeline ? Q6_ROUTED_SHARED_BYTES : 0, NULL, args, NULL);
                    if (error == 0 && q6_pipeline) ++activations.q6_routed_pipelined;
                    if (error == 0 && wide < batch) ++cuda_matrix_launches;
                    tile += wide;
                }
                if (error == 0 && wide < batch) {
                    error = cuda.launch(routed_shared, (unsigned) rows / 16, (unsigned) (batch - wide),
                        1, 128, 1, 1, 0, NULL, args, NULL);
                }
            } else if (routed_down) {
                int tile = first;
                void * args[] = {&weight, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address,
                    &cuda.mmq_tiles.address, &stride, &extent, &rows, &tile, &first_route, &input_stride};
                error = cuda.launch(cuda.mmq_q4_routed_down_m128, (unsigned) rows / 128, (unsigned) batch,
                    1, 256, 1, 1, 0, NULL, args, NULL);
            } else if (q6_half_k) {
                const bool pipeline = cuda.mmq_q6_dense_pipeline != NULL;
                const Function matrix = pipeline ? cuda.mmq_q6_dense_pipeline : cuda.mmq_q6_dense_half_k;
                void * args[] = {&weight, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address,
                    &stride, &extent, &first_route, &columns};
                error = cuda.launch(matrix, (unsigned) rows / 128, 1 + (routes - 1) / 128,
                    1, 256, 1, 1, pipeline ? Q6_PIPE_SHARED_BYTES : Q6_HALF_K_SHARED_BYTES, NULL, args, NULL);
                if (error == 0 && pipeline) ++activations.q6_pipelined;
            } else if (dense_m64 && !shared && first % 16 == 0 && first_route == 8 * first) {
                const bool large = cuda.mmq_q4_dense_large && routes >= cuda.mmq_q4_large_cols &&
                    (unsigned) rows % cuda.mmq_q4_large_rows == 0;
                // The same legacy stream orders the temporary weight image
                // with every producer and consumer of attention scratch.
                const bool prepare = large && cuda.mmq_q4_dense_prepared && routes >= 1024 && extent <= 65535 / 4 &&
                    cuda.attention_scratch.address && (uint64_t) rows * extent * 320 <= cuda.attention_scratch.bytes;
                DevicePtr matrix_weights = prepare ? cuda.attention_scratch.address : weight;
                if (prepare && !weights_prepared) {
                    void * args[] = {&weight, &matrix_weights, &stride, &extent};
                    error = cuda.launch(cuda.mmq_q4_prepare, (unsigned) rows / 16, 4u * extent, 1,
                        32, 1, 1, 0, NULL, args, NULL);
                    if (error == 0) {
                        weights_prepared = true;
                        ++activations.q4_prepared;
                    }
                }
                const bool pipeline = prepare && cuda.mmq_q4_dense_pipeline;
                const Function matrix = pipeline ? cuda.mmq_q4_dense_pipeline :
                    prepare ? cuda.mmq_q4_dense_prepared : large ? cuda.mmq_q4_dense_large :
                    cuda.mmq_q4_dense_m128 ? cuda.mmq_q4_dense_m128 : cuda.mmq_q4_dense_m64;
                const unsigned row_tile = large ? cuda.mmq_q4_large_rows : cuda.mmq_q4_dense_m128 ? 128 : 64;
                const unsigned column_tile = large ? cuda.mmq_q4_large_cols : 128;
                void * args[] = {&matrix_weights, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address,
                    &stride, &extent, &first_route, &columns};
                if (error == 0) error = cuda.launch(matrix, (unsigned) rows / row_tile,
                    1 + (routes - 1) / column_tile, 1, 256, 1, 1,
                    pipeline ? Q4_PIPE_SHARED_BYTES : 0, NULL, args, NULL);
                if (error == 0 && pipeline) ++activations.q4_pipelined;
            } else if (dense_wide && !shared && first % 4 == 0 &&
                first_route == 8 * first && routes >= 32) {
                const bool x128 = dense_x128 && first % 16 == 0 && batch % 16 == 0 && routes >= 128;
                const bool x64 = dense_x64 && first % 8 == 0 && batch % 8 == 0 && routes >= 64;
                const unsigned group = x128 ? 16 : x64 ? 8 : 4;
                const Function matrix = x128 ? dense_x128 : x64 ? dense_x64 : dense_shared;
                const unsigned complete = routes / (8 * group);
                void * args[] = {&weight, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address,
                    &stride, &extent, &rows, &first_route, &cuda.multiprocessors};
                error = cuda.launch(matrix, (unsigned) rows / 16, complete,
                    1, x128 ? 256 : 128, 1, 1, 0, NULL, args, NULL);
                first += (int) (complete * group);
                if (error == 0 && first < end) {
                    // Its packed rows remain in the same window as the wide prefix.
                    ++cuda_matrix_launches;
                    error = cuda.launch(reference_matrix, 1u + ((unsigned) rows - 1) / 64,
                        (unsigned) (end - first), 1, 128, 1, 1, 0, NULL, matrix_args, NULL);
                }
            } else if (workers > 0) {
                error = launch_mmq_stream(weight, stride, extent, kind, rows, first, first_route, job_words, batch, workers, input_stride);
            } else {
                Function matrix = reference ? reference_matrix : cuda.mmq_matrix;
                error = cuda.launch(matrix, 1u + ((unsigned) rows - 1) / 64, (unsigned) batch, 1,
                                    128, 1, 1, 0, NULL, matrix_args, NULL);
            }
            if (error == 0) ++cuda_matrix_launches;
        }
        first = end;
    }
    free(tiles);
    if (first != 0) error = finish_matrix_launches(error);
    if (error != 0) {
        fprintf(stderr, "Goose CUDA: MMQ failed (%d), packed activations need %zu bytes\n", error, packed_bytes);
        return -1;
    }
    if (cache_shared || cache_dense) {
        shared_activation_pack = (__typeof__(shared_activation_pack)) {
            .host = matrix_reusable_host, .input = matrix_input, .packed = cuda.mmq_packed.address,
            .bytes = matrix_input_bytes, .step = activations.step, .rows = cache_rows, .stride = cache_stride,
            .extent = extent, .layout = layout, .dense = cache_dense,
        };
        if (reuse_pack && cache_dense) ++activations.dense_pack_reuses;
    }
    ++cuda_mmq_projections;
    return 0;
}

static int launch_cuda_kernels(const struct kernel_node_params * kernels, size_t count, Stream stream) {
    int error = 0;
    for (size_t i = 0; i < count && error == 0; ++i) {
        const struct kernel_node_params * p = &kernels[i];
        error = cuda.launch(p->function, p->grid_x, p->grid_y, p->grid_z, p->block_x, p->block_y, p->block_z,
            p->shared_bytes, stream, p->params, p->extra);
    }
    return error;
}

static int capture_cuda_graph(const struct kernel_node_params * kernels, size_t count, Graph * graph) {
    enum { blocking_stream = 0, thread_local_capture = 1 };
    // All capture scopes share a stream with legacy-stream ordering.
    int error = cuda.graph_stream ? 0 : cuda.stream_create(&cuda.graph_stream, blocking_stream);
    if (error == 0) error = cuda.graph_begin(cuda.graph_stream, thread_local_capture);
    if (error != 0) return error;
    error = launch_cuda_kernels(kernels, count, cuda.graph_stream);
    const int ended = cuda.graph_end(cuda.graph_stream, graph);
    return error != 0 ? error : ended;
}

static GraphExec cached_graph_for(const uint64_t key[MMVQ_GRAPH_KEY_WORDS],
                                  const struct kernel_node_params * kernels, size_t kernel_count) {
    if (!cuda.mmvq_graph_cache) return NULL;
    const size_t key_bytes = sizeof(cuda.mmvq_graph_cache[0].key);
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < MMVQ_GRAPH_KEY_WORDS; ++i) {
        hash = (hash ^ (key[i] >> 32)) * UINT64_C(1099511628211);
        hash = (hash ^ (key[i] & UINT32_MAX)) * UINT64_C(1099511628211);
    }
    hash ^= hash >> 33;
    hash *= UINT64_C(0xff51afd7ed558ccd);
    hash ^= hash >> 33;
    struct mmvq_graph_entry * entry = NULL, * uninstantiated = NULL;
    for (size_t probe = 0; probe < 8; ++probe) {
        struct mmvq_graph_entry * slot = &cuda.mmvq_graph_cache[(hash + probe) % MMVQ_GRAPH_SLOTS];
        if (memcmp(slot->key, key, key_bytes) == 0) {
            entry = slot;
            break;
        }
        if (slot->key[0] == 0) {
            uninstantiated = slot;
            break;
        }
        if (!slot->executable && !uninstantiated) uninstantiated = slot;
    }
    if (!entry) {
        // Admit recurring signatures without evicting live executables.
        if (uninstantiated) memcpy(uninstantiated->key, key, key_bytes);
        return NULL;
    }
    if (entry->executable) return entry->executable;
    Graph graph = NULL;
    int error = capture_cuda_graph(kernels, kernel_count, &graph);
    if (error == 0) error = cuda.graph_instantiate(&entry->executable, graph, 0);
    if (graph) cuda.graph_destroy(graph);
    if (error == 0) return entry->executable;
    if (getenv("GOOSE_GRAPH_LOG")) fprintf(stderr, "Goose CUDA cached graph: setup failed (%d)\n", error);
    release_mmvq_graphs();
    return NULL;
}

static int launch_mmvq(DevicePtr weight, uint64_t stride, int extent, int kind, int rows, int count,
                       int words, const int64_t *dense_offsets) {
    shared_activation_pack.host = 0;
    const size_t packed_bytes = (size_t) extent * 288 * (size_t) count;
    int error = reserve(&cuda.mmq_packed, packed_bytes);
    if (error == 0) {
        DevicePtr input = matrix_input, output = matrix_output;
        if (dense_offsets) {
            input += (uint64_t) dense_offsets[0];
            output += (uint64_t) dense_offsets[1];
        }
        void * packet_pack[] = {&input, &cuda.mmq_packed.address, &cuda.jobs.address, &extent, &words};
        void * packet_matrix[] = {&weight, &cuda.mmq_packed.address, &output, &cuda.jobs.address,
            &stride, &extent, &kind, &words};
        void * dense_pack[] = {&input, &cuda.mmq_packed.address, &extent};
        void * dense_matrix[] = {&weight, &cuda.mmq_packed.address, &output, &stride, &extent, &kind};
        void **pack = dense_offsets ? dense_pack : packet_pack;
        void **matrix = dense_offsets ? dense_matrix : packet_matrix;
        const Function pack_function = dense_offsets ? cuda.mmvq_dense_pack : cuda.mmvq_pack;
        const Function matrix_function = dense_offsets ? cuda.mmvq_dense_row : cuda.mmvq;
        GraphExec graph = NULL;
        if (!decode_capture.scope && cuda.mmvq_graph_enabled && cuda.mmvq_graph_cache && activations.active && activations.defer_waits &&
            (count == 1 || count == 8)) {
            const struct kernel_node_params kernels[] = {
                { .function = pack_function, .grid_x = (unsigned) (2 * extent), .grid_y = (unsigned) count,
                  .grid_z = 1, .block_x = 128, .block_y = 1, .block_z = 1, .params = pack },
                { .function = matrix_function, .grid_x = (unsigned) rows, .grid_y = (unsigned) count,
                  .grid_z = 1, .block_x = 128, .block_y = 1, .block_z = 1, .params = matrix },
            };
            // The ordered descriptor upload provides the current expert slots.
            const uint64_t key[MMVQ_GRAPH_KEY_WORDS] = {weight, stride, input, output,
                cuda.mmq_packed.address, dense_offsets ? 0 : cuda.jobs.address,
                ((uint64_t) extent << 32) | (unsigned) rows,
                ((uint64_t) count << 32) | (unsigned) words, (unsigned) kind};
            graph = cached_graph_for(key, kernels, 2);
        }
        if (graph) {
            error = cuda.graph_launch(graph, cuda.graph_stream);
        } else {
            error = cuda.launch(pack_function, (unsigned) (2 * extent), (unsigned) count, 1, 128, 1, 1, 0, NULL, pack, NULL);
            if (error == 0) {
                error = cuda.launch(matrix_function, (unsigned) rows, (unsigned) count, 1, 128, 1, 1, 0, NULL, matrix, NULL);
            }
        }
        error = finish_matrix_launches(error);
    }
    if (error != 0) {
        fprintf(stderr, "Goose CUDA: MMVQ failed (%d)\n", error);
        return -1;
    }
    ++cuda_matrix_launches;
    cuda_matrix_kind = kind;
    return 0;
}

static int launch_matrix(DevicePtr weight, int64_t row_stride, int64_t extent,
                         int64_t kind, int64_t rows, sl_i64 jobs, int job_words, int64_t shared_rows, int64_t input_stride) {
    if (!weight || rows <= 0 || rows > INT_MAX || extent <= 0 || extent > INT_MAX ||
        (kind != 0 && kind != 12 && kind != 14) ||
        jobs.len <= 0 || jobs.len % job_words != 0 || jobs.len / job_words > INT_MAX ||
        (uint64_t) jobs.len > SIZE_MAX / sizeof(*jobs.data)) return -1;
    if (matrix_glu_active && (kind == 0 || extent != 2 || job_words != 3 ||
        shared_rows != 0 || jobs.len / job_words < 16)) return -1;
    if (kind != 0 && extent <= INT_MAX / 256 && job_words == 2 && jobs.len == 2 &&
        cuda.mmvq_dense_pack && cuda.mmvq_dense_row && jobs.data[0] >= 0 && jobs.data[1] >= 0 &&
        (uint64_t) jobs.data[0] <= UINT64_MAX - matrix_input &&
        (uint64_t) jobs.data[1] <= UINT64_MAX - matrix_output) {
        static bool dense_mmvq_logged;
        if (!dense_mmvq_logged && !fg143_cpu_prefill()) {
            dense_mmvq_logged = true;
            fprintf(stderr, "Goose CUDA: dense decode MMVQ uses direct tensor addresses\n");
        }
        return launch_mmvq(weight, (uint64_t) row_stride, (int) extent, (int) kind,
            (int) rows, 1, job_words, jobs.data);
    }
    const size_t job_bytes = (size_t) jobs.len * sizeof(*jobs.data);
    int error = reserve(&cuda.jobs, job_bytes);
    if (error == 0) {
        // Only the large packs pay the synchronous copy; a small pack keeps the
        // pinned packet ring it already used, so decode is untouched.
        const int queued = job_bytes > 4096 ? upload_jobs_async(cuda.jobs.address, jobs.data, job_bytes) : -2;
        error = queued == -2 ? upload_packet(cuda.jobs.address, jobs.data, job_bytes) : queued;
    }
    if (error == 0 && cuda.mmq_matrix && kind != 0 && jobs.len / job_words >= 16) {
        const int result = launch_mmq(weight, (uint64_t) row_stride, (int) extent, (int) kind,
                                      (int) rows, jobs, job_words, shared_rows, input_stride);
        if (result == 0) cuda_matrix_kind = (int) kind;
        return result;
    }

    const int count = (int) (jobs.len / job_words);
    if (error == 0 && cuda.mmvq && kind != 0 && extent <= INT_MAX / 256 &&
        (count == 1 || (job_words == 3 && count == 8) ||
         (cuda.mmvq_dense_tail && job_words == 2 && count <= 8))) {
        return launch_mmvq(weight, (uint64_t) row_stride, (int) extent, (int) kind, (int) rows, count, job_words, NULL);
    }
    if (error == 0 && cuda.dense_mma && kind == 0 && job_words == 2 && rows >= 16 && count >= 16 && count <= 8 * 65535) {
        int32_t k = (int32_t) extent, m = (int32_t) rows;
        int32_t jobs_count = count;
        // These SM89 router geometries were traced in the frozen reference.
        int32_t partitions = 0;
        if (cuda.dense_mma_reference && rows == 256 && extent == 2048) {
            if (count == 124) partitions = 4;
            if (count == 4096) partitions = 1;
        }
        Function matrix = partitions > 0 ? cuda.dense_mma_reference : cuda.dense_mma;
        void * args[] = {&weight, &matrix_input, &matrix_output, &cuda.jobs.address, &row_stride, &k, &m, &jobs_count,
            &partitions};
        error = cuda.launch(matrix, 1u + ((unsigned) rows - 1) / 64, 1u + ((unsigned) count - 1) / 8, 1,
            128, 1, 1, 0, NULL, args, NULL);
        if (error == 0) error = finish_matrix_launches(error);
        if (error != 0) {
            fprintf(stderr, "Goose CUDA: dense MMA failed (%d)\n", error);
            return -1;
        }
        ++cuda_matrix_launches;
        ++cuda_dense_mma;
        cuda_matrix_kind = 0;
        return 0;
    }

    uint64_t stride = (uint64_t) row_stride;
    int width = (int) extent, type = (int) kind, start = 0;
    void * args[] = { &weight, &matrix_input, &matrix_output,
                      &cuda.jobs.address, &stride, &width, &type, &start, &job_words };
    const unsigned lanes = kind == 14 ? 64 : 32;
    while (error == 0 && start < count) {
        const int batch = count - start > 65535 ? 65535 : count - start;
        error = cuda.launch(cuda.kernel, (unsigned) rows, (unsigned) batch, 1,
                            lanes, 1, 1, 0, NULL, args, NULL);
        if (error == 0) ++cuda_matrix_launches;
        start += batch;
    }
    if (start != 0) error = finish_matrix_launches(error);
    if (error != 0) fprintf(stderr, "Goose CUDA: matrix kernel failed (%d)\n", error);
    else cuda_matrix_kind = type;
    return error == 0 ? 0 : -1;
}

// ---- Expert prefetch from a one-layer-ahead route prediction (EXP-PF1) ----
// At the decode FFN of layer L the router of layer L+1 is applied to the FFN
// input of layer L on a side stream. After the FFN of L is submitted, the
// predicted experts of L+1 go through the regular SLRU prepare path, so their
// misses are copied while L computes. The prediction only chooses which expert
// weights are copied early; the model always uses its own routes, and every
// consumer still waits on the regular readiness events. GOOSE_EXPERT_PREFETCH=0
// disables it.
#include "pred-image.h"
enum { PF_LAYERS = 64, PF_EXPERTS = 256, PF_MAX_K = 32 };
static struct {
    int state;                 // 0 untried, 1 ready, -1 off
    unsigned k;
    Function route;
    Stream stream;
    DevicePtr dev_logits, dev_counter, dev_host;
    volatile int32_t * host_ids;   // [0] sequence, [1..k] predicted ids
    int seq;
    uint64_t router_host[PF_LAYERS];
    DevicePtr router_dev[PF_LAYERS];
    int routers;
    uint64_t layer_up[PF_LAYERS];
    int layers;
    struct { uint64_t w[3]; int64_t plane[3]; uint64_t down_base; size_t down_plane; bool disjoint, ok; } plan[PF_LAYERS];
    int pending;
    uint64_t launched, waves, jobs, skipped, ns_launch, ns_wait, ns_prepare, ns_submit;
} pf = {.pending = -1};
static uint64_t pf_now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t) t.tv_sec * 1000000000ull + (uint64_t) t.tv_nsec; }

static bool pf_init(void) {
    if (pf.state) return pf.state == 1;
    pf.state = -1;
    const char * off = getenv("GOOSE_EXPERT_PREFETCH");
    if (off && off[0] == '0' && !off[1]) return false;
    const char * k = getenv("GOOSE_EXPERT_PREFETCH_K");
    pf.k = k ? (unsigned) strtoul(k, NULL, 10) : 8;
    if (pf.k == 0 || pf.k > PF_MAX_K) return false;
    int (*host_device)(DevicePtr *, void *, unsigned) =
        (int (*)(DevicePtr *, void *, unsigned)) dlsym(cuda.library, "cuMemHostGetDevicePointer_v2");
    Module module = NULL;
    void * host = NULL;
    if (!host_device || !cuda.host_alloc || !cuda.zero ||
        cuda.module_load(&module, pred_image) != 0 ||
        cuda.function_get(&pf.route, module, "goose_pred_route") != 0 ||
        cuda.stream_create(&pf.stream, 1 /* CU_STREAM_NON_BLOCKING */) != 0 ||
        cuda.alloc(&pf.dev_logits, PF_EXPERTS * 4) != 0 ||
        cuda.alloc(&pf.dev_counter, 64) != 0 || cuda.zero(pf.dev_counter, 0, 64) != 0 ||
        cuda.host_alloc(&host, (PF_MAX_K + 1) * 4, 2 /* CU_MEMHOSTALLOC_DEVICEMAP */) != 0 ||
        host_device(&pf.dev_host, host, 0) != 0) {
        fputs("Goose CUDA: expert prefetch unavailable\n", stderr);
        return false;
    }
    pf.host_ids = host;
    pf.host_ids[0] = 0;
    pf.state = 1;
    fprintf(stderr, "Goose CUDA: expert prefetch from predicted routes active k=%u\n", pf.k);
    return true;
}

// A dense F32 256 x hidden matrix is a router; first-seen order is the layer order.
static void pf_note_router(uint64_t host, DevicePtr device, int64_t kind, int64_t rows, int64_t row_stride, int64_t bytes) {
    if (kind != 0 || rows != PF_EXPERTS || row_stride <= 0 || bytes != rows * row_stride || !device) return;
    for (int i = 0; i < pf.routers; ++i) if (pf.router_host[i] == host) return;
    if (pf.routers == PF_LAYERS) return;
    pf.router_host[pf.routers] = host;
    pf.router_dev[pf.routers++] = device;
}

static int pf_layer(uint64_t up) {
    for (int i = 0; i < pf.layers; ++i) if (pf.layer_up[i] == up) return i;
    if (pf.layers == PF_LAYERS) return -1;
    pf.layer_up[pf.layers] = up;
    return pf.layers++;
}

static void pf_launch(int layer, DevicePtr input, int64_t width) {
    pf.pending = -1;
    const int next = layer + 1;
    if (!pf_init() || layer < 0 || next >= pf.layers || pf.routers != pf.layers || !pf.plan[next].ok) return;
    const uint64_t t0 = pf_now();
    DevicePtr w = pf.router_dev[next];
    int iw = (int) width, k = (int) pf.k, seq = pf.seq = pf.seq == 0x7fffffff ? 1 : pf.seq + 1;
    void * args[] = {&w, &input, &pf.dev_logits, &pf.dev_counter, &pf.dev_host, &iw, &k, &seq};
    if (cuda.launch(pf.route, PF_EXPERTS, 1, 1, 256, 1, 1, 0, pf.stream, args, NULL) != 0) {
        pf.state = -1;
        fputs("Goose CUDA: expert prefetch disabled after launch failure\n", stderr);
        return;
    }
    pf.pending = next;
    ++pf.launched;
    pf.ns_launch += pf_now() - t0;
}

static int pf_submit(void) {
    const int next = pf.pending;
    pf.pending = -1;
    if (next < 0 || pf.state != 1) return 0;
    const uint64_t t0 = pf_now();
    if (cuda.expert_upload_count != 0) { ++pf.skipped; return 0; }
    // The kernel publishes the sequence number after the ids; a late answer is skipped.
    for (unsigned spin = 0; pf.host_ids[0] != pf.seq; ++spin) {
        if (spin > 2000000) { ++pf.skipped; return 0; }
        __builtin_ia32_pause();
    }
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    const uint64_t t1 = pf_now();
    pf.ns_wait += t1 - t0;
    int64_t heads[PF_MAX_K], offsets[PF_MAX_K];
    const unsigned k = pf.k;
    for (unsigned i = 0; i < k; ++i) {
        const int64_t id = pf.host_ids[1 + i];
        if (id < 0 || id >= PF_EXPERTS) { ++pf.skipped; return 0; }
        unsigned at = i;
        while (at && heads[at - 1] > id) { heads[at] = heads[at - 1]; --at; }
        heads[at] = id;
    }
    const sl_i64 active = {heads, (int64_t) k}, out = {offsets, (int64_t) k};
    for (size_t p = 0; p < 3; ++p) {
        const int64_t ready = prepare_expert_offsets(pf.plan[next].w[p], pf.plan[next].plane[p], active, out, true, true, p != 0);
        if (ready != 1) {
            reset_expert_stage();
            ++pf.skipped;
            return 0;
        }
    }
    const size_t jobs = cuda.expert_upload_count;
    const uint64_t t2 = pf_now();
    pf.ns_prepare += t2 - t1;
    if (jobs == 0) return 0;
    if (cuda.expert_split_planes && pf.plan[next].disjoint)
        cuda.expert_upload_split = expert_down_split(cuda.expert_upload_jobs, jobs,
            pf.plan[next].down_base, pf.plan[next].down_plane, PF_EXPERTS);
    const int status = submit_expert_stage(cuda.experts.one_touch);
    if (status == 0) { ++pf.waves; pf.jobs += jobs; }
    pf.ns_submit += pf_now() - t2;
    if (pf.waves % 10000 == 0)
        fprintf(stderr, "Goose CUDA: expert prefetch launched=%llu waves=%llu jobs=%llu skipped=%llu us launch=%llu wait=%llu prepare=%llu submit=%llu\n",
            (unsigned long long) pf.launched, (unsigned long long) pf.waves,
            (unsigned long long) pf.jobs, (unsigned long long) pf.skipped,
            (unsigned long long) (pf.ns_launch / 1000), (unsigned long long) (pf.ns_wait / 1000),
            (unsigned long long) (pf.ns_prepare / 1000), (unsigned long long) (pf.ns_submit / 1000));
    return status;
}
// ---- end EXP-PF1 ----

int64_t fg_cuda_plane(uint64_t weights, int64_t weight_bytes, int64_t row_stride,
                     int64_t extent, int64_t kind, int64_t rows, sl_i64 jobs) {
    DevicePtr weight = 0;
    if (cuda.dense_ranges.len > 0) {
        const int64_t offset = dw_cuda_dense_offset(cuda.dense_ranges, weights, weight_bytes);
        if (offset >= 0) weight = cuda.dense.address + (uint64_t) offset;
    }
    if (weight) pf_note_router(weights, weight, kind, rows, row_stride, weight_bytes);
    if (!weight && activation_input((sl_u8) {(void *) (uintptr_t) weights, weight_bytes}, &cuda.weight, &weight) != 0) {
        fprintf(stderr, "Goose CUDA: matrix weight transfer failed\n");
        return -1;
    }
    return launch_matrix(weight, row_stride, extent, kind, rows, jobs, 2, 0, 0);
}

int64_t fg_cuda_expert_batch(int64_t weight_bytes, int64_t row_stride,
                            int64_t extent, int64_t kind, int64_t rows, sl_i64 jobs,
                            int64_t shared_rows, int64_t input_stride) {
    const struct device_buffer * bank = &cuda.experts.weights;
    if (!bank->address || weight_bytes <= 0 || (uint64_t) weight_bytes > bank->bytes ||
        jobs.len <= 0 || jobs.len % 3 != 0 || jobs.len / 3 > INT_MAX) return -1;
    for (int64_t i = 2; i < jobs.len; i += 3) {
        const int64_t offset = jobs.data[i];
        if (offset < 0 || (uint64_t) offset > bank->bytes - (uint64_t) weight_bytes) return -1;
    }
    int status = launch_matrix(bank->address, row_stride, extent, kind, rows, jobs, 3, shared_rows, input_stride);
    if (status == 0) status = submit_expert_stage(false);
    if (status == 0) ++cuda_cached_projections;
    return status;
}

uint8_t fg_cuda_routed_graph_enabled(void) {
    return cuda.routed_graph_enabled && cuda.mmvq_graph_cache && cuda.experts.weights.address &&
        activations.active && activations.defer_waits;
}

uint8_t fg_cuda_full_ffn_enabled(void) {
    return cuda.full_ffn_enabled;
}

static struct kernel_node_params ffn_kernel(Function f, unsigned gx, unsigned gy, unsigned threads, void ** args) {
    return (struct kernel_node_params) { .function = f, .grid_x = gx, .grid_y = gy, .grid_z = 1,
        .block_x = threads, .block_y = 1, .block_z = 1, .params = args };
}

static bool dense_weight(uint64_t host, int64_t bytes, DevicePtr * device) {
    if (!cuda.dense.address) return false;
    const int64_t offset = dw_cuda_dense_offset(cuda.dense_ranges, host, bytes);
    if (offset < 0) return false;
    *device = cuda.dense.address + (uint64_t) offset;
    return true;
}

int64_t fg_cuda_routed_graph(sl_i64 plan, sl_i64 glu, sl_i64 norm, sl_i64 shared, int64_t step, int64_t deadline) {
    enum { routes = 8, experts = 256, words = 3 };
    if (!fg_cuda_routed_graph_enabled()) return 0;
    if (plan.len != 10 || glu.len != 26 || norm.len != 22 || (shared.len != 0 && shared.len != 138)) return -1;
    const int64_t width = plan.data[7], hidden = plan.data[8], used = routes;
    if (width <= 0 || width > 8192 || width % 256 || hidden <= 0 || hidden > 8192 || hidden % 256 ||
        (plan.data[9] != 12 && plan.data[9] != 14) ||
        norm.data[4] != experts || norm.data[12] != routes) return -1;
    if (!fg143_routed_layer_matches(cuda.experts.layers, plan)) return 0;
    const bool full = shared.len != 0;
    const int64_t shared_hidden = full ? shared.data[6] : 0;
    int shared_down_kind = full ? (int) shared.data[7] : 0;
    if (full && (!cuda.full_ffn_enabled || shared_hidden <= 0 || shared_hidden > 8192 ||
        shared_hidden % 256 || (shared_down_kind != 12 && shared_down_kind != 14))) return 0;
    int shared_extent = (int) (shared_hidden / 256);
    uint64_t shared_stride = (uint64_t) shared_extent * (shared_down_kind == 12 ? 144 : 210);
    uint64_t router_stride = (uint64_t) width * 4;
    DevicePtr dense_weights[4] = {0};
    if (full) {
        const int64_t up_bytes = (width / 256) * 144 * shared_hidden;
        if (!dense_weight((uint64_t) shared.data[0], up_bytes, &dense_weights[0]) ||
            !dense_weight((uint64_t) shared.data[1], up_bytes, &dense_weights[1]) ||
            !dense_weight((uint64_t) shared.data[2], (int64_t) shared_stride * width, &dense_weights[2]) ||
            !dense_weight((uint64_t) shared.data[3], (int64_t) router_stride, &dense_weights[3])) return 0;
    }
    const uint64_t input_host = (uint64_t) plan.data[3], probs_host = (uint64_t) plan.data[4];
    const uint64_t ids_host = (uint64_t) plan.data[5];
    const uint64_t residual_host = full ? (uint64_t) shared.data[4] : 0;
    const uint64_t out_host = (uint64_t) (full ? shared.data[5] : plan.data[6]);
    const int64_t output_bytes = width * 4;
    if (!activation_device(input_host, output_bytes) || !activation_device(probs_host, experts * 4)) return 0;
    if (full && !activation_device(residual_host, output_bytes)) return 0;
    // A planned host slot can hold the FFN input and then the residual
    // result. Keep its exact device record live until the final add.
    int64_t * reused_input = NULL;
    if (full && out_host == input_host) {
        for (int64_t i = 0; i < activations.count; ++i) {
            int64_t * record = activations.ranges + i * ACTIVATION_WORDS;
            if ((uint64_t) record[0] == input_host && record[1] == output_bytes) {
                reused_input = record;
                break;
            }
        }
        if (!reused_input) return 0;
    }
    decode_split_armed = 0;
    if (fg_cuda_activation_read_address(ids_host, routes * 4) != 0) return -1;
    int32_t ids[routes];
    memcpy(ids, (const void *) (uintptr_t) ids_host, sizeof ids);
    const bool stage_all = full && cuda.expert_decode_overlap && activations.defer_waits;
    const bool compact = stage_all && cuda.compact_moe_ids && !fg143_cpu_prefill();
    const bool compact_weights = compact && cuda.route_weights.address &&
        route_compact_pending.ids_host == ids_host && route_compact_pending.scores_host == probs_host &&
        route_compact_pending.scores_device == activation_device(probs_host, experts * 4) &&
        norm.data[0] == 4 && norm.data[4] == experts && norm.data[12] == routes &&
        (norm.data[21] == 0 || norm.data[21] == 1065353216);
    route_compact_pending.ids_host = 0;
    int64_t heads[experts], offsets[experts];
    unsigned rank[routes];
    if (compact) {
        unsigned order[routes];
        for (unsigned r = 0; r < routes; ++r) {
            const int id = ids[r];
            if (id < 0 || id >= experts) return 0;
            unsigned at = r;
            while (at && heads[at - 1] > id) {
                heads[at] = heads[at - 1];
                order[at] = order[at - 1];
                --at;
            }
            if (at && heads[at - 1] == id) return 0;
            heads[at] = id;
            order[at] = r;
        }
        for (unsigned i = 0; i < routes; ++i) rank[order[i]] = i;
    } else {
        for (size_t e = 0; e < experts; ++e) heads[e] = -1;
        for (size_t r = 0; r < routes; ++r) {
            const int id = ids[r];
            if (id < 0 || id >= experts || heads[id] >= 0) return 0;
            heads[id] = (int64_t) r;
        }
    }
    if (!reused_input && fg_cuda_activation_prepare(out_host, output_bytes, true, false, step, deadline) != 0) return -1;
    const int active = start_context();
    if (active != 1) return active;

    // Both Q4_K projections consume the same packed input. The down pack
    // can reuse it after the two matvecs and SwiGLU have completed.
    int input_extent = (int) (width / 256), down_extent = (int) (hidden / 256);
    int hidden_rows = (int) hidden, output_rows = (int) width;
    int q4 = 12, down_kind = (int) plan.data[9], job_words = words;
    uint64_t stride_up = (uint64_t) input_extent * 144;
    uint64_t stride_down = (uint64_t) down_extent * (down_kind == 12 ? 144 : 210);
    const size_t routed_pack = (size_t) (input_extent > down_extent ? input_extent : down_extent) * 288 * routes;
    const size_t shared_pack = (size_t) shared_extent * 288;
    const size_t packed_bytes = routed_pack > shared_pack ? routed_pack : shared_pack;
    const size_t pack_space = packed_bytes + (stage_all ? shared_pack : 0);
    const size_t middle_bytes = (size_t) hidden * routes * 4;
    const size_t down_bytes = (size_t) width * routes * 4;
    struct {
        int64_t jobs[3][routes * words];
        int64_t config[22];
        int32_t ids[routes];
        int64_t dense_jobs[2];
    } packet;
    _Static_assert(sizeof(packet) % 16 == 0, "routed graph packet alignment");
    const size_t shared_bytes = full ? (size_t) (3 * shared_hidden + 4 * width + 2) * 4 : 0;
    const size_t scratch_bytes = sizeof packet + pack_space + 3 * middle_bytes + down_bytes + routes * 4 + shared_bytes;
    int error = reserve(&cuda.routed_graph_scratch, scratch_bytes);
    if (error == 2) return finish_context() == 0 ? 0 : -1;
    DevicePtr output = reused_input ? cuda.activations.address + (uint64_t) reused_input[2] : 0;
    if (error == 0 && !reused_input) error = activation_output(out_host, output_bytes, &output);
    const DevicePtr input = activation_device(input_host, output_bytes);
    const DevicePtr probs = activation_device(probs_host, experts * 4);
    DevicePtr residual = full ? activation_device(residual_host, output_bytes) : 0;
    if (error == 0 && (!input || !probs || (full && !residual) || (reused_input && input != output))) {
        return finish_context() == 0 ? 0 : -1;
    }
    const int pf_this = compact && stage_all && error == 0 ? pf_layer((uint64_t) plan.data[0]) : -1;
    if (pf_this >= 0) {
        const uint64_t gate_up_bytes = stride_up * hidden * experts, down_all = stride_down * width * experts;
        const uint64_t down_base = (uint64_t) plan.data[2];
        bool disjoint = true;
        for (size_t p = 0; p < 2; ++p) {
            const uint64_t source = (uint64_t) plan.data[p];
            if (source <= down_base ? down_base - source < gate_up_bytes : source - down_base < down_all) disjoint = false;
        }
        pf.plan[pf_this].w[0] = (uint64_t) plan.data[0];
        pf.plan[pf_this].w[1] = (uint64_t) plan.data[1];
        pf.plan[pf_this].w[2] = (uint64_t) plan.data[2];
        pf.plan[pf_this].plane[0] = pf.plan[pf_this].plane[1] = (int64_t) (stride_up * hidden);
        pf.plan[pf_this].plane[2] = (int64_t) (stride_down * width);
        pf.plan[pf_this].down_base = down_base;
        pf.plan[pf_this].down_plane = (size_t) (stride_down * width);
        pf.plan[pf_this].disjoint = disjoint;
        pf.plan[pf_this].ok = true;
        pf_launch(pf_this, input, width);
    }
    if (stage_all && error == 0) error = begin_expert_prepare();
    const sl_i64 active_routes = {heads, compact ? routes : experts};
    const sl_i64 device_offsets = {offsets, active_routes.len};
    for (size_t p = 0; p < 3 && error == 0; ++p) {
        const int64_t plane = (int64_t) (p == 2 ? stride_down * width : stride_up * hidden);
        const uint64_t weight = (uint64_t) plan.data[p];
        const int64_t ready = stage_all ?
            prepare_expert_offsets(weight, plane, active_routes, device_offsets, true, compact, p != 0) :
            fg_cuda_prepare_experts(weight, plane, active_routes, device_offsets);
        if (ready == 0 && p == 0) return finish_context() == 0 ? 0 : -1;
        if (ready != 1 || (!stage_all && cuda.expert_upload_count != 0)) {
            reset_expert_stage();
            error = -1;
            break;
        }
        for (size_t r = 0; r < routes; ++r) {
            const int64_t offset = offsets[compact ? rank[r] : (unsigned)ids[r]];
            if (offset < 0 || (uint64_t) offset > cuda.experts.weights.bytes ||
                (uint64_t) plane > cuda.experts.weights.bytes - (uint64_t) offset) {
                error = -1;
                break;
            }
            packet.jobs[p][r * words] = p == 2 ? (int64_t) r * hidden * 4 : 0;
            packet.jobs[p][r * words + 1] = (int64_t) r * (p == 2 ? width : hidden) * 4;
            packet.jobs[p][r * words + 2] = offset;
        }
    }
    if (stage_all && error == 0) {
        if (cuda.expert_split_planes) {
            const uint64_t gate_up_bytes = stride_up * hidden * experts;
            const uint64_t down_bytes_all = stride_down * width * experts;
            const uint64_t down_base = (uint64_t) plan.data[2];
            bool disjoint = true;
            for (size_t p = 0; p < 2; ++p) {
                const uint64_t source = (uint64_t) plan.data[p];
                if (source <= down_base ? down_base - source < gate_up_bytes
                                        : source - down_base < down_bytes_all) disjoint = false;
            }
            if (disjoint) {
                cuda.expert_upload_split = expert_down_split(cuda.expert_upload_jobs, cuda.expert_upload_count,
                    down_base, (size_t) (stride_down * width), experts);
            }
        }
        error = submit_expert_stage(compact && cuda.experts.one_touch);
    }
    if (error == 0) {
        memcpy(packet.config, norm.data, sizeof packet.config);
        memcpy(packet.ids, ids, sizeof packet.ids);
        packet.dense_jobs[0] = packet.dense_jobs[1] = 0;
        error = upload_packet(cuda.routed_graph_scratch.address, &packet, sizeof packet);
    }
    if (error == 0) {
        const DevicePtr scratch = cuda.routed_graph_scratch.address;
        DevicePtr jobs_up = scratch, jobs_gate = jobs_up + sizeof packet.jobs[0], jobs_down = jobs_gate + sizeof packet.jobs[0];
        DevicePtr norm_cfg = jobs_down + sizeof packet.jobs[0], selected = norm_cfg + sizeof packet.config;
        DevicePtr packed = scratch + sizeof packet;
        DevicePtr packed_shared = stage_all ? packed + packed_bytes : packed;
        DevicePtr up = packed + pack_space, gate = up + middle_bytes;
        DevicePtr activated = gate + middle_bytes, down = activated + middle_bytes, normalized = down + down_bytes;
        DevicePtr bank = cuda.experts.weights.address;
        int64_t vector[26];
        memcpy(vector, glu.data, sizeof vector);
        if (cuda.activation_reference) vector[0] = 15;
        float zero = 0.0f;
        void * pack_up[] = {(void *) &input, &packed, &jobs_up, &input_extent, &job_words};
        void * matrix_up[] = {&bank, &packed, &up, &jobs_up, &stride_up, &input_extent, &q4, &job_words};
        void * matrix_gate[] = {&bank, &packed, &gate, &jobs_gate, &stride_up, &input_extent, &q4, &job_words};
        void * gl[] = {&gate, &up, &activated, vector, &zero, &zero};
        void * pack_down[] = {&activated, &packed, &jobs_down, &down_extent, &job_words};
        void * matrix_down[] = {&bank, &packed, &down, &jobs_down, &stride_down, &down_extent, &down_kind, &job_words};
        void * normalize[] = {(void *) &probs, &selected, &normalized, &norm_cfg};
        DevicePtr routed_out = full ? normalized + routes * 4 : output;
        DevicePtr normalized_input = compact_weights ? cuda.route_weights.address : normalized;
        void * reduce[] = {&down, &normalized_input, &routed_out, (void *) &width, (void *) &used};
        DevicePtr dense_jobs = selected + sizeof packet.ids;
        const uint64_t shared_row_bytes = (uint64_t) shared_hidden * 4;
        DevicePtr sh_up = routed_out + (uint64_t) output_bytes, sh_gate = sh_up + shared_row_bytes;
        DevicePtr sh_glu = sh_gate + shared_row_bytes, sh_down = sh_glu + shared_row_bytes;
        DevicePtr route_score = sh_down + (uint64_t) output_bytes, route_gate = route_score + 4;
        DevicePtr product = route_gate + 4, mixed = product + (uint64_t) output_bytes;
        int dense_words = 2, f32 = 0, first = 0;
        int shared_rows = (int) shared_hidden;
        int64_t plans[5][26];
        if (full) memcpy(plans, shared.data + 8, sizeof plans);
        if (full && cuda.activation_reference) { plans[0][0] = 15; plans[1][0] = 13; }
        void * dense_up[] = {&dense_weights[0], &packed, &sh_up, &dense_jobs, &stride_up, &input_extent, &q4, &dense_words};
        void * dense_gate[] = {&dense_weights[1], &packed, &sh_gate, &dense_jobs, &stride_up, &input_extent, &q4, &dense_words};
        void * dense_glu[] = {&sh_gate, &sh_up, &sh_glu, plans[0], &zero, &zero};
        void * dense_pack[] = {&sh_glu, &packed_shared, &dense_jobs, &shared_extent, &dense_words};
        void * dense_down[] = {&dense_weights[2], &packed_shared, &sh_down, &dense_jobs, &shared_stride,
            &shared_extent, &shared_down_kind, &dense_words};
        void * dense_router[] = {&dense_weights[3], (void *) &input, &route_score, &dense_jobs,
            &router_stride, &output_rows, &f32, &first, &dense_words};
        DevicePtr unused = 0;
        void * sigmoid[] = {&route_score, &unused, &route_gate, plans[1], &zero, &zero};
        void * scale[] = {&sh_down, &route_gate, &product, plans[2], &zero, &zero};
        void * add_shared[] = {&routed_out, &product, &mixed, plans[3], &zero, &zero};
        void * add_residual[] = {&mixed, &residual, &output, plans[4], &zero, &zero};
        const bool fused_tail = full && cuda.moe_add_shared_residual;
        static bool fused_tail_logged;
        if (fused_tail && !fused_tail_logged) {
            fused_tail_logged = true;
            fprintf(stderr, "Goose CUDA: fused shared expert scale and residual adds\n");
        }
        void * fused_add[] = {&routed_out, &sh_down, &route_gate, &residual, &output};
        const bool shared_fused = stage_all && compact && cuda.experts.one_touch &&
            cuda.mmvq_shared_up_gate_glu && width == 2048 && shared_hidden == 512 &&
            plans[0][0] == 15 && plans[0][1] == shared_hidden &&
            plans[0][2] == shared_hidden && plans[0][3] == 1 &&
            plans[0][6] == shared_hidden && plans[0][14] == shared_hidden &&
            plans[0][10] == 4 && plans[0][18] == 4 && plans[0][22] == 0;
        void *shared_fused_args[] = {&dense_weights[0], &dense_weights[1],
            &packed, &sh_glu, &stride_up, &input_extent};
        const size_t shared_tail_count = fused_tail ? 5 : 6;
        struct kernel_node_params kernels[18];
        struct kernel_node_params shared_head[2], shared_tail[6];
        if (full) {
            shared_head[0] = ffn_kernel(cuda.mmvq, (unsigned) shared_rows, 1, 128, dense_up);
            shared_head[1] = ffn_kernel(cuda.mmvq, (unsigned) shared_rows, 1, 128, dense_gate);
            shared_tail[0] = ffn_kernel(cuda.vector, (unsigned) plans[0][24], 1, 128, dense_glu);
            shared_tail[1] = ffn_kernel(cuda.mmvq_pack, 2 * (unsigned) shared_extent, 1, 128, dense_pack);
            shared_tail[2] = ffn_kernel(cuda.mmvq, (unsigned) output_rows, 1, 128, dense_down);
            shared_tail[3] = ffn_kernel(cuda.kernel, 1, 1, 32, dense_router);
            shared_tail[4] = ffn_kernel(cuda.vector, (unsigned) plans[1][24], 1, 128, sigmoid);
            shared_tail[5] = ffn_kernel(cuda.vector, (unsigned) plans[2][24], 1, 128, scale);
        }
        const struct kernel_node_params normalize_routes = ffn_kernel(cuda.route_norm, 1, 1, 32, normalize);
        size_t count = 0;
        kernels[count++] = ffn_kernel(cuda.mmvq_pack, 2 * (unsigned) input_extent, routes, 128, pack_up);
        if (stage_all) {
            // Shared FFN and route normalization do not read the expert bank.
            if (shared_fused) {
                static bool reported;
                if (!reported) {
                    reported = true;
                    fputs("Goose CUDA: shared up/gate/SwiGLU fusion selected\n", stderr);
                }
                kernels[count++] = ffn_kernel(cuda.mmvq_shared_up_gate_glu, shared_rows, 1, 128, shared_fused_args);
            } else {
                memcpy(kernels + count, shared_head, sizeof shared_head);
                count += 2;
            }
            const size_t skip_glu = shared_fused ? 1 : 0;
            memcpy(kernels + count, shared_tail + skip_glu, (shared_tail_count - skip_glu) * sizeof shared_tail[0]);
            count += shared_tail_count - skip_glu;
            if (!compact_weights) kernels[count++] = normalize_routes;
        }
        const size_t expert_wait_index = count;
        const bool fused_up_gate = stage_all && compact && cuda.experts.one_touch && cuda.mmvq_up_gate_glu &&
            width == 2048 && hidden == 512 && vector[0] == 15 && vector[1] == hidden * routes &&
            vector[2] == hidden && vector[3] == routes &&
            vector[6] == hidden && vector[7] == routes && vector[14] == hidden && vector[15] == routes &&
            vector[10] == 4 && vector[11] == hidden * 4 && vector[22] == 0 &&
            vector[18] == 4 && vector[19] == hidden * 4;
        void *fused_glu[] = {&bank, &packed, &activated, &jobs_up, &jobs_gate, &stride_up, &input_extent};
        if (fused_up_gate) {
            static bool reported;
            if (!reported) {
                reported = true;
                fputs("Goose CUDA: routed up/gate/SwiGLU fusion selected\n", stderr);
            }
            kernels[count++] = ffn_kernel(cuda.mmvq_up_gate_glu, hidden_rows, routes, 128, fused_glu);
        } else {
            kernels[count++] = ffn_kernel(cuda.mmvq, (unsigned) hidden_rows, routes, 128, matrix_up);
            kernels[count++] = ffn_kernel(cuda.mmvq, (unsigned) hidden_rows, routes, 128, matrix_gate);
        }
        if (full && !stage_all) {
            memcpy(kernels + count, shared_head, sizeof shared_head);
            count += 2;
        }
        if (!fused_up_gate) kernels[count++] = ffn_kernel(cuda.vector, (unsigned) vector[24], 1, 128, gl);
        kernels[count++] = ffn_kernel(cuda.mmvq_pack, 2 * (unsigned) down_extent, routes, 128, pack_down);
        // The down plane is only read from here on, so its transfer can continue
        // while the gate and up projections above run.
        const size_t expert_down_wait_index = stage_all && cuda.expert_planes_split ? count : 0;
        const bool short_down = cuda.mmvq_short && down_extent <= 2 && output_rows % 4 == 0;
        kernels[count++] = ffn_kernel(short_down ? cuda.mmvq_short : cuda.mmvq,
            (unsigned) (short_down ? output_rows / 4 : output_rows), routes, 128, matrix_down);
        if (!stage_all) kernels[count++] = normalize_routes;
        kernels[count++] = ffn_kernel(cuda.moe_weighted, (unsigned) 1, (unsigned) ((width + 255) / 256), 256, reduce);
        if (full) {
            if (!stage_all) {
                memcpy(kernels + count, shared_tail, shared_tail_count * sizeof shared_tail[0]);
                count += shared_tail_count;
            }
            if (fused_tail) {
                kernels[count++] = ffn_kernel(cuda.moe_add_shared_residual, (unsigned) (width / 128), 1, 128, fused_add);
            } else {
                kernels[count++] = ffn_kernel(cuda.vector, (unsigned) plans[3][24], 1, 128, add_shared);
                kernels[count++] = ffn_kernel(cuda.vector, (unsigned) plans[4][24], 1, 128, add_residual);
            }
        }
        const uint64_t key[MMVQ_GRAPH_KEY_WORDS] = {bank, scratch, input, probs, output,
            (uint64_t) width, (uint64_t) hidden, (uint64_t) vector[0], UINT64_MAX - (unsigned) down_kind, residual,
            dense_weights[0], dense_weights[1], dense_weights[2], dense_weights[3],
            (uint64_t) shared_hidden, (unsigned) shared_down_kind};
        if (full && cuda.decode_graph_enabled) decode_capture.scope = true;
        const GraphExec graph = decode_capture.scope ? NULL : cached_graph_for(key, kernels, count);
        if (stage_all) {
            error = launch_cuda_kernels(kernels, expert_wait_index, NULL);
            if (error == 0) {
                error = cuda.expert_planes_split ? decode_wait_for_expert_planes() : decode_wait_for_experts();
            }
            const size_t middle_end = expert_down_wait_index ? expert_down_wait_index : count;
            if (error == 0) error = launch_cuda_kernels(kernels + expert_wait_index, middle_end - expert_wait_index, NULL);
            if (error == 0 && expert_down_wait_index) {
                error = decode_wait_for_experts();
                if (error == 0) error = launch_cuda_kernels(kernels + expert_down_wait_index, count - expert_down_wait_index, NULL);
            }
            if (error == 0) cuda.expert_upload_queued = false;
            if (error == 0 && cuda.decode_early_ffn && cuda.decode_reuse && full && decode_capture.capturing) {
                static bool reported;
                error = decode_capture_submit();
                if (error == 0 && !reported) {
                    reported = true;
                    fputs("Goose CUDA: early decode FFN submission active\n", stderr);
                }
            }
        } else {
            error = graph ? cuda.graph_launch(graph, cuda.graph_stream) : launch_cuda_kernels(kernels, count, NULL);
        }
        if (error == 0 && graph) {
            if (full) ++cuda_full_ffn_replays;
            else ++cuda_routed_replays;
        }
        if (error && decode_capture.capturing) {
            decode_capture.scope = false;
            (void) decode_capture_discard();
        }
        error = finish_matrix_launches(error);
        if (error == 0 && reused_input) {
            reused_input[3] = 0;
            reused_input[4] = deadline;
        } else if (error == 0) {
            error = activation_publish(out_host, output_bytes, output, true);
        }
    }
    if (error == 0 && pf_this >= 0 && pf.pending >= 0) error = pf_submit();
    else pf.pending = -1;
    if (stage_all && error) reset_expert_stage();
    const int popped = finish_context();
    if (error || popped) {
        fprintf(stderr, "Goose CUDA: routed FFN launch failed (%d)\n", error ? error : popped);
        return -1;
    }
    if (compact) { static int count = -1; if (count < 0) { const char * v = getenv("GOOSE_DECODE_SPLIT_COUNT"); count = v ? atoi(v) : 2; } decode_split_armed = count; }
    if (compact) {
        static bool reported;
        if (!reported) {
            reported = true;
            fputs("Goose CUDA: compact decode routes active\n", stderr);
        }
    }
    if (compact_weights) {
        static _Thread_local uint64_t consumed;
        ++consumed;
        if (consumed == 1 || consumed % 10000 == 0 || consumed == 40880)
            fprintf(stderr, "Goose CUDA: compact route weights consumed=%" PRIu64 "\n", consumed);
    }
    if (full) ++cuda_full_ffn_groups;
    else ++cuda_routed_groups;
    cuda_matrix_launches += full ? 7 : 3;
    cuda_cached_projections += 3;
    cuda_matrices += full ? 7 : 3;
    cuda_f32_matrices += full;
    cuda_vectors += full ? 6 : 1;
    ++cuda_routes[4];
    ++cuda_moe_weighted_tiles;
    return 1;
}

void fg_cuda_take_routed_graph(uint64_t * groups, uint64_t * replays, uint64_t * full, uint64_t * full_replays) {
    *groups = cuda_routed_groups;
    *replays = cuda_routed_replays;
    cuda_routed_groups = cuda_routed_replays = 0;
    *full = cuda_full_ffn_groups;
    *full_replays = cuda_full_ffn_replays;
    cuda_full_ffn_groups = cuda_full_ffn_replays = 0;
}

void fg_cuda_take_matrix_launches(uint64_t * launches, uint64_t * cached) {
    *launches = cuda_matrix_launches;
    *cached = cuda_cached_projections;
    cuda_matrix_launches = 0;
    cuda_cached_projections = 0;
}

uint64_t fg_cuda_take_mmq(void) {
    const uint64_t count = cuda_mmq_projections;
    cuda_mmq_projections = 0;
    return count;
}

void fg_cuda_take_mmq_stream(uint64_t * windows, uint64_t * fixups) {
    *windows = cuda_mmq_stream_windows;
    *fixups = cuda_mmq_fixups;
    cuda_mmq_stream_windows = cuda_mmq_fixups = 0;
}

uint64_t fg_cuda_take_dense_mma(void) {
    const uint64_t count = cuda_dense_mma;
    cuda_dense_mma = 0;
    return count;
}

int64_t fg_cuda_finish(uint64_t output, int64_t output_bytes) {
    int error = 0;
    if (cuda.expert_upload_count != 0) {
        reset_expert_stage();
        error = -1;
    }
    if (error == 0 && output) {
        error = activation_publish(output, output_bytes, matrix_output, true);
    }
    if (error == 0 && output && matrix_glu_active && getenv("GOOSE_GRAPH_LOG"))
        fprintf(stderr, "Goose CUDA: fused SwiGLU Q8_1 down projection\n");
    const int pop_error = finish_context();
    if (error == 0) error = pop_error;
    if (error != 0) {
        fprintf(stderr, "Goose CUDA: output transfer failed (%d)\n", error);
        return -1;
    }
    if (output) {
        ++cuda_matrices;
        if (cuda_matrix_kind == 0) ++cuda_f32_matrices;
    }
    return 0;
}

uint64_t fg_cuda_take_matrices(void) {
    const uint64_t matrices = cuda_matrices;
    cuda_matrices = 0;
    return matrices;
}

uint64_t fg_cuda_take_f32_matrices(void) {
    const uint64_t matrices = cuda_f32_matrices;
    cuda_f32_matrices = 0;
    return matrices;
}

static bool flash_mma_shape(const int64_t * c, int64_t mask_bytes) {
    return cuda.flash_mma && mask_bytes == 0 && c[0] == 256 && c[1] == 256 &&
        (c[2] >= 8 || (cuda.flash_tail_mma && c[2] == 4)) &&
        c[4] > 0 && c[4] <= 65535 && c[5] == c[4] && c[3] == 8 * c[4] &&
        c[6] >= c[2] && c[6] <= INT_MAX - 31 && c[21] > 0 && c[21] <= c[6] - c[2] + 1 &&
        c[7] > 0 && c[7] % 8 == 0 && c[8] > 0 && c[8] % 8 == 0;
}

static bool flash_vec_shape(const int64_t * c, int64_t mask_bytes) {
    return cuda.flash_vec && mask_bytes == 0 && c[0] == 256 && c[1] == 256 && c[2] == 1 &&
        c[3] > 0 && c[3] <= 256 && c[4] > 0 && c[5] == c[4] && c[3] % c[4] == 0 &&
        c[6] > 0 && c[6] <= INT_MAX - 127 && c[21] >= 0 && c[21] <= c[6] && c[14] == 1024;
}

static int capture_flash_graph(const struct kernel_node_params * kernels, size_t count) {
    int error = capture_cuda_graph(kernels, count, &cuda.flash_graph);
    if (error != 0) return error;
    size_t nodes = 1;
    error = cuda.graph_roots(cuda.flash_graph, &cuda.flash_graph_nodes[0], &nodes);
    if (error != 0 || nodes != 1) return error ? error : -1;
    for (size_t i = 1; i < count; ++i) {
        nodes = 1;
        error = cuda.graph_dependents(cuda.flash_graph_nodes[i - 1], &cuda.flash_graph_nodes[i], &nodes);
        if (error != 0 || nodes != 1) return error ? error : -1;
    }
    error = cuda.graph_instantiate(&cuda.flash_graph_exec, cuda.flash_graph, 0);
    if (error == 0) cuda.flash_graph_node_count = count;
    return error;
}

static int launch_flash_chain(const struct kernel_node_params * kernels, size_t count) {
    if (cuda.flash_graph_enabled && !decode_capture.scope) {
        int error = 0;
        if (cuda.flash_graph_exec && cuda.flash_graph_node_count != count) release_flash_graph();
        if (!cuda.flash_graph_exec) {
            error = capture_flash_graph(kernels, count);
        } else {
            for (size_t i = 0; i < count && error == 0; ++i) {
                error = cuda.graph_set_kernel(cuda.flash_graph_exec, cuda.flash_graph_nodes[i], &kernels[i]);
            }
        }
        if (error == 0) return cuda.graph_launch(cuda.flash_graph_exec, cuda.graph_stream);
        if (getenv("GOOSE_GRAPH_LOG")) fprintf(stderr, "Goose CUDA Flash graph: setup failed (%d)\n", error);
        release_flash_graph();
        cuda.flash_graph_enabled = false;
    }
    return launch_cuda_kernels(kernels, count, NULL);
}

static int launch_flash_vec(const int64_t * c, DevicePtr * staged, DevicePtr output, float scale, bool * executed) {
    enum { vector_bytes = 1024, packed_query_bytes = 320, min_prepack_parts = 64 };
    int32_t keys = (int32_t) (c[21] == 0 ? c[6] : c[21]);
    int32_t heads = (int32_t) c[3];
    int32_t parts = (int32_t) fg143_flash_vector_parts(keys);
    DevicePtr matrix = output, meta = 0, query = staged[0];
    bool prepacked = cuda.flash_vec_pack && parts >= min_prepack_parts;
    if (parts > 1) {
        const size_t vectors = (size_t) heads * (size_t) parts;
        const size_t span = vectors * vector_bytes;
        const size_t scratch = span + vectors * 2 * sizeof(float);
        const size_t extra = prepacked ? (size_t) heads * packed_query_bytes : 0;
        int error = reserve(&cuda.attention_scratch, scratch + extra);
        if (error == 2 && prepacked) {
            prepacked = false;
            error = reserve(&cuda.attention_scratch, scratch);
        }
        if (error == 2) return 0;
        if (error != 0) return error;
        matrix = cuda.attention_scratch.address;
        meta = matrix + span;
        if (prepacked) query = matrix + scratch;
    }
    const bool pack_in_graph = prepacked && cuda.flash_graph_enabled && cuda.flash_graph_pack && !decode_capture.scope;
    void * pack_args[] = {&staged[0], &query, &staged[4], &scale};
    const struct kernel_node_params pack = { .function = cuda.flash_vec_pack, .grid_x = (unsigned) heads,
        .grid_y = 1, .grid_z = 1, .block_x = 32, .block_y = 1, .block_z = 1, .params = pack_args };
    if (prepacked && !pack_in_graph) {
        const int error = launch_cuda_kernels(&pack, 1, NULL);
        if (error != 0) return error;
    }
    void * args[] = {&query, &staged[1], &staged[2], &matrix, &meta, &staged[4], &keys, &scale};
    const bool paired = prepacked && cuda.flash_vec_paired && c[4] > 0 && heads % c[4] == 0 &&
        (heads / c[4]) % 2 == 0;
    static bool paired_logged;
    if (paired && !paired_logged) {
        paired_logged = true;
        fprintf(stderr, "Goose CUDA: paired query heads share packed K/V\n");
    }
    const bool quad = paired && cuda.flash_vec_quad && (heads / c[4]) % 4 == 0;
    static bool quad_logged;
    if (quad && !quad_logged) {
        quad_logged = true;
        fprintf(stderr, "Goose CUDA: four query heads share packed K/V\n");
    }
    const bool key_bound = quad && cuda.flash_vec_quad_key_bound &&
        heads == 16 && c[4] == 2 && parts == 64 && keys >= 32768 && keys <= 33279;
    static bool key_bound_logged;
    if (key_bound && !key_bound_logged) {
        key_bound_logged = true;
        fputs("Goose CUDA: SM89 bounded-key Attention selected\n", stderr);
    }
    const Function function = key_bound ? cuda.flash_vec_quad_key_bound :
        quad ? cuda.flash_vec_quad : paired ? cuda.flash_vec_paired :
        prepacked ? cuda.flash_vec_packed : cuda.flash_vec;
    const struct kernel_node_params vector = { .function = function,
        .grid_x = (unsigned) (heads / (quad ? 4 : paired ? 2 : 1)),
        .grid_y = (unsigned) parts, .grid_z = 1, .block_x = 128, .block_y = 1, .block_z = 1, .params = args };
    void * merge_args[] = {&matrix, &meta, &output, &parts, &heads};
    const struct kernel_node_params merge = { .function = cuda.flash_vec_merge, .grid_x = (unsigned) heads,
        .grid_y = 1, .grid_z = 1, .block_x = 256, .block_y = 1, .block_z = 1, .params = merge_args };
    const struct kernel_node_params kernels[] = {pack, vector, merge};
    const int error = parts > 1 ? launch_flash_chain(kernels + (pack_in_graph ? 0 : 1), pack_in_graph ? 3 : 2) :
        launch_cuda_kernels(&vector, 1, NULL);
    if (error == 0) *executed = true;
    else (void) cuda.sync();
    return error;
}

static int launch_flash_mma(const int64_t * c, DevicePtr * staged, DevicePtr output, float scale, bool * executed) {
    int64_t keys = c[6], padded = (keys + 31) & ~INT64_C(31);
    const uint64_t span = (uint64_t) padded * (uint64_t) c[4] * 512;
    if (span > SIZE_MAX / 2) return -1;
    int error = reserve(&cuda.attention_scratch, (size_t) (2 * span));
    if (error == 2) return 0; // The scalar path can use smaller scratch waves.
    if (error != 0) return error;
    const unsigned heads = (unsigned) c[4];
    const unsigned columns = c[2] == 4 ? 4 : 8;
    const unsigned tiles_per_head = (unsigned) ((c[2] + columns - 1) / columns);
    const uint64_t tiles = (uint64_t) tiles_per_head * heads;
    const bool stream = c[2] == 4 ? cuda.flash_tail_mma : cuda.flash_stream_all;
    unsigned workers = stream ?
        (unsigned) fg143_flash_stream_workers(c[2], c[4], keys, cuda.multiprocessors) : 0;
    DevicePtr fix = 0;
    if (workers && tiles % workers != 0) {
        const size_t fix_bytes = (size_t) workers * columns * 8 * (256 + 4) * sizeof(float);
        // Flash and MMQ consume this scratch on the same legacy stream.
        // Reuse the already allocated MMQ pack when it fits.
        error = reserve(&cuda.mmq_packed, fix_bytes);
        if (error == 2) { workers = 0; error = 0; }
        else if (error != 0) return error;
        else fix = cuda.mmq_packed.address;
    }
    DevicePtr keys_half = cuda.attention_scratch.address, values_half = keys_half + span;
    int64_t k_row = c[9], k_head = c[10], v_row = c[11], v_head = c[12];
    void * k_args[] = {&staged[1], &keys_half, &keys, &padded, &k_row, &k_head};
    void * v_args[] = {&staged[2], &values_half, &keys, &padded, &v_row, &v_head};
    error = cuda.launch(cuda.flash_stage, (unsigned) padded, heads, 1, 128, 1, 1, 0, NULL, k_args, NULL);
    if (error == 0) error = cuda.launch(cuda.flash_stage, (unsigned) padded, heads, 1, 128, 1, 1, 0, NULL, v_args, NULL);
    if (error == 0) {
        enum { FLASH_MMA_SHARED_BYTES = 33792 };
        if (workers) {
            void * args[] = {&staged[0], &keys_half, &values_half, &output, &fix, &staged[4], &scale};
            error = cuda.launch(cuda.flash_stream, workers, 1, 1, 128, 1, 1, FLASH_MMA_SHARED_BYTES, NULL, args, NULL);
        } else {
            void * args[] = {&staged[0], &keys_half, &values_half, &output, &staged[4], &scale};
            error = cuda.launch(cuda.flash_mma, tiles_per_head, heads, 1, 128, 1, 1, FLASH_MMA_SHARED_BYTES, NULL, args, NULL);
        }
    }
    if (error == 0 && fix) {
        int32_t queries = (int32_t) c[2], query_heads = (int32_t) c[3];
        if (workers % tiles == 0) {
            int32_t blocks_per_tile = (int32_t) (workers / tiles);
            void * args[] = {&output, &fix, &queries, &query_heads, &workers, &blocks_per_tile};
            error = cuda.launch(cuda.flash_fixup_uniform, (unsigned) tiles, columns, 8, 256, 1, 1, 0, NULL, args, NULL);
        } else {
            int32_t key_count = (int32_t) keys;
            void * args[] = {&output, &fix, &queries, &query_heads, &key_count};
            error = cuda.launch(cuda.flash_fixup_general, workers, columns, 8, 256, 1, 1, 0, NULL, args, NULL);
        }
    }
    if (error == 0) *executed = true;
    else (void) cuda.sync();
    return error;
}

int64_t fg_cuda_q4_attention(sl_i64 config, sl_u8 q, sl_u8 k, sl_u8 v, sl_u8 mask, sl_u8 out, float scale) {
    if (config.len != 22 || config.data[17] <= 0 || config.data[17] > 65535 ||
        config.data[18] <= 0 || config.data[18] > INT_MAX ||
        config.data[19] <= 0 || config.data[19] > config.data[18] || config.data[20] <= 0 ||
        config.data[20] % config.data[19] != 0 || (uint64_t) config.data[20] > SIZE_MAX ||
        q.len <= 0 || k.len <= 0 || v.len <= 0 || mask.len < 0 || out.len <= 0) return -1;
    const int active = start_context();
    if (active != 1) return active;

    DevicePtr key = 0, value = 0;
    int error = map_state_address((uint64_t) (uintptr_t) k.data, k.len, 0, &key);
    if (error == 0) error = map_state_address((uint64_t) (uintptr_t) v.data, v.len, 0, &value);
    const bool resident = key != 0 && value != 0;
    const sl_u8 data[] = {q, key ? (sl_u8) {0} : k, value ? (sl_u8) {0} : v, mask,
        {(uint8_t *) config.data, config.len * (int64_t) sizeof(*config.data)}};
    struct device_buffer * buffers[] = {&cuda.input, &cuda.weight, &cuda.attention_value, &cuda.jobs, &cuda.attention_config};
    DevicePtr staged[] = {0, key, value, 0, 0}, output = 0;
    const uint64_t host = (uint64_t) (uintptr_t) out.data;
    if (error == 0) error = activation_output(host, out.len, &output);
    if (error == 0) error = upload_inputs(data, buffers, staged, sizeof(data) / sizeof(*data));

    bool mma = false;
    if (error == 0 && flash_mma_shape(config.data, mask.len)) {
        error = launch_flash_mma(config.data, staged, output, scale, &mma);
    }
    bool vector = false;
    if (error == 0 && flash_vec_shape(config.data, mask.len)) {
        error = launch_flash_vec(config.data, staged, output, scale, &vector);
    }
    const bool tiled = mma || vector;

    const int tasks = (int) config.data[18];
    const int requested = (int) config.data[19];
    const size_t task_bytes = (size_t) (config.data[20] / requested);
    int capacity = requested;
    if (error == 0 && !tiled) {
        while ((error = reserve(&cuda.attention_scratch, task_bytes * (size_t) capacity)) == 2 &&
               capacity > 1) {
            capacity /= 2;
        }
        if (error == 0 && capacity != requested && getenv("GOOSE_GRAPH_LOG")) {
            fprintf(stderr, "Goose CUDA: attention scratch wave %d of %d tasks\n", capacity, requested);
        }
    }
    const unsigned tiles = (unsigned) config.data[17];
    int start = 0;
    void * tile_args[] = {&staged[0], &staged[1], &staged[2],
        &staged[3], &cuda.attention_scratch.address, &staged[4], &scale, &start};
    void * merge_args[] = {&cuda.attention_scratch.address, &output, &staged[4], &start};
    while (error == 0 && !tiled && start < tasks) {
        const int count = tasks - start < capacity ? tasks - start : capacity;
        error = cuda.launch(cuda.attention_tiles, (unsigned) count, tiles, 1, 32, 1, 1, 0, NULL, tile_args, NULL);
        if (error == 0) error = cuda.launch(cuda.attention_merge, (unsigned) count, 1, 1, 32, 1, 1, 0, NULL, merge_args, NULL);
        start += count;
    }
    if (error == 0) error = activation_publish(host, out.len, output, false);
    if (error != 0 && start > 0) (void) cuda.sync();
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: Q4 attention failed (%d)\n", error ? error : popped);
        return -1;
    }
    ++cuda_attentions;
    if (mma) ++cuda_flash_mmas;
    if (config.data[21] > 0) ++cuda_causal_attentions;
    if (resident) ++cuda_resident_kv;
    return 1;
}

uint64_t fg_cuda_take_attentions(void) {
    const uint64_t attentions = cuda_attentions;
    cuda_attentions = 0;
    return attentions;
}

uint64_t fg_cuda_take_flash_mmas(void) {
    const uint64_t count = cuda_flash_mmas;
    cuda_flash_mmas = 0;
    return count;
}

uint64_t fg_cuda_take_causal_attentions(void) {
    const uint64_t count = cuda_causal_attentions;
    cuda_causal_attentions = 0;
    return count;
}

int64_t fg_cuda_gdn(sl_i64 plan, sl_f32 q, sl_f32 k, sl_f32 v, sl_f32 gate, sl_f32 beta, sl_f32 initial, sl_f32 out) {
    if (plan.len != 50 || plan.data[25] <= 0 || plan.data[25] > INT_MAX ||
        plan.data[27] <= 0 || plan.data[27] > 65535 ||
        plan.data[34] <= 0 || plan.data[34] > 65535 ||
        (plan.data[35] != 16 && plan.data[35] != 32) || plan.data[36] != 4 ||
        plan.data[48] < 0 || plan.data[48] >= out.len ||
        out.len <= 0 || out.len > INT64_MAX / (int64_t) sizeof(float)) return -1;
    const sl_f32 tensors[] = {q, k, v, gate, beta, initial};
    sl_u8 data[6];
    for (size_t i = 0; i < sizeof(tensors) / sizeof(*tensors); ++i) {
        if (!tensors[i].data || tensors[i].len <= 0 ||
            tensors[i].len > INT64_MAX / (int64_t) sizeof(float)) return -1;
        data[i] = (sl_u8) {(uint8_t *) tensors[i].data, tensors[i].len * (int64_t) sizeof(float)};
    }
    const int active = start_context();
    if (active != 1) return active;

    const uint64_t initial_host = (uint64_t) (uintptr_t) initial.data;
    const int64_t heads = plan.data[25], sequences = plan.data[27];
    const bool mma = cuda.gdn_mma && plan.data[24] == 128 && plan.data[44] == 16 &&
        heads >= 16 && heads <= 64 && heads % 16 == 0 && sequences <= INT_MAX / heads &&
        plan.data[26] >= 64 && plan.data[26] <= INT64_MAX - 32 && plan.data[30] == 1 && !plan.data[40];
    const int64_t state_bytes = data[5].len;
    const size_t score_bytes = (size_t) plan.data[48] * sizeof(float);
    const DevicePtr initial_device = state_address(initial_host, state_bytes, 1);
    const bool publish = initial_device && plan.data[49] == 1 && plan.data[30] == 1 &&
        out.len - plan.data[48] == initial.len;
    if (initial_device) data[5] = (sl_u8) {0};
    struct device_buffer * buffers[] = {&cuda.input, &cuda.weight, &cuda.attention_value,
        &cuda.jobs, &cuda.gdn_beta, &cuda.attention_scratch};
    DevicePtr staged[] = {0, 0, 0, 0, 0, initial_device}, output = 0;
    const uint64_t host = (uint64_t) (uintptr_t) out.data;
    const size_t output_bytes = publish ? score_bytes : (size_t) out.len * sizeof(float);
    int error = publish && published_state.bytes > 0 ? -1 : activation_output(host, (int64_t) output_bytes, &output);
    if (error == 0) error = upload_inputs(data, buffers, staged, sizeof(data) / sizeof(*data));

    uint64_t packet[57];
    if (error == 0) {
        for (size_t i = 0; i < sizeof(data) / sizeof(*data); ++i) packet[i] = staged[i];
        packet[6] = output;
        packet[7] = publish ? initial_device : output + score_bytes;
        memcpy(packet + 8, plan.data, sizeof(packet) - 8 * sizeof(*packet));
    }
    void * args[] = {packet};
    if (error == 0) {
        if (mma) {
            const bool sliced = cuda.gdn_mma_v32 && sequences <= INT_MAX / (4 * heads);
            error = cuda.launch(sliced ? cuda.gdn_mma_v32 : cuda.gdn_mma,
                (unsigned) (heads * sequences * (sliced ? 4 : 1)), 1, 1, 256, 1, 1,
                sliced ? GDN_V32_SHARED_BYTES : GDN_MMA_SHARED_BYTES, NULL, args, NULL);
            if (error == 0 && sliced && getenv("GOOSE_GRAPH_LOG"))
                fprintf(stderr, "Goose CUDA GDN: V32 slices\n");
        } else {
            const Function scalar = plan.data[38] && cuda.gdn_decode ? cuda.gdn_decode : cuda.gdn;
            error = cuda.launch(scalar, (unsigned) heads, (unsigned) sequences,
                (unsigned) plan.data[34], (unsigned) (plan.data[35] * plan.data[36]), 1, 1, 0, NULL, args, NULL);
        }
        if (error == 0) {
            error = activation_publish(host, (int64_t) output_bytes, output, false);
            if (error != 0) (void) cuda.sync();
        }
    }
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: GDN failed (%d)\n", error ? error : popped);
        return -1;
    }
    ++cuda_gdns;
    if (mma) ++cuda_gdn_mmas;
    if (publish) {
        published_state.source = (uint64_t) (uintptr_t) out.data + score_bytes;
        published_state.destination = initial_host;
        published_state.bytes = state_bytes;
        ++cuda_resident_gdn;
    }
    return 1;
}

uint64_t fg_cuda_take_gdns(void) {
    const uint64_t gdns = cuda_gdns;
    cuda_gdns = 0;
    return gdns;
}

uint64_t fg_cuda_take_gdn_mmas(void) {
    const uint64_t count = cuda_gdn_mmas;
    cuda_gdn_mmas = 0;
    return count;
}

void fg_cuda_take_resident_state(uint64_t * kv, uint64_t * gdn) {
    *kv = cuda_resident_kv;
    *gdn = cuda_resident_gdn;
    cuda_resident_kv = 0;
    cuda_resident_gdn = 0;
}

uint64_t fg_cuda_take_q4_writes(void) {
    const uint64_t writes = cuda_q4_writes;
    cuda_q4_writes = 0;
    return writes;
}

static int vector_input(sl_u8 data, struct device_buffer * staging, DevicePtr * device,
                        int64_t * shape, bool pack) {
    const uint64_t host = (uint64_t) (uintptr_t) data.data;
    if (!pack || activation_device(host, data.len)) return activation_input(data, staging, device);
    int error = activation_sync_range(host, data.len, false);
    if (error != 0) return error;
    const size_t row_bytes = (size_t) shape[0] * sizeof(float);
    const size_t compact_bytes = row_bytes * (size_t) shape[1] * (size_t) shape[2] * (size_t) shape[3];
    if (compact_bytes > (uint64_t) data.len) return -1;
    uint8_t * compact = malloc(compact_bytes);
    if (!compact) return -1;
    uint8_t * dest = compact;
    for (int64_t i3 = 0; i3 < shape[3]; ++i3) {
        for (int64_t i2 = 0; i2 < shape[2]; ++i2) {
            for (int64_t i1 = 0; i1 < shape[1]; ++i1) {
                memcpy(dest, (const uint8_t *) data.data + i3 * shape[7] +
                    i2 * shape[6] + i1 * shape[5], row_bytes);
                dest += row_bytes;
            }
        }
    }
    error = reserve(staging, compact_bytes);
    if (error == 0) error = cuda.upload(staging->address, compact, compact_bytes);
    free(compact);
    if (error != 0) return error;
    size_t stride = sizeof(float);
    for (int i = 0; i < 4; ++i) {
        shape[4 + i] = (int64_t) stride;
        stride *= (size_t) shape[i];
    }
    *device = staging->address;
    return 0;
}

static bool vector_aligned(uint64_t base, const int64_t * stride) {
    return (base | (uint64_t) stride[1] | (uint64_t) stride[2] | (uint64_t) stride[3]) % 16 == 0;
}

int64_t fg_cuda_vector(sl_i64 config, sl_u8 x, sl_u8 y, sl_u8 out, float param0, float param1) {
    if (config.len != 26 || config.data[24] <= 0 || config.data[24] > INT_MAX ||
        (config.data[25] != 32 && config.data[25] != 128) ||
        x.len <= 0 || y.len < 0 || out.len <= 0) return -1;
    if ((uintptr_t) x.data % sizeof(float) || (uintptr_t) out.data % sizeof(float) ||
        (y.len > 0 && (uintptr_t) y.data % sizeof(float))) return 0;
    const int active = start_context();
    if (active != 1) return active;
    int64_t plan[26];
    memcpy(plan, config.data, sizeof(plan));
    enum { sigmoid_operation = 7, silu_operation = 8, swiglu_operation = 10,
        sigmoid_reference_operation = 13, silu_reference_operation = 14, swiglu_reference_operation = 15 };
    if (cuda.activation_reference) {
        if (plan[0] == sigmoid_operation) plan[0] = sigmoid_reference_operation;
        if (plan[0] == silu_operation) plan[0] = silu_reference_operation;
        if (plan[0] == swiglu_operation) plan[0] = swiglu_reference_operation;
    }
    DevicePtr staged[2] = {0}, output = 0;
    const uint64_t host = (uint64_t) (uintptr_t) out.data;
    int error = 0;
    if (plan[23] & 4) {
        error = activation_reserve_output((size_t) out.len);
        output = cuda.output.address;
    } else {
        error = activation_output(host, out.len, &output);
    }
    if (error == 0) error = vector_input(x, &cuda.input, &staged[0], plan + 6, (plan[23] & 1) != 0);
    if (error == 0 && y.len > 0) {
        const bool pack = (plan[23] & 2) != 0;
        const bool resident = cuda.decode_graph_enabled && !pack &&
            dense_weight((uint64_t) (uintptr_t) y.data, y.len, &staged[1]);
        if (!resident) error = vector_input(y, &cuda.weight, &staged[1], plan + 14, pack);
    }
    if (error == 0) {
        enum { rms_operation = 11, flat_plan = 8 };
        const int64_t width = plan[2];
        const bool reference = cuda.rms_reference && plan[0] == rms_operation && width <= INT_MAX &&
            plan[10] == 4 && (y.len == 0 || plan[18] == 4);
        if (cuda.vector_flat && (plan[23] & flat_plan) && plan[1] >= 16384 && plan[25] == 128) {
            const bool use4 = cuda.vector_flat4 && plan[1] % 4 == 0 &&
                (staged[0] | staged[1] | output) % 16 == 0;
            int64_t work = use4 ? plan[1] / 4 : plan[1];
            const unsigned groups = use4 ? (unsigned) (1 + (work - 1) / 128) : (unsigned) plan[24];
            void * args[] = {&staged[0], &staged[1], &output, &plan[0], &work, &param0, &param1};
            error = cuda.launch(use4 ? cuda.vector_flat4 : cuda.vector_flat, groups, 1, 1, 128, 1, 1, 0, NULL, args, NULL);
            if (error == 0) {
                ++activations.flat_vectors;
                if (use4) ++activations.flat4_vectors;
            }
        } else if (reference) {
            const bool vec = width % 4 == 0 && width <= 16384 && output % 16 == 0 &&
                vector_aligned((uint64_t) (uintptr_t) x.data, config.data + 10) &&
                vector_aligned(staged[0], plan + 10) && (y.len == 0 || (plan[14] == width &&
                    vector_aligned((uint64_t) (uintptr_t) y.data, config.data + 18) && vector_aligned(staged[1], plan + 18)));
            const int32_t vectors = vec ? (width <= 4096 ? 1 : 4) : 0;
            const unsigned lanes = vec ? (width <= 128 ? 32 : width <= 512 ? 128 : width <= 1024 ? 256 :
                width <= 2048 ? 512 : 1024) : (width < 1024 ? 256 : 1024);
            if (vec && width == 2048 && plan[1] == width && plan[24] == 1 && !(plan[23] & 16) &&
                cuda.rms_decode_2048 && !fg143_cpu_prefill()) {
                static bool rms_decode_logged;
                if (!rms_decode_logged) {
                    rms_decode_logged = true;
                    fprintf(stderr, "Goose CUDA: direct single-row RMSNorm for decode\n");
                }
                void *args[] = {&staged[0], &staged[1], &output, &param0};
                error = cuda.launch(cuda.rms_decode_2048, 1, 1, 1, 512, 1, 1, 0, NULL, args, NULL);
            } else {
                void * args[] = {&staged[0], &staged[1], &output, plan, &param0, (void *) &vectors, &param1};
                error = cuda.launch(cuda.rms_reference, (unsigned) plan[24], 1, 1, lanes, 1, 1, 0, NULL, args, NULL);
            }
        } else {
            void * args[] = {&staged[0], &staged[1], &output, plan, &param0, &param1};
            error = cuda.launch(cuda.vector, (unsigned) config.data[24], 1, 1,
                (unsigned) config.data[25], 1, 1, 0, NULL, args, NULL);
        }
        if (error == 0) error = activation_publish(host, out.len, output, false);
    }
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: vector operation failed (%d)\n", error ? error : popped);
        return -1;
    }
    ++cuda_vectors;
    return 1;
}

int64_t fg_cuda_rms_scale(sl_i64 plan, uint64_t input, int64_t input_bytes,
                          uint64_t output, int64_t bytes, int64_t step, int64_t deadline, float eps, float scale) {
    if (!cuda.rms_postscale || !activations.active) return 0;
    if (fg_cuda_activation_prepare(output, bytes, true, false, step, deadline) != 0) return -1;
    const int64_t status = fg_cuda_vector(plan, (sl_u8) {(void *) (uintptr_t) input, input_bytes},
        (sl_u8) {0}, (sl_u8) {(void *) (uintptr_t) output, bytes}, eps, scale);
    if (status > 0 && getenv("GOOSE_GRAPH_LOG")) fprintf(stderr, "Goose CUDA: fused RMS and scale\n");
    return status;
}

uint64_t fg_cuda_take_vectors(void) {
    const uint64_t vectors = cuda_vectors;
    cuda_vectors = 0;
    return vectors;
}

bool fg_cuda_direct_ssm_enabled(void) {
    return cuda.ssm_conv_direct && cuda.ssm_conv_save_tail && activations.active && activations.defer_waits;
}

int64_t fg_cuda_direct_ssm(sl_i64 plan, int64_t step, int64_t deadline) {
    if (plan.len != 7 || !fg_cuda_direct_ssm_enabled()) return 0;
    const uint64_t history = (uint64_t) plan.data[0], input = (uint64_t) plan.data[1];
    const uint64_t weights = (uint64_t) plan.data[2], output = (uint64_t) plan.data[3];
    uint64_t channels = (uint64_t) plan.data[4], tokens = (uint64_t) plan.data[5];
    int32_t silu = (int32_t) plan.data[6];
    const int64_t bytes = (int64_t) (4 * channels * tokens);
    DevicePtr state = state_address(history, (int64_t) (12 * channels), 2);
    if (!state) return 0;
    // The graph can reuse the projection's host slot for the convolution.
    // Keep the old device record until the convolution and tail finish reading.
    const bool reused = input == output;
    if (reused) activation_prepare_target(output, bytes, true, step, deadline);
    else if (fg_cuda_activation_prepare(output, bytes, true, false, step, deadline) != 0) return -1;
    const int active = start_context();
    if (active != 1) return active;
    DevicePtr result = 0, mixed = 0, filter = 0;
    const sl_u8 projection = {(void *) (uintptr_t) input, bytes};
    int error = activation_output_for_input(output, bytes, &result, projection, &mixed);
    if (error == 0 && !mixed) error = activation_input(projection, &cuda.input, &mixed);
    const int64_t filter_bytes = (int64_t) (16 * channels);
    if (error == 0 && !dense_weight(weights, filter_bytes, &filter))
        error = activation_input((sl_u8) {(void *) (uintptr_t) weights, filter_bytes}, &cuda.weight, &filter);
    if (error == 0) {
        const unsigned groups = (unsigned) ((channels + 127) / 128);
        void *conv[] = {&state, &mixed, &filter, &result, &channels, &tokens, &silu};
        void *tail[] = {&state, &mixed, &channels, &tokens};
        error = cuda.launch(cuda.ssm_conv_direct, groups, (unsigned) ((tokens + 31) / 32), 1,
            128, 1, 1, 0, NULL, conv, NULL);
        if (error == 0) error = cuda.launch(cuda.ssm_conv_save_tail, groups, 1, 1, 128, 1, 1, 0, NULL, tail, NULL);
        if (error == 0 && reused) {
            fg143_activation_discard_overwritten(activation_ranges(), output, bytes);
            fg143_activation_discard_overwritten(activation_large_range(), output, bytes);
        }
        if (error == 0) error = activation_publish(output, bytes, result, false);
    }
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: direct SSM failed (%d)\n", error ? error : popped);
        return -1;
    }
    ++cuda_convs;
    if (getenv("GOOSE_GRAPH_LOG")) fprintf(stderr, "Goose CUDA: direct SSM and ordered tail (host reuse=%d)\n", reused);
    return 1;
}

int64_t fg_cuda_position(sl_i64 config, sl_u8 x, sl_u8 y, sl_u8 z, sl_u8 out) {
    enum { rope_operation = 0, conv_operation = 1, rope_decode_operation = 2 };
    if (config.len != 27 || config.data[0] < rope_operation || config.data[0] > rope_decode_operation ||
        config.data[1] <= 0 || config.data[1] > INT_MAX ||
        config.data[2] <= 0 || config.data[2] > 65535 ||
        x.len <= 0 || y.len <= 0 || z.len < 0 || out.len <= 0) return -1;
    const bool rope = config.data[0] != conv_operation;
    if (rope && (config.data[3] <= 0 || config.data[3] > INT_MAX || config.data[4] <= 0)) return -1;
    const int active = start_context();
    if (active != 1) return active;
    DevicePtr staged[4] = {0}, output = 0;
    const bool resident = cuda.decode_graph_enabled && !rope &&
        dense_weight((uint64_t) (uintptr_t) y.data, y.len, &staged[1]);
    const sl_u8 data[] = {x, resident ? (sl_u8) {0} : y, z,
        {(uint8_t *) config.data, config.len * (int64_t) sizeof(*config.data)}};
    struct device_buffer * buffers[] = {&cuda.input, &cuda.weight, &cuda.jobs, &cuda.attention_config};
    const uint64_t host = (uint64_t) (uintptr_t) out.data;
    int error = activation_output(host, out.len, &output);
    const bool rope_decode = config.data[0] == rope_decode_operation && cuda.rope_decode;
    if (error == 0 && rope && !rope_decode) error = reserve(&cuda.attention_scratch, (size_t) config.data[4]);
    if (error == 0) error = upload_inputs(data, buffers, staged, sizeof(data) / sizeof(*data));
    const unsigned gx = (unsigned) config.data[1], gy = (unsigned) config.data[2];
    if (error == 0) {
        if (rope_decode) {
            const unsigned lanes = ((unsigned) config.data[5] + 63) / 64 * 32;
            void * args[] = {&staged[0], &staged[1], &staged[2], &output, &staged[3]};
            error = cuda.launch(cuda.rope_decode, (unsigned) config.data[6], 1, 1, lanes, 1, 1, 0, NULL, args, NULL);
        } else if (rope) {
            void * phase_args[] = {&staged[1], &staged[2], &cuda.attention_scratch.address, &staged[3]};
            void * apply_args[] = {&staged[0], &output, &cuda.attention_scratch.address, &staged[3]};
            error = cuda.launch(cuda.rope_phases, (unsigned) config.data[3], 1, 1, 128, 1, 1, 0, NULL, phase_args, NULL);
            if (error == 0) error = cuda.launch(cuda.rope_apply, gx, 1, 1, 128, 1, 1, 0, NULL, apply_args, NULL);
        } else {
            void * args[] = {&staged[0], &staged[1], &output, &staged[3]};
            enum { taps = 4, tile = 32, channels = 128, grid_z_limit = 65535 };
            const int64_t tokens = config.data[7];
            const bool tiled = cuda.ssm_conv_long && config.data[6] == taps && config.data[5] % channels == 0 &&
                tokens > tile && tokens <= (int64_t) tile * grid_z_limit;
            const unsigned gz = tiled ? 1 + (unsigned) (tokens - 1) / tile : 1;
            const bool decode = cuda.ssm_conv_decode && config.data[6] == taps &&
                tokens == 1 && config.data[8] == sizeof(float) && config.data[12] == sizeof(float);
            Function conv = decode ? cuda.ssm_conv_decode : tiled ? cuda.ssm_conv_long : cuda.ssm_conv;
            error = cuda.launch(conv, gx, gy, gz, channels, 1, 1, 0, NULL, args, NULL);
        }
        if (error == 0) error = activation_publish(host, out.len, output, false);
        if (error != 0) (void) cuda.sync();
    }
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: %s failed (%d)\n", rope ? "RoPE" : "SSM convolution", error ? error : popped);
        return -1;
    }
    if (rope) ++cuda_ropes;
    else ++cuda_convs;
    return 1;
}

void fg_cuda_take_position(uint64_t * ropes, uint64_t * convs) {
    *ropes = cuda_ropes;
    *convs = cuda_convs;
    cuda_ropes = 0;
    cuda_convs = 0;
}

int64_t fg_cuda_giga_dsa_top_wide(sl_u8 scores, sl_u8 ids, int64_t cells, int64_t rows) {
    (void) scores;
    (void) ids;
    (void) cells;
    (void) rows;
    return 0;
}

int64_t fg_cuda_routing(sl_i64 config, sl_u8 x, sl_u8 y, sl_u8 out) {
    if (config.len != 22 || config.data[0] < 0 || config.data[0] >= 5 ||
        config.data[1] <= 0 || config.data[1] > INT_MAX ||
        (config.data[2] != 32 && config.data[2] != 128) || x.len <= 0 || y.len < 0 || out.len <= 0) return -1;
    const int active = start_context();
    if (active != 1) return active;
    const int64_t op = config.data[0];
    const bool compact = op == 1 && config.data[3] != 0;
    const bool topk = compact && cuda.moe_topk;
    if (op == 1) route_compact_pending.ids_host = 0;
    float scale = 0.0f;
    if (op == 0) {
        const uint32_t bits = (uint32_t)config.data[21];
        memcpy(&scale, &bits, sizeof scale);
    }
    const bool fixed_softmax = op == 0 && cuda.route_softmax_fixed && activations.retire &&
        cuda.decode_reuse && !fg143_cpu_prefill() && x.len == 1024 && y.len == 0 && out.len == 1024 &&
        config.data[1] == 1 && config.data[2] == 32 && config.data[4] == 256 &&
        config.data[5] == 1 && config.data[6] == 1 && config.data[7] == 1 &&
        config.data[8] == 4 && config.data[12] == 256 &&
        scale > 0.0f && scale <= 1.0f;
    bool compact_weights = topk && cuda.route_compact8 && activations.retire && cuda.decode_reuse &&
        !fg143_cpu_prefill() && x.len == 1024 && y.len == 0 && out.len == 32 &&
        config.data[4] == 256 && config.data[5] == 1 && config.data[6] == 1 &&
        config.data[7] == 1 && config.data[8] == 4 && config.data[12] == 8;
    const int64_t groups = compact && !topk ? config.data[5] : config.data[1];
    const Function operations[] = {cuda.route_reduce, topk ? cuda.route_topk : cuda.route_sort,
        cuda.route_gather, cuda.route_reduce, cuda.route_norm};
    const sl_u8 data[] = {x, y, {(uint8_t *) config.data, config.len * (int64_t) sizeof(*config.data)}};
    struct device_buffer * buffers[] = {&cuda.input, &cuda.weight, &cuda.attention_config};
    DevicePtr staged[3] = {0}, output = 0;
    const uint64_t host = (uint64_t) (uintptr_t) out.data;
    int error = activation_output(host, out.len, &output);
    if (error == 0) error = upload_inputs(data, buffers, staged, sizeof(data) / sizeof(*data));
    if (error == 0) {
        void * args[] = {&staged[0], &staged[1], &output, &staged[2]};
        if (compact_weights && !cuda.route_weights.address && reserve(&cuda.route_weights, 32) != 0)
            compact_weights = false;
        if (fixed_softmax) {
            void *softmax_args[] = {&staged[0], &output, &scale};
            error = cuda.launch(cuda.route_softmax_fixed, 1, 1, 1, 32, 1, 1, 0, NULL, softmax_args, NULL);
            static _Thread_local uint64_t launches;
            if (error == 0 && (++launches == 1 || launches == 40880))
                fprintf(stderr, "Goose CUDA: unrolled router softmax launches=%" PRIu64 "\n", launches);
        } else if (compact_weights) {
            float default_scale = 1.0f;
            void * compact_args[] = {&staged[0], &output, &cuda.route_weights.address, &default_scale};
            error = cuda.launch(cuda.route_compact8, 1, 1, 1, 32, 1, 1, 0, NULL, compact_args, NULL);
        } else {
            error = cuda.launch(operations[op], (unsigned) groups, 1, 1,
                (unsigned) config.data[2], 1, 1, 0, NULL, args, NULL);
        }
        if (error == 0) error = activation_publish(host, out.len, output, false);
        if (compact_weights && error == 0) {
            route_compact_pending.ids_host = host;
            route_compact_pending.scores_host = (uint64_t)(uintptr_t)x.data;
            route_compact_pending.scores_device = staged[0];
        }
    }
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: routing operation %lld failed (%d)\n", (long long) op, error ? error : popped);
        return -1;
    }
    ++cuda_routes[op];
    return 1;
}

void fg_cuda_take_routes(uint64_t * routes) {
    memcpy(routes, cuda_routes, sizeof(cuda_routes));
    memset(cuda_routes, 0, sizeof(cuda_routes));
}

int64_t fg_cuda_blit(sl_i64 config, sl_u8 x, sl_u8 y, sl_u8 out) {
    if (config.len != 17 || config.data[0] < 0 || config.data[0] >= 3 ||
        config.data[1] <= 0 || config.data[1] > INT_MAX || x.len <= 0 || y.len < 0 || out.len <= 0) return -1;
    const int64_t mode = config.data[0];
    const uint64_t host_x = (uint64_t) (uintptr_t) x.data;
    const uint64_t host_y = (uint64_t) (uintptr_t) y.data;
    const uint64_t host_out = (uint64_t) (uintptr_t) out.data;
    DevicePtr lhs = state_address(host_x, x.len, 2);
    DevicePtr rhs = state_address(host_y, y.len, 2);
    DevicePtr dest = mode == 2 ? state_address(host_out, out.len, 2) : 0;
    if (mode == 2 && !dest) return 0;
    if (!lhs && !rhs && !activation_device(host_x, x.len) && !activation_device(host_y, y.len)) return 0;
    const int active = start_context();
    if (active != 1) return active;

    int error = dest ? 0 : activation_output(host_out, out.len, &dest);
    const sl_u8 data[] = {lhs ? (sl_u8) {0} : x, rhs ? (sl_u8) {0} : y,
        {(uint8_t *) config.data, config.len * (int64_t) sizeof(*config.data)}};
    struct device_buffer * buffers[] = {&cuda.input, &cuda.weight, &cuda.attention_config};
    DevicePtr staged[] = {lhs, rhs, 0};
    if (error == 0) error = upload_inputs(data, buffers, staged, sizeof(data) / sizeof(*data));
    if (error == 0) {
        void * args[] = {&staged[0], &staged[1], &dest, &staged[2]};
        const int64_t prefix = config.data[4], width = config.data[5], channels = config.data[6];
        const bool tiled = cuda.blit_transpose && mode == 0 && config.data[3] == 0 &&
            prefix > 0 && prefix <= 8 && width - prefix >= 32 && width - prefix <= 32ll * 65535 &&
            channels >= 32 && channels <= INT_MAX && config.data[7] == 1 && config.data[8] == 1 &&
            config.data[9] == 4 && config.data[10] >= 4 * prefix && config.data[10] % 4 == 0 &&
            config.data[14] == 4 && config.data[13] >= 4 * channels && config.data[13] % 4 == 0;
        if (tiled) {
            error = cuda.launch(cuda.blit_transpose, (unsigned) ((channels + 31) / 32),
                (unsigned) ((width - prefix + 31) / 32), 1, 256, 1, 1, 0, NULL, args, NULL);
            if (error == 0 && getenv("GOOSE_GRAPH_LOG"))
                fprintf(stderr, "Goose CUDA: tiled transpose concat\n");
        } else {
            error = cuda.launch(cuda.blit, (unsigned) config.data[1], 1, 1, 128, 1, 1, 0, NULL, args, NULL);
        }
        if (error == 0) {
            error = mode == 2 ? finish_device_state_write() : activation_publish(host_out, out.len, dest, false);
        }
    }
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: tensor transform %lld failed (%d)\n", (long long) mode, error ? error : popped);
        return -1;
    }
    ++cuda_blits[mode];
    return 1;
}

void fg_cuda_take_blits(uint64_t * blits) {
    memcpy(blits, cuda_blits, sizeof(cuda_blits));
    memset(cuda_blits, 0, sizeof(cuda_blits));
}

int64_t fg_cuda_fwht(sl_u8 src, sl_u8 dst, int32_t width, int64_t rows, float scale) {
    if (width > 512) return 0;
    const int64_t blocks = (rows + 3) / 4;
    if (blocks <= 0 || blocks > INT_MAX) return -1;
    const int active = start_context();
    if (active != 1) return active;
    DevicePtr input = 0, output = 0;
    const uint64_t host = (uint64_t) (uintptr_t) dst.data;
    int error = activation_output(host, dst.len, &output);
    if (error == 0) error = activation_input(src, &cuda.input, &input);
    if (error == 0) {
        void * args[] = {&input, &output, &width, &rows, &scale};
        error = cuda.launch(cuda.fwht, (unsigned) blocks, 1, 1, 128, 1, 1, 0, NULL, args, NULL);
        if (error == 0) error = activation_publish(host, dst.len, output, false);
    }
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: KV FWHT failed (%d)\n", error ? error : popped);
        return -1;
    }
    ++cuda_fwhts;
    return 1;
}

uint64_t fg_cuda_take_fwhts(void) {
    const uint64_t count = cuda_fwhts;
    cuda_fwhts = 0;
    return count;
}

int64_t fg_cuda_moe_weighted(sl_u8 x, sl_u8 weights, sl_u8 dst, int64_t width, int64_t used,
                            int64_t count, int64_t tile) {
    const int active = start_context();
    if (active != 1) return active;
    const int64_t x_row = x.len / count, w_row = weights.len / count, out_row = dst.len / count;
    const unsigned columns = (unsigned) ((width + 255) / 256);
    const uint64_t host = (uint64_t) (uintptr_t) dst.data;
    DevicePtr output = 0;
    int error = activation_output(host, dst.len, &output);
    for (int64_t first = 0; first < count && error == 0; first += tile) {
        const int64_t tokens = count - first < tile ? count - first : tile;
        DevicePtr input = 0, scale = 0, dest = output + (uint64_t) (first * out_row);
        error = activation_input((sl_u8) {x.data + first * x_row, tokens * x_row}, &cuda.input, &input);
        if (error == 0) {
            error = activation_input((sl_u8) {weights.data + first * w_row, tokens * w_row}, &cuda.weight, &scale);
        }
        if (error == 0) {
            void * args[] = {&input, &scale, &dest, &width, &used};
            error = cuda.launch(cuda.moe_weighted, (unsigned) tokens, columns, 1, 256, 1, 1, 0, NULL, args, NULL);
            if (error == 0) ++cuda_moe_weighted_tiles;
        }
    }
    if (error == 0) error = activation_publish(host, dst.len, output, false);
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: MoE weighted reduction failed (%d)\n", error ? error : popped);
        return -1;
    }
    return 1;
}

uint64_t fg_cuda_take_moe_weighted(void) {
    const uint64_t count = cuda_moe_weighted_tiles;
    cuda_moe_weighted_tiles = 0;
    return count;
}
