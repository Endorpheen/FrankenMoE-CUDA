#include "cpu.h"
#include "goose-cuda-image.h"

#include <dlfcn.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
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
extern void ec_reset(sl_i64 groups, sl_i64 layers, uint64_t words32, sl_i64 words64);
extern int64_t ec_prepare(sl_i64 groups, sl_i64 layers, uint64_t words32, sl_i64 words64,
    uint64_t tensor, int64_t bytes, sl_i64 heads, sl_i64 result);
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

struct device_buffer {
    DevicePtr address;
    size_t bytes;
    struct vmm_mapping * mappings;
    size_t reservation_bytes;
    bool vmm_failed;
};

struct expert_cache {
    struct device_buffer weights;
    sl_i64 groups, layers, work;
    int32_t * state;
    uint64_t hits, fills, evictions, uploaded_bytes;
};

struct expert_upload_job {
    DevicePtr destination;
    const void * source;
    size_t bytes;
};

static struct {
    void * library;
    Context context;
    Module module;
    Function kernel, attention_tiles, attention_merge, gdn, q4_rows, vector, rms_reference;
    Function mmq_pack, mmq_pack_shared, mmq_matrix, mmq_q4_matrix, mmq_q6_matrix;
    Function mmq_q4_dense_shared, mmq_q4_routed_shared;
    Function mmq_q4_dense_x64, mmq_q4_routed_x64;
    Function mmq_q6_dense_shared, mmq_q6_routed_shared;
    Function mmq_q6_dense_x64, mmq_q6_routed_x64;
    Function mmvq_pack, mmvq;
    Function dense_mma, dense_mma_reference, gdn_mma, flash_mma, flash_stage, flash_vec, flash_vec_merge;
    Function flash_vec_pack, flash_vec_packed;
    Function mmq_stream, mmq_stream_fixup;
    Function flash_stream, flash_fixup_uniform, flash_fixup_general;
    Function rope_phases, rope_apply, ssm_conv, ssm_conv_long;
    Function route_reduce, route_sort, route_topk, route_gather, route_norm, blit, fwht, moe_weighted;
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
    Event expert_slots_ready, expert_uploads_ready;
    bool expert_upload_queued;
    struct expert_upload_job * expert_upload_jobs;
    size_t expert_upload_count, expert_upload_capacity;
    sl_i64 expert_pins;
    int64_t expert_pinned_bytes;
    int status, multiprocessors;
    bool mmq_window, mmq_shared, moe_topk, activation_reference;
    bool mmq_align_window;
    size_t vmm_granularity, vmm_capacity;
    bool scratch_vmm, lazy_kv;
    struct memory_properties vmm_properties;
    bool flash_graph_enabled;
    Stream flash_graph_stream;
    Graph flash_graph;
    GraphExec flash_graph_exec;
    GraphNode flash_graph_root, flash_graph_merge;
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
    int (*graph_begin)(Stream, int);
    int (*graph_end)(Stream, Graph *);
    int (*graph_roots)(Graph, GraphNode *, size_t *);
    int (*graph_dependents)(GraphNode, GraphNode *, size_t *);
    int (*graph_instantiate)(GraphExec *, Graph, unsigned long long);
    int (*graph_set_kernel)(GraphExec, GraphNode, const struct kernel_node_params *);
    int (*graph_launch)(GraphExec, Stream);
    int (*graph_destroy)(Graph);
    int (*graph_exec_destroy)(GraphExec);
    int (*host_register)(void *, size_t, unsigned);
    int (*host_unregister)(void *);
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

static pthread_once_t cuda_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t cuda_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t activation_lock = PTHREAD_MUTEX_INITIALIZER;
enum { ACTIVATION_WORDS = 5, ACTIVATION_RECORDS = 256 };
enum { GDN_MMA_SHARED_BYTES = 93312 };
static _Thread_local struct {
    bool active, unavailable, defer_waits, pending;
    bool retire, reuse, selective_eviction;
    uint64_t target;
    int64_t target_bytes, target_deadline, step, count, used;
    int64_t ranges[ACTIVATION_RECORDS * ACTIVATION_WORDS];
    int64_t large_output[ACTIVATION_WORDS];
    uint64_t hits, skipped_download_bytes, expired_bytes;
    size_t packet_used;
} activations;
static _Thread_local DevicePtr matrix_input, matrix_output;
static _Thread_local uint64_t cuda_matrices;
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

static void init_flash_graph(void) {
    const char * setting = getenv("GOOSE_CUDA_GRAPH");
    if (!setting || strcmp(setting, "1") != 0 || !cuda.flash_vec) return;
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
    cuda.flash_graph_enabled = true;
}

static void release_flash_graph(void) {
    if (cuda.flash_graph_exec) cuda.graph_exec_destroy(cuda.flash_graph_exec);
    if (cuda.flash_graph) cuda.graph_destroy(cuda.flash_graph);
    if (cuda.flash_graph_stream) cuda.stream_destroy(cuda.flash_graph_stream);
    cuda.flash_graph_exec = NULL;
    cuda.flash_graph = NULL;
    cuda.flash_graph_stream = NULL;
    cuda.flash_graph_enabled = false;
}

static void release_expert_stream(void) {
    if (cuda.expert_stream) cuda.stream_destroy(cuda.expert_stream);
    if (cuda.expert_uploads_ready) cuda.event_destroy(cuda.expert_uploads_ready);
    if (cuda.expert_slots_ready) cuda.event_destroy(cuda.expert_slots_ready);
    cuda.expert_stream = NULL;
    cuda.expert_uploads_ready = NULL;
    cuda.expert_slots_ready = NULL;
}

static void init_expert_stream(void) {
    enum { nonblocking_stream = 1, disable_timing = 2 };
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
    int error = cuda.stream_create(&cuda.expert_stream, nonblocking_stream);
    if (error == 0) error = cuda.event_create(&cuda.expert_slots_ready, disable_timing);
    if (error == 0) error = cuda.event_create(&cuda.expert_uploads_ready, disable_timing);
    if (error != 0) release_expert_stream();
}

static void cleanup(void) {
    if (cuda.context) {
        if (cuda.push(cuda.context) == 0) {
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
    cuda.mmq_shared = fg_standalone_option_enabled("GOOSE_MMQ_SHARED");
    cuda.moe_topk = fg_standalone_option_enabled("GOOSE_MOE_TOPK");
    const char * setting = getenv("GOOSE_MMQ");
    const char * stream_setting = getenv("GOOSE_MMQ_STREAM_K");
    const bool stream_k = stream_setting && strcmp(stream_setting, "1") == 0;
    const bool mmq = stream_k || (setting && strcmp(setting, "1") == 0);
    const bool mmvq_requested = fg_standalone_option_enabled("GOOSE_MMVQ");
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
    const bool flash_stream = flash_mma && flash_stream_setting && strcmp(flash_stream_setting, "1") == 0;
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
        if (error == 0 && mmq) error = cuda.function_get(&cuda.mmq_matrix, cuda.module, "goose_mmq_matmul");
        if (error == 0 && mmvq) error = cuda.function_get(&cuda.mmvq_pack, cuda.module, "goose_mmvq_quantize");
        if (error == 0 && mmvq) error = cuda.function_get(&cuda.mmvq, cuda.module, "goose_mmvq_matvec");
        if (error == 0 && reference_arch && q4_reference) {
            error = cuda.function_get(&cuda.mmq_q4_matrix, cuda.module, "goose_mmq_q4_reference");
            if (error == 0 && q4_shared)
                error = cuda.function_get(&cuda.mmq_q4_dense_shared, cuda.module, "goose_mmq_q4_dense_shared");
            if (error == 0 && q4_routed_shared)
                error = cuda.function_get(&cuda.mmq_q4_routed_shared, cuda.module, "goose_mmq_q4_routed_shared");
            if (error == 0 && q4_x64 && q4_shared)
                error = cuda.function_get(&cuda.mmq_q4_dense_x64, cuda.module, "goose_mmq_q4_dense_x64");
            if (error == 0 && q4_x64 && q4_routed_shared)
                error = cuda.function_get(&cuda.mmq_q4_routed_x64, cuda.module, "goose_mmq_q4_routed_x64");
        }
        if (error == 0 && reference_arch && q6_reference) {
            error = cuda.function_get(&cuda.mmq_q6_matrix, cuda.module, "goose_mmq_q6_reference");
            if (error == 0 && q6_shared)
                error = cuda.function_get(&cuda.mmq_q6_dense_shared, cuda.module, "goose_mmq_q6_dense_shared");
            if (error == 0 && q6_shared)
                error = cuda.function_get(&cuda.mmq_q6_routed_shared, cuda.module, "goose_mmq_q6_routed_shared");
            if (error == 0 && q6_shared && q6_x64)
                error = cuda.function_get(&cuda.mmq_q6_dense_x64, cuda.module, "goose_mmq_q6_dense_x64");
            if (error == 0 && q6_shared && q6_x64)
                error = cuda.function_get(&cuda.mmq_q6_routed_x64, cuda.module, "goose_mmq_q6_routed_x64");
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
        if (error == 0 && flash_vec && vec_pack)
            error = cuda.function_get(&cuda.flash_vec_packed, cuda.module, "goose_flash_vec_q4_packed");
        if (error == 0) init_flash_graph();
        if (error == 0) error = cuda.function_get(&cuda.gdn, cuda.module, "goose_gated_delta_net");
        if (error == 0 && gdn_mma) {
            enum { max_dynamic_shared_size = 8 };
            error = cuda.function_get(&cuda.gdn_mma, cuda.module, "goose_gdn_mma");
            if (error == 0) error = cuda.function_set_attribute(cuda.gdn_mma, max_dynamic_shared_size, GDN_MMA_SHARED_BYTES);
        }
        if (error == 0) error = cuda.function_get(&cuda.q4_rows, cuda.module, "goose_set_q4_rows");
        if (error == 0) error = cuda.function_get(&cuda.vector, cuda.module, "goose_vector_ops");
        if (error == 0 && rms_reference) error = cuda.function_get(&cuda.rms_reference, cuda.module, "goose_rms_reference");
        if (error == 0) error = cuda.function_get(&cuda.rope_phases, cuda.module, "goose_rope_phases");
        if (error == 0) error = cuda.function_get(&cuda.rope_apply, cuda.module, "goose_rope_apply");
        if (error == 0) error = cuda.function_get(&cuda.ssm_conv, cuda.module, "goose_ssm_conv");
        if (error == 0 && ssm_reference) {
            error = cuda.function_get(&cuda.ssm_conv_long, cuda.module, "goose_ssm_conv_long");
        }
        if (error == 0) error = cuda.function_get(&cuda.route_reduce, cuda.module, "goose_route_reduce");
        if (error == 0) error = cuda.function_get(&cuda.route_sort, cuda.module, "goose_route_sort");
        if (error == 0) error = cuda.function_get(&cuda.route_topk, cuda.module, "goose_route_topk");
        if (error == 0) error = cuda.function_get(&cuda.route_gather, cuda.module, "goose_route_gather");
        if (error == 0) error = cuda.function_get(&cuda.route_norm, cuda.module, "goose_route_norm");
        if (error == 0) error = cuda.function_get(&cuda.blit, cuda.module, "goose_tensor_blit");
        if (error == 0) error = cuda.function_get(&cuda.fwht, cuda.module, "goose_kv_fwht");
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
    if (activations.active) return -1;
    const char * setting = getenv("GOOSE_CUDA");
    if (setting && setting[0] == '0' && !setting[1]) return 0;
    pthread_mutex_lock(&activation_lock);
    activations.active = true;
    activations.step = -1;
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

int fg_cuda_activation_prepare(uint64_t host, int64_t bytes, bool fresh, bool overwrite, int64_t step, int64_t deadline) {
    if (!activations.active) return 0;
    activations.target = fresh ? host : 0;
    activations.target_bytes = bytes;
    activations.target_deadline = deadline;
    activations.step = activations.retire ? step : -1;
    if (step >= 0 && activations.retire) {
        if (activations.count > 0) activations.expired_bytes += fg143_activation_expire(activation_ranges(), step);
        if (activations.large_output[1] > 0) activations.expired_bytes += fg143_activation_expire(activation_large_range(), step);
    }
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
    int error = 0;
    if (activations.pending) {
        if (push_context() != 1) error = -1;
        else {
            error = cuda.sync();
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
    const int64_t slot = fg143_pinned_packet_slot((int64_t) activations.packet_used, MIB_BYTES, (int64_t) bytes);
    if (slot < 0) return cuda.upload(device, data, bytes);
    unsigned char * host = cuda.packet_host + slot;
    memcpy(host, data, bytes);
    activations.packet_used = (size_t) slot + bytes;
    activations.pending = true;
    return cuda.upload_async(device, host, bytes, NULL);
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

int64_t fg_cuda_set_q4_rows(uint64_t source, int64_t source_bytes, int64_t stride,
                            uint64_t target, int64_t target_bytes, int64_t width, int64_t rows) {
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
        error = cuda.launch(cuda.q4_rows, (unsigned) ((tasks + 127) / 128), 1, 1, 128, 1, 1, 0, NULL, args, NULL);
        if (error == 0) error = cuda.sync();
    }
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: Q4 cache update failed (%d)\n", error ? error : popped);
        return -1;
    }
    ++cuda_q4_writes;
    return 1;
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
        !cache.state || start_context() != 1) {
        free_expert_metadata(&cache);
        return -1;
    }
    int status = cuda.experts.weights.address ? -1 : reserve(&cache.weights, (size_t) bytes);
    if (status == 0) {
        memcpy(cache.groups.data, groups.data, (size_t) groups.len * sizeof(*groups.data));
        memcpy(cache.layers.data, layers.data, (size_t) layers.len * sizeof(*layers.data));
        ec_reset(cache.groups, cache.layers, (uint64_t) (uintptr_t) cache.state, cache.work);
        cuda.experts = cache;
    } else {
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
    return (int64_t) cuda.experts.weights.bytes;
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
    pthread_mutex_unlock(&cuda_lock);
}

bool fg_cuda_expert_stage_enabled(int64_t active_experts) {
    if (active_experts <= 8 || !activations.active || !activations.defer_waits ||
        !cuda.defer_matrix_waits || !cuda.expert_stream || cuda.expert_pins.len <= 0 ||
        (uint64_t) active_experts > SIZE_MAX / 2 / sizeof(*cuda.expert_upload_jobs)) return false;
    const size_t capacity = (size_t) active_experts * 2;
    if (capacity > cuda.expert_upload_capacity) {
        struct expert_upload_job * jobs = realloc(cuda.expert_upload_jobs, capacity * sizeof(*jobs));
        if (!jobs) return false;
        cuda.expert_upload_jobs = jobs;
        cuda.expert_upload_capacity = capacity;
    }
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

static int64_t expert_upload(int64_t offset, uint64_t source, int64_t bytes, bool staged) {
    const struct device_buffer * dest = &cuda.experts.weights;
    if (!dest->address || !source || offset < 0 || bytes <= 0 ||
        (uint64_t) offset > dest->bytes || (uint64_t) bytes > dest->bytes - (uint64_t) offset) return -1;
    const DevicePtr address = dest->address + (uint64_t) offset;
    const void * host = (const void *) (uintptr_t) source;
    int status;
    if (staged && expert_source_pinned(source, (uint64_t) bytes)) {
        if (cuda.expert_upload_count == cuda.expert_upload_capacity) return -1;
        status = 0;
        if (cuda.expert_upload_count == 0) {
            // The chosen slots may still be read by the previous projection.
            status = cuda.event_record(cuda.expert_slots_ready, NULL);
        }
        if (status == 0) {
            cuda.expert_upload_jobs[cuda.expert_upload_count++] =
                (struct expert_upload_job) {address, host, (size_t) bytes};
        }
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

static int submit_expert_stage(void) {
    if (cuda.expert_upload_count == 0) return 0;
    // Queue uploads after the consumer's descriptor copies and kernels.
    // Future projections wait on the event, never on a host-side launch.
    int status = cuda.stream_wait_event(cuda.expert_stream, cuda.expert_slots_ready, 0);
    for (size_t i = 0; status == 0 && i < cuda.expert_upload_count; ++i) {
        const struct expert_upload_job * job = &cuda.expert_upload_jobs[i];
        activations.pending = true;
        status = cuda.upload_async(job->destination, job->source, job->bytes, cuda.expert_stream);
    }
    if (status == 0) status = cuda.event_record(cuda.expert_uploads_ready, cuda.expert_stream);
    if (status == 0) {
        cuda.expert_upload_count = 0;
        cuda.expert_upload_queued = true;
        return 0;
    }
    (void) cuda.sync();
    reset_expert_stage();
    fprintf(stderr, "Goose CUDA: staged expert upload failed (%d)\n", status);
    return -1;
}

int64_t fg_cuda_prepare_experts(uint64_t tensor, int64_t bytes, sl_i64 active, sl_i64 offsets) {
    struct expert_cache * cache = &cuda.experts;
    if (!cache->weights.address) return 0;
    if (cuda.expert_upload_count != 0) {
        reset_expert_stage();
        return -1;
    }
    if (cuda.expert_upload_queued) {
        // The current projection may consume a plane staged by the last one.
        if (cuda.stream_wait_event(NULL, cuda.expert_uploads_ready, 0) != 0) return -1;
        cuda.expert_upload_queued = false;
    }
    const uint64_t state = (uint64_t) (uintptr_t) cache->state;
    const int64_t status = ec_prepare(cache->groups, cache->layers, state, cache->work,
        tensor, bytes, active, offsets);
    if (status < 0) reset_expert_stage();
    return status;
}

int64_t fg_cuda_begin(uint64_t input, int64_t input_bytes, uint64_t output, int64_t output_bytes) {
    const int active = start_context();
    if (active != 1) return active;
    cuda_matrix_kind = -1;
    matrix_input = 0;
    const sl_u8 source = {(void *) (uintptr_t) input, input_bytes};
    DevicePtr * staged = cuda.stage_matrix_input ? &matrix_input : NULL;
    int status = activation_output_for_input(output, output_bytes, &matrix_output, source, staged);
    if (status == 0 && !matrix_input) status = activation_input(source, &cuda.input, &matrix_input);
    if (status == 0) return 1;
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
        mmq_d4 = 0, mmq_ds4 = 1, mmq_d4_reciprocal = 3, mmq_ds4_reciprocal = 4 };
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
    const bool dense_wide = dense_shared && dense_reference && count == 4096 && rows % 128 == 0;
    const bool routed_wide = indexed_reference && routed_shared;
    const int routes_per_tile = routed_wide ? (routed_x64 ? 64 : 32) : 8;
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
    bool shared = cuda.mmq_shared && shared_rows > 0 && shared_rows < count && shared_rows <= 65535 &&
        input_stride >= (int64_t) extent * 256 * (int64_t) sizeof(float) &&
        (uint64_t) shared_rows * (uint64_t) extent * 288 <= staging_budget;
    size_t packed_bytes;
    int error;
    for (;;) {
        if (cuda.mmq_align_window && dense_wide && !shared) {
            const int group = dense_x64 && window_tiles >= 8 ? 8 : 4;
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
        if (error == 2 && shared) {
            shared = false;
            continue;
        }
        if (error != 2 || window_tiles == 1) break;
        window_tiles /= 2;
    }
    if (error == 0) error = reserve(&cuda.mmq_tiles, tile_bytes);
    if (error == 0) error = cuda.upload(cuda.mmq_tiles.address, tiles, tile_bytes);
    if (!shared) input_stride = 0;
    if (error == 0 && shared) {
        if (cuda.mmq_window) window_tiles = tile_count < 8191 ? tile_count : 8191;
        void * args[] = {&matrix_input, &cuda.mmq_packed.address, &extent, &layout, &input_stride};
        error = cuda.launch(cuda.mmq_pack_shared, (unsigned) (2 * extent), (unsigned) shared_rows, 1,
            32, 1, 1, 0, NULL, args, NULL);
    }
    int first = 0, first_route = 0;
    void * pack_args[] = {&matrix_input, &cuda.jobs.address, &cuda.mmq_packed.address,
                         &extent, &layout, &first_route, &job_words};
    void * matrix_args[] = {&weight, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address,
        &cuda.mmq_tiles.address, &stride, &extent, &kind, &rows, &first, &first_route, &job_words, &input_stride,
        &reference_columns, &cuda.multiprocessors};
    while (error == 0 && first < tile_count) {
        const int batch = (int) (tile_count - first > window_tiles ? window_tiles : tile_count - first);
        const int64_t last = 3ll * (first + batch - 1);
        first_route = (int) tiles[3ll * first];
        const unsigned routes = (unsigned) (tiles[last] + tiles[last + 1] - first_route);
        if (!shared) {
            error = cuda.launch(cuda.mmq_pack, (unsigned) (2 * extent), routes, 1,
                                32, 1, 1, 0, NULL, pack_args, NULL);
        }
        if (error == 0) {
            const int workers = cuda.mmq_stream && !reference ?
                (int) fg143_mmq_stream_workers(rows, batch, extent, cuda.multiprocessors) : 0;
            if (routed_wide) {
                void * args[] = {&weight, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address,
                    &cuda.mmq_tiles.address, &stride, &extent, &rows, &first, &first_route, &input_stride};
                error = cuda.launch(routed_shared, (unsigned) rows / 16, (unsigned) batch,
                    1, 128, 1, 1, 0, NULL, args, NULL);
            } else if (dense_wide && !shared && first % 4 == 0 && batch % 4 == 0 &&
                first_route == 8 * first && routes == 8u * (unsigned) batch) {
                const bool x64 = dense_x64 && first % 8 == 0 && batch % 8 == 0;
                void * args[] = {&weight, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address,
                    &stride, &extent, &rows, &first_route, &cuda.multiprocessors};
                error = cuda.launch(x64 ? dense_x64 : dense_shared, (unsigned) rows / 16, (unsigned) batch / (x64 ? 8 : 4),
                    1, 128, 1, 1, 0, NULL, args, NULL);
            } else if (workers > 0) {
                error = launch_mmq_stream(weight, stride, extent, kind, rows, first, first_route, job_words, batch, workers, input_stride);
            } else {
                Function matrix = reference ? reference_matrix : cuda.mmq_matrix;
                error = cuda.launch(matrix, 1u + ((unsigned) rows - 1) / 64, (unsigned) batch, 1,
                                    128, 1, 1, 0, NULL, matrix_args, NULL);
            }
            if (error == 0) ++cuda_matrix_launches;
        }
        first += batch;
    }
    free(tiles);
    if (first != 0) error = finish_matrix_launches(error);
    if (error != 0) {
        fprintf(stderr, "Goose CUDA: MMQ failed (%d), packed activations need %zu bytes\n", error, packed_bytes);
        return -1;
    }
    ++cuda_mmq_projections;
    return 0;
}

static int launch_mmvq(DevicePtr weight, uint64_t stride, int extent, int kind, int rows, int count, int words) {
    const size_t packed_bytes = (size_t) extent * 288 * (size_t) count;
    int error = reserve(&cuda.mmq_packed, packed_bytes);
    if (error == 0) {
        void * pack[] = {&matrix_input, &cuda.mmq_packed.address, &cuda.jobs.address, &extent, &words};
        error = cuda.launch(cuda.mmvq_pack, (unsigned) (2 * extent), (unsigned) count, 1, 128, 1, 1, 0, NULL, pack, NULL);
    }
    if (error == 0) {
        void * matrix[] = {&weight, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address,
            &stride, &extent, &kind, &words};
        error = cuda.launch(cuda.mmvq, (unsigned) rows, (unsigned) count, 1, 128, 1, 1, 0, NULL, matrix, NULL);
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
    const size_t job_bytes = (size_t) jobs.len * sizeof(*jobs.data);
    int error = reserve(&cuda.jobs, job_bytes);
    if (error == 0) error = upload_packet(cuda.jobs.address, jobs.data, job_bytes);
    if (error == 0 && cuda.mmq_matrix && kind != 0 && jobs.len / job_words >= 16) {
        const int result = launch_mmq(weight, (uint64_t) row_stride, (int) extent, (int) kind,
                                      (int) rows, jobs, job_words, shared_rows, input_stride);
        if (result == 0) cuda_matrix_kind = (int) kind;
        return result;
    }

    const int count = (int) (jobs.len / job_words);
    if (error == 0 && cuda.mmvq && kind != 0 && extent <= INT_MAX / 256 &&
        (count == 1 || (job_words == 3 && count == 8))) {
        return launch_mmvq(weight, (uint64_t) row_stride, (int) extent, (int) kind, (int) rows, count, job_words);
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

int64_t fg_cuda_plane(uint64_t weights, int64_t weight_bytes, int64_t row_stride,
                     int64_t extent, int64_t kind, int64_t rows, sl_i64 jobs) {
    DevicePtr weight = 0;
    if (cuda.dense_ranges.len > 0) {
        const int64_t offset = dw_cuda_dense_offset(cuda.dense_ranges, weights, weight_bytes);
        if (offset >= 0) weight = cuda.dense.address + (uint64_t) offset;
    }
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
    if (status == 0) status = submit_expert_stage();
    if (status == 0) ++cuda_cached_projections;
    return status;
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
    return cuda.flash_mma && mask_bytes == 0 && c[0] == 256 && c[1] == 256 && c[2] >= 8 &&
        c[4] > 0 && c[4] <= 65535 && c[5] == c[4] && c[3] == 8 * c[4] &&
        c[6] >= c[2] && c[6] <= INT_MAX - 31 && c[21] > 0 && c[21] <= c[6] - c[2] + 1 &&
        c[7] > 0 && c[7] % 8 == 0 && c[8] > 0 && c[8] % 8 == 0;
}

static bool flash_vec_shape(const int64_t * c, int64_t mask_bytes) {
    return cuda.flash_vec && mask_bytes == 0 && c[0] == 256 && c[1] == 256 && c[2] == 1 &&
        c[3] > 0 && c[3] <= 256 && c[4] > 0 && c[5] == c[4] && c[3] % c[4] == 0 &&
        c[6] > 0 && c[6] <= INT_MAX - 127 && c[21] >= 0 && c[21] <= c[6] && c[14] == 1024;
}

static int launch_flash_node(const struct kernel_node_params * p, Stream stream) {
    return cuda.launch(p->function, p->grid_x, p->grid_y, p->grid_z, p->block_x, p->block_y, p->block_z,
        p->shared_bytes, stream, p->params, p->extra);
}

static int capture_flash_graph(const struct kernel_node_params * vector, const struct kernel_node_params * merge) {
    enum { blocking_stream = 0, thread_local_capture = 1 };
    // The blocking stream preserves dependencies with the legacy stream.
    int error = cuda.stream_create(&cuda.flash_graph_stream, blocking_stream);
    if (error == 0) error = cuda.graph_begin(cuda.flash_graph_stream, thread_local_capture);
    if (error != 0) return error;
    error = launch_flash_node(vector, cuda.flash_graph_stream);
    if (error == 0) error = launch_flash_node(merge, cuda.flash_graph_stream);
    const int end_error = cuda.graph_end(cuda.flash_graph_stream, &cuda.flash_graph);
    if (error != 0 || end_error != 0) return error ? error : end_error;
    size_t nodes = 1;
    error = cuda.graph_roots(cuda.flash_graph, &cuda.flash_graph_root, &nodes);
    if (error != 0 || nodes != 1) return error ? error : -1;
    nodes = 1;
    error = cuda.graph_dependents(cuda.flash_graph_root, &cuda.flash_graph_merge, &nodes);
    if (error != 0 || nodes != 1) return error ? error : -1;
    return cuda.graph_instantiate(&cuda.flash_graph_exec, cuda.flash_graph, 0);
}

static int launch_flash_pair(const struct kernel_node_params * vector, const struct kernel_node_params * merge) {
    if (cuda.flash_graph_enabled) {
        int error;
        if (!cuda.flash_graph_exec) {
            error = capture_flash_graph(vector, merge);
        } else {
            error = cuda.graph_set_kernel(cuda.flash_graph_exec, cuda.flash_graph_root, vector);
            if (error == 0) error = cuda.graph_set_kernel(cuda.flash_graph_exec, cuda.flash_graph_merge, merge);
        }
        if (error == 0) return cuda.graph_launch(cuda.flash_graph_exec, cuda.flash_graph_stream);
        if (getenv("GOOSE_GRAPH_LOG")) fprintf(stderr, "Goose CUDA Flash graph: setup failed (%d)\n", error);
        release_flash_graph();
    }
    const int error = launch_flash_node(vector, NULL);
    return error == 0 ? launch_flash_node(merge, NULL) : error;
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
    if (prepacked) {
        void * pack[] = {&staged[0], &query, &staged[4], &scale};
        const int error = cuda.launch(cuda.flash_vec_pack, (unsigned) heads, 1, 1, 32, 1, 1, 0, NULL, pack, NULL);
        if (error != 0) return error;
    }
    void * args[] = {&query, &staged[1], &staged[2], &matrix, &meta, &staged[4], &keys, &scale};
    const struct kernel_node_params vector = { .function = prepacked ? cuda.flash_vec_packed : cuda.flash_vec, .grid_x = (unsigned) heads,
        .grid_y = (unsigned) parts, .grid_z = 1, .block_x = 128, .block_y = 1, .block_z = 1, .params = args };
    void * merge_args[] = {&matrix, &meta, &output, &parts, &heads};
    const struct kernel_node_params merge = { .function = cuda.flash_vec_merge, .grid_x = (unsigned) heads,
        .grid_y = 1, .grid_z = 1, .block_x = 256, .block_y = 1, .block_z = 1, .params = merge_args };
    const int error = parts > 1 ? launch_flash_pair(&vector, &merge) : launch_flash_node(&vector, NULL);
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
    const unsigned tiles_per_head = (unsigned) ((c[2] + 7) / 8);
    const uint64_t tiles = (uint64_t) tiles_per_head * heads;
    unsigned workers = cuda.flash_stream ?
        (unsigned) fg143_flash_stream_workers(c[2], c[4], keys, cuda.multiprocessors) : 0;
    DevicePtr fix = 0;
    if (workers && tiles % workers != 0) {
        const size_t fix_bytes = (size_t) workers * 64 * (256 + 4) * sizeof(float);
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
            error = cuda.launch(cuda.flash_fixup_uniform, (unsigned) tiles, 8, 8, 256, 1, 1, 0, NULL, args, NULL);
        } else {
            int32_t key_count = (int32_t) keys;
            void * args[] = {&output, &fix, &queries, &query_heads, &key_count};
            error = cuda.launch(cuda.flash_fixup_general, workers, 8, 8, 256, 1, 1, 0, NULL, args, NULL);
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
        error = reserve(&cuda.attention_config, sizeof packet);
    }
    if (error == 0) error = upload_packet(cuda.attention_config.address, packet, sizeof packet);
    void * args[] = {&cuda.attention_config.address};
    if (error == 0) {
        if (mma) {
            error = cuda.launch(cuda.gdn_mma, (unsigned) (heads * sequences), 1, 1, 256, 1, 1, GDN_MMA_SHARED_BYTES, NULL, args, NULL);
        } else {
            error = cuda.launch(cuda.gdn, (unsigned) heads, (unsigned) sequences,
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
    DevicePtr staged[3] = {0}, output = 0;
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
        error = vector_input(y, &cuda.weight, &staged[1], plan + 14, (plan[23] & 2) != 0);
    }
    if (error == 0) {
        error = activation_input((sl_u8) {plan, sizeof(plan)}, &cuda.attention_config, &staged[2]);
    }
    if (error == 0) {
        enum { rms_operation = 11 };
        const int64_t width = plan[2];
        const bool reference = cuda.rms_reference && plan[0] == rms_operation && width <= INT_MAX &&
            plan[10] == 4 && (y.len == 0 || plan[18] == 4);
        if (reference) {
            const bool vec = width % 4 == 0 && width <= 16384 && output % 16 == 0 &&
                vector_aligned((uint64_t) (uintptr_t) x.data, config.data + 10) &&
                vector_aligned(staged[0], plan + 10) && (y.len == 0 || (plan[14] == width &&
                    vector_aligned((uint64_t) (uintptr_t) y.data, config.data + 18) && vector_aligned(staged[1], plan + 18)));
            const int32_t vectors = vec ? (width <= 4096 ? 1 : 4) : 0;
            const unsigned lanes = vec ? (width <= 128 ? 32 : width <= 512 ? 128 : width <= 1024 ? 256 :
                width <= 2048 ? 512 : 1024) : (width < 1024 ? 256 : 1024);
            void * args[] = {&staged[0], &staged[1], &output, &staged[2], &param0, (void *) &vectors};
            error = cuda.launch(cuda.rms_reference, (unsigned) plan[24], 1, 1, lanes, 1, 1, 0, NULL, args, NULL);
        } else {
            void * args[] = {&staged[0], &staged[1], &output, &staged[2], &param0, &param1};
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

uint64_t fg_cuda_take_vectors(void) {
    const uint64_t vectors = cuda_vectors;
    cuda_vectors = 0;
    return vectors;
}

int64_t fg_cuda_position(sl_i64 config, sl_u8 x, sl_u8 y, sl_u8 z, sl_u8 out) {
    if (config.len != 27 || (config.data[0] != 0 && config.data[0] != 1) ||
        config.data[1] <= 0 || config.data[1] > INT_MAX ||
        config.data[2] <= 0 || config.data[2] > 65535 ||
        x.len <= 0 || y.len <= 0 || z.len < 0 || out.len <= 0) return -1;
    const bool rope = config.data[0] == 0;
    if (rope && (config.data[3] <= 0 || config.data[3] > INT_MAX || config.data[4] <= 0)) return -1;
    const int active = start_context();
    if (active != 1) return active;
    const sl_u8 data[] = {x, y, z, {(uint8_t *) config.data, config.len * (int64_t) sizeof(*config.data)}};
    struct device_buffer * buffers[] = {&cuda.input, &cuda.weight, &cuda.jobs, &cuda.attention_config};
    DevicePtr staged[4] = {0}, output = 0;
    const uint64_t host = (uint64_t) (uintptr_t) out.data;
    int error = activation_output(host, out.len, &output);
    if (error == 0 && rope) error = reserve(&cuda.attention_scratch, (size_t) config.data[4]);
    if (error == 0) error = upload_inputs(data, buffers, staged, sizeof(data) / sizeof(*data));
    const unsigned gx = (unsigned) config.data[1], gy = (unsigned) config.data[2];
    if (error == 0) {
        if (rope) {
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
            Function conv = tiled ? cuda.ssm_conv_long : cuda.ssm_conv;
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

int64_t fg_cuda_routing(sl_i64 config, sl_u8 x, sl_u8 y, sl_u8 out) {
    if (config.len != 22 || config.data[0] < 0 || config.data[0] >= 5 ||
        config.data[1] <= 0 || config.data[1] > INT_MAX ||
        (config.data[2] != 32 && config.data[2] != 128) || x.len <= 0 || y.len < 0 || out.len <= 0) return -1;
    const int active = start_context();
    if (active != 1) return active;
    const int64_t op = config.data[0];
    const bool compact = op == 1 && config.data[3] != 0;
    const bool topk = compact && cuda.moe_topk;
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
        error = cuda.launch(operations[op], (unsigned) groups, 1, 1,
            (unsigned) config.data[2], 1, 1, 0, NULL, args, NULL);
        if (error == 0) error = activation_publish(host, out.len, output, false);
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
        error = cuda.launch(cuda.blit, (unsigned) config.data[1], 1, 1, 128, 1, 1, 0, NULL, args, NULL);
        if (error == 0) {
            error = mode == 2 ? cuda.sync() : activation_publish(host_out, out.len, dest, false);
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
