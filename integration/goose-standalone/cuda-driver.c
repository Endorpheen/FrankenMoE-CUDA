#include "cpu.h"
#include "goose-cuda-image.h"

#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int64_t dw_cuda_dense_offset(sl_i64 ranges, uint64_t address, int64_t bytes);
extern int64_t qr_state_offset(sl_i64 ranges, uint64_t address, int64_t bytes, int64_t kind);
extern int64_t fg143_activation_lookup(sl_i64 ranges, uint64_t host, int64_t bytes);
extern int64_t fg143_activation_overlap(sl_i64 ranges, uint64_t host, int64_t bytes, int64_t first);
extern int64_t fg143_activation_slot(int64_t used, int64_t capacity, int64_t bytes);
extern void ec_reset(sl_i64 groups, sl_i64 layers, uint64_t words32, sl_i64 words64);
extern int64_t ec_prepare(sl_i64 groups, sl_i64 layers, uint64_t words32, sl_i64 words64,
    uint64_t tensor, int64_t bytes, sl_i64 heads, sl_i64 result);

typedef struct CUctx_st * Context;
typedef struct CUmod_st * Module;
typedef struct CUfunc_st * Function;
typedef unsigned long long DevicePtr;

struct device_buffer {
    DevicePtr address;
    size_t bytes;
};

struct expert_cache {
    struct device_buffer weights;
    sl_i64 groups, layers, work;
    int32_t * state;
};

static struct {
    void * library;
    Context context;
    Module module;
    Function kernel, attention_tiles, attention_merge, gdn, q4_rows, vector;
    Function mmq_pack, mmq_matrix;
    Function rope_phases, rope_apply, ssm_conv;
    Function route_reduce, route_sort, route_gather, blit, fwht;
    struct device_buffer input, output, weight, jobs;
    struct device_buffer mmq_packed, mmq_tiles;
    struct device_buffer attention_value, attention_scratch, attention_config;
    struct device_buffer gdn_beta;
    struct device_buffer activations;
    struct device_buffer dense;
    sl_i64 dense_ranges;
    struct device_buffer state;
    sl_i64 state_ranges;
    struct expert_cache experts;
    int status;
    int (*init)(unsigned);
    int (*device_get)(int *, int);
    int (*device_attribute)(int *, int, int);
    int (*create)(Context *, unsigned, int);
    int (*destroy)(Context);
    int (*push)(Context);
    int (*pop)(Context *);
    int (*module_load)(Module *, const void *);
    int (*function_get)(Function *, Module, const char *);
    int (*alloc)(DevicePtr *, size_t);
    int (*memory_info)(size_t *, size_t *);
    int (*release)(DevicePtr);
    int (*upload)(DevicePtr, const void *, size_t);
    int (*download)(void *, DevicePtr, size_t);
    int (*zero)(DevicePtr, unsigned char, size_t);
    int (*launch)(Function, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned,
                  unsigned, void *, void **, void **);
    int (*sync)(void);
} cuda;

static pthread_once_t cuda_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t cuda_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t activation_lock = PTHREAD_MUTEX_INITIALIZER;
enum { ACTIVATION_WORDS = 4, ACTIVATION_RECORDS = 256, ACTIVATION_BYTES = 64 * 1024 * 1024 };
static _Thread_local struct {
    bool active, unavailable;
    uint64_t target;
    int64_t target_bytes, count, used;
    int64_t ranges[ACTIVATION_RECORDS * ACTIVATION_WORDS];
    uint64_t hits;
} activations;
static _Thread_local DevicePtr matrix_input, matrix_output;
static _Thread_local uint64_t cuda_matrices;
static _Thread_local uint64_t cuda_matrix_launches, cuda_cached_projections;
static _Thread_local uint64_t cuda_mmq_projections;
static _Thread_local uint64_t cuda_f32_matrices;
static _Thread_local int cuda_matrix_kind;
static _Thread_local uint64_t cuda_attentions;
static _Thread_local uint64_t cuda_causal_attentions;
static _Thread_local uint64_t cuda_gdns;
static _Thread_local uint64_t cuda_resident_kv, cuda_resident_gdn;
static _Thread_local uint64_t cuda_q4_writes;
static _Thread_local uint64_t cuda_vectors;
static _Thread_local uint64_t cuda_fwhts;
static _Thread_local uint64_t cuda_ropes, cuda_convs;
static _Thread_local uint64_t cuda_routes[4];
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

static void cleanup(void) {
    if (cuda.context) cuda.destroy(cuda.context);
    free(cuda.dense_ranges.data);
    free(cuda.state_ranges.data);
    free_expert_metadata(&cuda.experts);
    if (cuda.library) dlclose(cuda.library);
}

static void init_cuda(void) {
    if (goose_cuda_image[0] == 0) return;
    cuda.library = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!cuda.library) return;
    atexit(cleanup);
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
    LOAD(alloc, "cuMemAlloc_v2");
    LOAD(memory_info, "cuMemGetInfo_v2");
    LOAD(release, "cuMemFree_v2");
    LOAD(upload, "cuMemcpyHtoD_v2");
    LOAD(download, "cuMemcpyDtoH_v2");
    LOAD(zero, "cuMemsetD8_v2");
    LOAD(launch, "cuLaunchKernel");
    LOAD(sync, "cuCtxSynchronize");
#undef LOAD
    int device;
    if (cuda.init(0) != 0 || cuda.device_get(&device, 0) != 0) return;
    cuda.status = -1;
    const char * setting = getenv("GOOSE_MMQ");
    const bool mmq = setting && strcmp(setting, "1") == 0;
    if (mmq) {
        enum { compute_capability_major = 75 };
        int major = 0;
        if (cuda.device_attribute(&major, compute_capability_major, device) != 0 ||
            major < 8 || goose_cuda_mmq_image[0] == 0) {
            fputs("Goose CUDA: MMQ requires the SM80 device image and compute capability 8 or newer\n", stderr);
            return;
        }
    }
    int error = cuda.create(&cuda.context, 0, device);
    if (error == 0) {
        error = cuda.module_load(&cuda.module, mmq ? goose_cuda_mmq_image : goose_cuda_image);
        if (error == 0) error = cuda.function_get(&cuda.kernel, cuda.module, "goose_quantized_matmul");
        if (error == 0 && mmq) error = cuda.function_get(&cuda.mmq_pack, cuda.module, "goose_mmq_pack_q8");
        if (error == 0 && mmq) error = cuda.function_get(&cuda.mmq_matrix, cuda.module, "goose_mmq_matmul");
        if (error == 0) error = cuda.function_get(&cuda.attention_tiles, cuda.module, "goose_q4_attention_tiles");
        if (error == 0) error = cuda.function_get(&cuda.attention_merge, cuda.module, "goose_q4_attention_merge");
        if (error == 0) error = cuda.function_get(&cuda.gdn, cuda.module, "goose_gated_delta_net");
        if (error == 0) error = cuda.function_get(&cuda.q4_rows, cuda.module, "goose_set_q4_rows");
        if (error == 0) error = cuda.function_get(&cuda.vector, cuda.module, "goose_vector_ops");
        if (error == 0) error = cuda.function_get(&cuda.rope_phases, cuda.module, "goose_rope_phases");
        if (error == 0) error = cuda.function_get(&cuda.rope_apply, cuda.module, "goose_rope_apply");
        if (error == 0) error = cuda.function_get(&cuda.ssm_conv, cuda.module, "goose_ssm_conv");
        if (error == 0) error = cuda.function_get(&cuda.route_reduce, cuda.module, "goose_route_reduce");
        if (error == 0) error = cuda.function_get(&cuda.route_sort, cuda.module, "goose_route_sort");
        if (error == 0) error = cuda.function_get(&cuda.route_gather, cuda.module, "goose_route_gather");
        if (error == 0) error = cuda.function_get(&cuda.blit, cuda.module, "goose_tensor_blit");
        if (error == 0) error = cuda.function_get(&cuda.fwht, cuda.module, "goose_kv_fwht");
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

static int release_buffer(struct device_buffer * buffer) {
    if (!buffer->address) return 0;
    const int error = cuda.release(buffer->address);
    if (error == 0) {
        *buffer = (struct device_buffer) {0};
    }
    return error;
}

static int reserve(struct device_buffer * buffer, size_t bytes) {
    if (bytes <= buffer->bytes) return 0;
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

static int activation_download_record(int64_t i) {
    int64_t * range = activations.ranges + ACTIVATION_WORDS * i;
    if (range[1] == 0 || range[3] != 0) return 0;
    const int error = cuda.download((void *) (uintptr_t) range[0],
        cuda.activations.address + (uint64_t) range[2], (size_t) range[1]);
    if (error == 0) range[3] = 1;
    return error;
}

static int activation_download_all(void) {
    for (int64_t i = 0; i < activations.count; ++i) {
        const int error = activation_download_record(i);
        if (error != 0) return error;
    }
    return 0;
}

static int activation_sync_range(uint64_t host, int64_t bytes, bool invalidate) {
    for (int64_t i = 0; activations.count > 0;) {
        const int64_t found = fg143_activation_overlap(activation_ranges(), host, bytes, i);
        if (found < 0) break;
        const int error = activation_download_record(found);
        if (error != 0) return error;
        if (invalidate) activations.ranges[ACTIVATION_WORDS * found + 1] = 0;
        i = found + 1;
    }
    return 0;
}

int fg_cuda_activation_graph_begin(void) {
    if (activations.active) return -1;
    const char * setting = getenv("GOOSE_CUDA");
    if (setting && setting[0] == '0' && !setting[1]) return 0;
    pthread_mutex_lock(&activation_lock);
    activations.active = true;
    activations.unavailable = false;
    activations.hits = 0;
    return 0;
}

int64_t fg_cuda_activation_read_address(uint64_t host, int64_t bytes) {
    if (activations.count == 0) return 0;
    int64_t first = 0;
    for (;;) {
        first = fg143_activation_overlap(activation_ranges(), host, bytes, first);
        if (first < 0) return 0;
        if (activations.ranges[first * ACTIVATION_WORDS + 3] == 0) break;
        ++first;
    }
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

int fg_cuda_activation_prepare(uint64_t host, int64_t bytes, bool fresh) {
    if (!activations.active) return 0;
    activations.target = fresh ? host : 0;
    activations.target_bytes = bytes;
    if (activations.count == 0 || fg143_activation_overlap(activation_ranges(), host, bytes, 0) < 0) return 0;
    if (push_context() != 1) return -1;
    const int error = activation_sync_range(host, bytes, true);
    return finish_context() == 0 && error == 0 ? 0 : -1;
}

int fg_cuda_activation_graph_end(bool success, bool all, uint64_t host, int64_t bytes, uint64_t * hits) {
    *hits = activations.hits;
    activations.hits = 0;
    if (!activations.active) return 0;
    int error = 0;
    if (success && activations.count > 0) {
        if (all) {
            if (push_context() != 1) error = -1;
            else {
                error = activation_download_all();
                if (finish_context() != 0) error = -1;
            }
        } else {
            error = (int) fg_cuda_activation_read_address(host, bytes);
        }
    }
    activations.count = activations.used = 0;
    activations.target = 0;
    activations.active = false;
    pthread_mutex_unlock(&activation_lock);
    return error;
}

// Called with the CUDA context bound. Reserve output before resolving input
// addresses so a flush never invalidates a pointer passed to a launch.
static int activation_output(uint64_t host, int64_t bytes, DevicePtr * device) {
    if (activations.active && !activations.unavailable && host != 0 && host == activations.target &&
        bytes <= activations.target_bytes && bytes > 0 && bytes <= ACTIVATION_BYTES) {
        if (!cuda.activations.address) {
            const int error = reserve(&cuda.activations, ACTIVATION_BYTES);
            if (error == 2) activations.unavailable = true; // CUDA_ERROR_OUT_OF_MEMORY
            else if (error != 0) return error;
        }
        if (cuda.activations.address) {
            int64_t pos = fg143_activation_slot(activations.used, (int64_t) cuda.activations.bytes, bytes);
            if (pos == -2 || activations.count == ACTIVATION_RECORDS) {
                const int error = activation_download_all();
                if (error != 0) return error;
                activations.count = activations.used = 0;
                pos = fg143_activation_slot(0, (int64_t) cuda.activations.bytes, bytes);
            }
            if (pos >= 0) {
                *device = cuda.activations.address + (uint64_t) pos;
                return 0;
            }
        }
    }
    const int error = reserve(&cuda.output, (size_t) bytes);
    if (error == 0) *device = cuda.output.address;
    return error;
}

static int activation_publish(uint64_t host, int64_t bytes, DevicePtr device, bool complete) {
    if (!activations.active || device == cuda.output.address) {
        return cuda.download((void *) (uintptr_t) host, device, (size_t) bytes);
    }
    const int error = complete ? 0 : cuda.sync();
    if (error != 0) return error;
    int64_t * next = activations.ranges + ACTIVATION_WORDS * activations.count++;
    next[0] = (int64_t) host; next[1] = bytes;
    next[2] = (int64_t) (device - cuda.activations.address); next[3] = 0;
    activations.used = next[2] + bytes;
    return 0;
}

static DevicePtr activation_device(uint64_t host, int64_t bytes) {
    if (activations.count == 0) return 0;
    const int64_t offset = fg143_activation_lookup(activation_ranges(), host, bytes);
    return offset >= 0 ? cuda.activations.address + (uint64_t) offset : 0;
}

static int activation_input(sl_u8 data, struct device_buffer * staging, DevicePtr * device) {
    if (activations.count > 0) {
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
    if (error == 0) error = cuda.upload(staging->address, data.data, (size_t) data.len);
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

int64_t fg_cuda_load_model_state(sl_i64 ranges, int64_t bytes) {
    if (!ranges.data || ranges.len <= 0 || ranges.len % 4 != 0 || bytes <= 0 ||
        (uint64_t) ranges.len > SIZE_MAX / sizeof(*ranges.data)) return -1;
    const size_t metadata_bytes = (size_t) ranges.len * sizeof(*ranges.data);
    int64_t * records = malloc(metadata_bytes);
    if (!records) return -1;
    memcpy(records, ranges.data, metadata_bytes);
    if (start_context() != 1) { free(records); return -1; }
    struct device_buffer staging = {0};
    int error = cuda.state.address ? -1 : reserve(&staging, (size_t) bytes);
    if (error == 0) error = cuda.zero(staging.address, 0, staging.bytes);
    if (error == 0) {
        cuda.state = staging;
        cuda.state_ranges = (sl_i64) {records, ranges.len};
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
    return (int64_t) cuda.state.bytes;
}

static DevicePtr state_address(uint64_t host, int64_t bytes, int64_t kind) {
    if (!cuda.state.address) return 0;
    const int64_t offset = qr_state_offset(cuda.state_ranges, host, bytes, kind);
    if (offset < 0 || (uint64_t) offset > cuda.state.bytes ||
        (uint64_t) bytes > cuda.state.bytes - (uint64_t) offset) return 0;
    return cuda.state.address + (uint64_t) offset;
}

int64_t fg_cuda_reset_model_state(void) {
    published_state.bytes = 0;
    if (!cuda.state.address) return 0;
    if (push_context() != 1) return -1;
    const int status = cuda.zero(cuda.state.address, 0, cuda.state.bytes);
    return finish_context() == 0 && status == 0 ? 0 : -1;
}

int64_t fg_cuda_state_write(uint64_t host, int64_t bytes) {
    const DevicePtr target = state_address(host, bytes, -1);
    if (!target) return 0;
    if (push_context() != 1) return -1;
    const int status = cuda.upload(target, (const void *) (uintptr_t) host, (size_t) bytes);
    return finish_context() == 0 && status == 0 ? 0 : -1;
}

static int64_t read_state(uint64_t host, int64_t bytes, int64_t kind) {
    const DevicePtr source = state_address(host, bytes, kind);
    if (!source) return 0;
    if (push_context() != 1) return -1;
    const int status = cuda.download((void *) (uintptr_t) host, source, (size_t) bytes);
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
    const DevicePtr key = state_address((uint64_t) (uintptr_t) k.data, k.len, 0);
    const DevicePtr value = state_address((uint64_t) (uintptr_t) v.data, v.len, 0);
    if (!key && !value) return 0;
    if (push_context() != 1) return -1;
    int status = key ? cuda.download(k.data, key, (size_t) k.len) : 0;
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
    DevicePtr destination = state_address(target, target_bytes, 0);
    if (!destination) return 0;
    const int active = start_context();
    if (active != 1) return active;

    DevicePtr input = 0;
    int error = activation_input((sl_u8) {(void *) (uintptr_t) source, source_bytes}, &cuda.input, &input);
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

int64_t fg_cuda_expert_upload(int64_t offset, uint64_t source, int64_t bytes) {
    const struct device_buffer * dest = &cuda.experts.weights;
    if (!dest->address || !source || offset < 0 || bytes <= 0 ||
        (uint64_t) offset > dest->bytes || (uint64_t) bytes > dest->bytes - (uint64_t) offset) return -1;
    const int status = cuda.upload(dest->address + (uint64_t) offset,
        (const void *) (uintptr_t) source, (size_t) bytes);
    if (status) fprintf(stderr, "Goose CUDA: expert upload failed (%d)\n", status);
    return status == 0 ? 0 : -1;
}

int64_t fg_cuda_prepare_experts(uint64_t tensor, int64_t bytes, sl_i64 active, sl_i64 offsets) {
    struct expert_cache * cache = &cuda.experts;
    if (!cache->weights.address) return 0;
    const uint64_t state = (uint64_t) (uintptr_t) cache->state;
    const int64_t status = ec_prepare(cache->groups, cache->layers, state, cache->work,
        tensor, bytes, active, offsets);
    if (status < 0) ec_reset(cache->groups, cache->layers, state, cache->work);
    return status;
}

int64_t fg_cuda_begin(uint64_t input, int64_t input_bytes, uint64_t output, int64_t output_bytes) {
    const int active = start_context();
    if (active != 1) return active;
    cuda_matrix_kind = -1;
    int status = activation_output(output, output_bytes, &matrix_output);
    if (status == 0) status = activation_input((sl_u8) {(void *) (uintptr_t) input, input_bytes}, &cuda.input, &matrix_input);
    if (status == 0) return 1;
    finish_context();
    fprintf(stderr, "Goose CUDA: input transfer failed (%d)\n", status);
    return -1;
}

static int launch_mmq(DevicePtr weight, uint64_t stride, int extent, int kind,
                      int rows, sl_i64 jobs, int job_words) {
    enum { window_tiles = 64, routes_per_tile = 8 };
    const int count = (int) (jobs.len / job_words);
    const size_t window_routes = count < window_tiles * routes_per_tile ?
        (size_t) count : window_tiles * routes_per_tile;
    if (extent > INT_MAX / 256 || (size_t) extent > SIZE_MAX / 288 / window_routes ||
        (size_t) count > SIZE_MAX / (3 * sizeof(int64_t))) return -1;
    const size_t packed_bytes = window_routes * (size_t) extent * 288;
    int64_t * tiles = malloc((size_t) count * 3 * sizeof(*tiles));
    if (!tiles) return -1;
    const int64_t tile_count = fg143_mmq_tiles(jobs, job_words, (sl_i64) {tiles, 3ll * count});
    if (tile_count <= 0 || tile_count > count) {
        free(tiles);
        return -1;
    }
    const size_t tile_bytes = (size_t) tile_count * 3 * sizeof(*tiles);
    int error = reserve(&cuda.mmq_packed, packed_bytes);
    if (error == 0) error = reserve(&cuda.mmq_tiles, tile_bytes);
    if (error == 0) error = cuda.upload(cuda.mmq_tiles.address, tiles, tile_bytes);
    int first = 0, first_route = 0;
    void * pack_args[] = {&matrix_input, &cuda.jobs.address, &cuda.mmq_packed.address,
                         &extent, &kind, &first_route, &job_words};
    void * matrix_args[] = {&weight, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address,
        &cuda.mmq_tiles.address, &stride, &extent, &kind, &rows, &first, &first_route, &job_words};
    while (error == 0 && first < tile_count) {
        const int batch = tile_count - first > window_tiles ? window_tiles : (int) (tile_count - first);
        const int64_t last = 3ll * (first + batch - 1);
        first_route = (int) tiles[3ll * first];
        const unsigned routes = (unsigned) (tiles[last] + tiles[last + 1] - first_route);
        error = cuda.launch(cuda.mmq_pack, (unsigned) (2 * extent), routes, 1,
                            32, 1, 1, 0, NULL, pack_args, NULL);
        if (error == 0) {
            error = cuda.launch(cuda.mmq_matrix, 1u + ((unsigned) rows - 1) / 64, (unsigned) batch, 1,
                                128, 1, 1, 0, NULL, matrix_args, NULL);
            if (error == 0) ++cuda_matrix_launches;
        }
        first += batch;
    }
    free(tiles);
    if (first != 0) {
        const int sync_error = cuda.sync();
        if (error == 0) error = sync_error;
    }
    if (error != 0) {
        fprintf(stderr, "Goose CUDA: MMQ failed (%d), packed activations need %zu bytes\n", error, packed_bytes);
        return -1;
    }
    ++cuda_mmq_projections;
    return 0;
}

static int launch_matrix(DevicePtr weight, int64_t row_stride, int64_t extent,
                         int64_t kind, int64_t rows, sl_i64 jobs, int job_words) {
    if (!weight || rows <= 0 || rows > INT_MAX || extent <= 0 || extent > INT_MAX ||
        (kind != 0 && kind != 12 && kind != 14) ||
        jobs.len <= 0 || jobs.len % job_words != 0 || jobs.len / job_words > INT_MAX ||
        (uint64_t) jobs.len > SIZE_MAX / sizeof(*jobs.data)) return -1;
    const size_t job_bytes = (size_t) jobs.len * sizeof(*jobs.data);
    int error = reserve(&cuda.jobs, job_bytes);
    if (error == 0) error = cuda.upload(cuda.jobs.address, jobs.data, job_bytes);
    if (error == 0 && cuda.mmq_matrix && kind != 0 && jobs.len / job_words >= 16) {
        const int result = launch_mmq(weight, (uint64_t) row_stride, (int) extent, (int) kind,
                                      (int) rows, jobs, job_words);
        if (result == 0) cuda_matrix_kind = (int) kind;
        return result;
    }

    uint64_t stride = (uint64_t) row_stride;
    int width = (int) extent, type = (int) kind, start = 0;
    void * args[] = { &weight, &matrix_input, &matrix_output,
                      &cuda.jobs.address, &stride, &width, &type, &start, &job_words };
    const unsigned lanes = kind == 14 ? 64 : 32;
    const int count = (int) (jobs.len / job_words);
    while (error == 0 && start < count) {
        const int batch = count - start > 65535 ? 65535 : count - start;
        error = cuda.launch(cuda.kernel, (unsigned) rows, (unsigned) batch, 1,
                            lanes, 1, 1, 0, NULL, args, NULL);
        if (error == 0) ++cuda_matrix_launches;
        start += batch;
    }
    if (start != 0) {
        const int sync_error = cuda.sync();
        if (error == 0) error = sync_error;
    }
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
    return launch_matrix(weight, row_stride, extent, kind, rows, jobs, 2);
}

int64_t fg_cuda_expert_batch(int64_t weight_bytes, int64_t row_stride,
                            int64_t extent, int64_t kind, int64_t rows, sl_i64 jobs) {
    const struct device_buffer * bank = &cuda.experts.weights;
    if (!bank->address || weight_bytes <= 0 || (uint64_t) weight_bytes > bank->bytes ||
        jobs.len <= 0 || jobs.len % 3 != 0 || jobs.len / 3 > INT_MAX) return -1;
    for (int64_t i = 2; i < jobs.len; i += 3) {
        const int64_t offset = jobs.data[i];
        if (offset < 0 || (uint64_t) offset > bank->bytes - (uint64_t) weight_bytes) return -1;
    }
    const int status = launch_matrix(bank->address, row_stride, extent, kind, rows, jobs, 3);
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

int64_t fg_cuda_finish(uint64_t output, int64_t output_bytes) {
    int error = 0;
    if (output) {
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

int64_t fg_cuda_q4_attention(sl_i64 config, sl_u8 q, sl_u8 k, sl_u8 v, sl_u8 mask, sl_u8 out, float scale) {
    if (config.len != 22 || config.data[17] <= 0 || config.data[17] > 65535 ||
        config.data[18] <= 0 || config.data[18] > INT_MAX ||
        config.data[19] <= 0 || config.data[19] > config.data[18] || config.data[20] <= 0 ||
        config.data[20] % config.data[19] != 0 || (uint64_t) config.data[20] > SIZE_MAX ||
        q.len <= 0 || k.len <= 0 || v.len <= 0 || mask.len < 0 || out.len <= 0) return -1;
    const int active = start_context();
    if (active != 1) return active;

    DevicePtr key = state_address((uint64_t) (uintptr_t) k.data, k.len, 0);
    DevicePtr value = state_address((uint64_t) (uintptr_t) v.data, v.len, 0);
    const bool resident = key != 0 && value != 0;
    const sl_u8 data[] = {q, key ? (sl_u8) {0} : k, value ? (sl_u8) {0} : v, mask,
        {(uint8_t *) config.data, config.len * (int64_t) sizeof(*config.data)}};
    struct device_buffer * buffers[] = {&cuda.input, &cuda.weight, &cuda.attention_value, &cuda.jobs, &cuda.attention_config};
    DevicePtr staged[] = {0, key, value, 0, 0}, output = 0;
    const uint64_t host = (uint64_t) (uintptr_t) out.data;
    int error = activation_output(host, out.len, &output);
    if (error == 0) error = upload_inputs(data, buffers, staged, sizeof(data) / sizeof(*data));

    const int tasks = (int) config.data[18];
    const int requested = (int) config.data[19];
    const size_t task_bytes = (size_t) (config.data[20] / requested);
    int capacity = requested;
    if (error == 0) {
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
    while (error == 0 && start < tasks) {
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
    if (config.data[21] > 0) ++cuda_causal_attentions;
    if (resident) ++cuda_resident_kv;
    return 1;
}

uint64_t fg_cuda_take_attentions(void) {
    const uint64_t attentions = cuda_attentions;
    cuda_attentions = 0;
    return attentions;
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
    if (error == 0) error = cuda.upload(cuda.attention_config.address, packet, sizeof packet);
    void * args[] = {&cuda.attention_config.address};
    if (error == 0) {
        error = cuda.launch(cuda.gdn, (unsigned) plan.data[25], (unsigned) plan.data[27],
            (unsigned) plan.data[34], (unsigned) (plan.data[35] * plan.data[36]),
            1, 1, 0, NULL, args, NULL);
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
    DevicePtr staged[3] = {0}, output = 0;
    const uint64_t host = (uint64_t) (uintptr_t) out.data;
    int error = 0;
    if (plan[23] & 4) {
        error = reserve(&cuda.output, (size_t) out.len);
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
    void * args[] = {&staged[0], &staged[1], &output, &staged[2], &param0, &param1};
    if (error == 0) {
        error = cuda.launch(cuda.vector, (unsigned) config.data[24], 1, 1,
            (unsigned) config.data[25], 1, 1, 0, NULL, args, NULL);
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
            error = cuda.launch(cuda.ssm_conv, gx, gy, 1, 128, 1, 1, 0, NULL, args, NULL);
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
    if (config.len != 22 || config.data[0] < 0 || config.data[0] >= 4 ||
        config.data[1] <= 0 || config.data[1] > INT_MAX ||
        (config.data[2] != 32 && config.data[2] != 128) || x.len <= 0 || y.len < 0 || out.len <= 0) return -1;
    const int active = start_context();
    if (active != 1) return active;
    const int64_t op = config.data[0];
    const Function operations[] = {cuda.route_reduce, cuda.route_sort, cuda.route_gather, cuda.route_reduce};
    const sl_u8 data[] = {x, y, {(uint8_t *) config.data, config.len * (int64_t) sizeof(*config.data)}};
    struct device_buffer * buffers[] = {&cuda.input, &cuda.weight, &cuda.attention_config};
    DevicePtr staged[3] = {0}, output = 0;
    const uint64_t host = (uint64_t) (uintptr_t) out.data;
    int error = activation_output(host, out.len, &output);
    if (error == 0) error = upload_inputs(data, buffers, staged, sizeof(data) / sizeof(*data));
    if (error == 0) {
        void * args[] = {&staged[0], &staged[1], &output, &staged[2]};
        error = cuda.launch(operations[op], (unsigned) config.data[1], 1, 1,
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
