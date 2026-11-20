#include "cpu.h"
#include "goose-cuda-image.h"

#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int64_t dw_cuda_dense_offset(sl_i64 ranges, uint64_t address, int64_t bytes);
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
    Function kernel;
    struct device_buffer input, output, weight, jobs;
    struct device_buffer dense;
    sl_i64 dense_ranges;
    struct expert_cache experts;
    int status;
    int (*init)(unsigned);
    int (*device_get)(int *, int);
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
    int (*launch)(Function, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned,
                  unsigned, void *, void **, void **);
    int (*sync)(void);
} cuda;

static pthread_once_t cuda_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t cuda_lock = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local uint64_t cuda_matrices;

static void free_expert_metadata(struct expert_cache * cache) {
    free(cache->groups.data);
    free(cache->layers.data);
    free(cache->work.data);
    free(cache->state);
}

static void cleanup(void) {
    if (cuda.context) cuda.destroy(cuda.context);
    free(cuda.dense_ranges.data);
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
    LOAD(launch, "cuLaunchKernel");
    LOAD(sync, "cuCtxSynchronize");
#undef LOAD
    int device;
    if (cuda.init(0) != 0 || cuda.device_get(&device, 0) != 0) return;
    cuda.status = -1;
    int error = cuda.create(&cuda.context, 0, device);
    if (error == 0) {
        error = cuda.module_load(&cuda.module, goose_cuda_image);
        if (error == 0) error = cuda.function_get(&cuda.kernel, cuda.module, "goose_quantized_matmul");
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

static int reserve(struct device_buffer * buffer, size_t bytes) {
    if (bytes <= buffer->bytes) return 0;
    if (buffer->address) {
        const int error = cuda.release(buffer->address);
        if (error != 0) return error;
        buffer->address = 0;
        buffer->bytes = 0;
    }
    const int error = cuda.alloc(&buffer->address, bytes);
    if (error == 0) buffer->bytes = bytes;
    return error;
}

static int finish_context(void) {
    Context context = NULL;
    const int status = cuda.pop(&context);
    pthread_mutex_unlock(&cuda_lock);
    return status;
}

static int start_context(void) {
    const char * setting = getenv("GOOSE_CUDA");
    if (setting && setting[0] == '0' && !setting[1]) return 0;
    pthread_once(&cuda_once, init_cuda);
    if (cuda.status != 1) return cuda.status;
    pthread_mutex_lock(&cuda_lock);
    int status = cuda.push(cuda.context);
    if (status != 0) {
        pthread_mutex_unlock(&cuda_lock);
        fprintf(stderr, "Goose CUDA: context switch failed (%d)\n", status);
        return -1;
    }
    return 1;
}

int64_t fg_cuda_free_bytes(void) {
    const int active = start_context();
    if (active != 1) return active;
    size_t bytes = 0, total = 0;
    const int status = cuda.memory_info(&bytes, &total);
    return finish_context() == 0 && status == 0 && bytes <= INT64_MAX ? (int64_t) bytes : -1;
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

int64_t fg_cuda_begin(uint64_t input, int64_t input_bytes, int64_t output_bytes) {
    const int active = start_context();
    if (active != 1) return active;
    int status = reserve(&cuda.input, (size_t) input_bytes);
    if (status == 0) status = reserve(&cuda.output, (size_t) output_bytes);
    if (status == 0) status = cuda.upload(cuda.input.address, (const void *) (uintptr_t) input, (size_t) input_bytes);
    if (status == 0) return 1;
    finish_context();
    fprintf(stderr, "Goose CUDA: input transfer failed (%d)\n", status);
    return -1;
}

int64_t fg_cuda_plane(uint64_t weights, int64_t weight_bytes, int64_t row_stride,
                       int64_t blocks, int64_t kind, int64_t rows, sl_i64 jobs, int64_t expert_offset) {
    if (rows <= 0 || rows > INT_MAX || blocks <= 0 || blocks > INT_MAX ||
        jobs.len <= 0 || jobs.len % 2 != 0 || jobs.len / 2 > INT_MAX) return -1;
    const size_t job_bytes = (size_t) jobs.len * sizeof(*jobs.data);
    int error = reserve(&cuda.jobs, job_bytes);
    DevicePtr weight = 0;
    if (expert_offset >= 0) {
        const struct device_buffer * bank = &cuda.experts.weights;
        if (!bank->address || weight_bytes <= 0 || (uint64_t) expert_offset > bank->bytes ||
            (uint64_t) weight_bytes > bank->bytes - (uint64_t) expert_offset) return -1;
        weight = bank->address + (uint64_t) expert_offset;
    } else if (error == 0 && cuda.dense_ranges.len > 0) {
        const int64_t offset = dw_cuda_dense_offset(cuda.dense_ranges, weights, weight_bytes);
        if (offset >= 0) weight = cuda.dense.address + (uint64_t) offset;
    }
    if (error == 0 && weight == 0) {
        error = reserve(&cuda.weight, (size_t) weight_bytes);
        weight = cuda.weight.address;
        if (error == 0) error = cuda.upload(weight, (const void *) (uintptr_t) weights, (size_t) weight_bytes);
    }
    if (error == 0) error = cuda.upload(cuda.jobs.address, jobs.data, job_bytes);

    uint64_t stride = (uint64_t) row_stride;
    int width = (int) blocks, type = (int) kind, start = 0;
    void * args[] = { &weight, &cuda.input.address, &cuda.output.address,
                      &cuda.jobs.address, &stride, &width, &type, &start };
    const unsigned lanes = kind == 12 ? 32 : 64;
    const int count = (int) (jobs.len / 2);
    while (error == 0 && start < count) {
        const int batch = count - start > 65535 ? 65535 : count - start;
        error = cuda.launch(cuda.kernel, (unsigned) rows, (unsigned) batch, 1,
                            lanes, 1, 1, 0, NULL, args, NULL);
        start += batch;
    }
    if (start != 0) {
        const int sync_error = cuda.sync();
        if (error == 0) error = sync_error;
    }
    if (error != 0) fprintf(stderr, "Goose CUDA: matrix kernel failed (%d)\n", error);
    return error == 0 ? 0 : -1;
}

int64_t fg_cuda_finish(uint64_t output, int64_t output_bytes) {
    int error = 0;
    if (output) {
        error = cuda.download((void *) (uintptr_t) output, cuda.output.address, (size_t) output_bytes);
    }
    const int pop_error = finish_context();
    if (error == 0) error = pop_error;
    if (error != 0) {
        fprintf(stderr, "Goose CUDA: output transfer failed (%d)\n", error);
        return -1;
    }
    if (output) ++cuda_matrices;
    return 0;
}

uint64_t fg_cuda_take_matrices(void) {
    const uint64_t matrices = cuda_matrices;
    cuda_matrices = 0;
    return matrices;
}
