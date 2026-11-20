#include "cpu.h"
#include "goose-cuda-image.h"

#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct CUctx_st * Context;
typedef struct CUmod_st * Module;
typedef struct CUfunc_st * Function;
typedef unsigned long long DevicePtr;

struct device_buffer {
    DevicePtr address;
    size_t bytes;
};

static struct {
    void * library;
    Context context;
    Module module;
    Function kernel;
    struct device_buffer input, output, weight, jobs;
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

static void cleanup(void) {
    if (cuda.context) cuda.destroy(cuda.context);
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

int64_t fg_cuda_begin(uint64_t input, int64_t input_bytes, int64_t output_bytes) {
    const char * setting = getenv("GOOSE_CUDA");
    if (setting && setting[0] == '0' && !setting[1]) return 0;
    pthread_once(&cuda_once, init_cuda);
    if (cuda.status != 1) return cuda.status;
    pthread_mutex_lock(&cuda_lock);
    int status = cuda.push(cuda.context);
    if (status != 0) {
        pthread_mutex_unlock(&cuda_lock);
    } else {
        status = reserve(&cuda.input, (size_t) input_bytes);
        if (status == 0) status = reserve(&cuda.output, (size_t) output_bytes);
        if (status == 0) status = cuda.upload(cuda.input.address, (const void *) (uintptr_t) input, (size_t) input_bytes);
        if (status == 0) return 1;
        finish_context();
    }
    fprintf(stderr, "Goose CUDA: input transfer failed (%d)\n", status);
    return -1;
}

int64_t fg_cuda_plane(uint64_t weights, int64_t weight_bytes, int64_t row_stride,
                       int64_t blocks, int64_t kind, int64_t rows, sl_i64 jobs) {
    if (rows <= 0 || rows > INT_MAX || blocks <= 0 || blocks > INT_MAX ||
        jobs.len <= 0 || jobs.len % 2 != 0 || jobs.len / 2 > INT_MAX) return -1;
    const size_t job_bytes = (size_t) jobs.len * sizeof(*jobs.data);
    int error = reserve(&cuda.weight, (size_t) weight_bytes);
    if (error == 0) error = reserve(&cuda.jobs, job_bytes);
    if (error == 0) error = cuda.upload(cuda.weight.address, (const void *) (uintptr_t) weights, (size_t) weight_bytes);
    if (error == 0) error = cuda.upload(cuda.jobs.address, jobs.data, job_bytes);

    uint64_t stride = (uint64_t) row_stride;
    int width = (int) blocks, type = (int) kind, start = 0;
    void * args[] = { &cuda.weight.address, &cuda.input.address, &cuda.output.address,
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
