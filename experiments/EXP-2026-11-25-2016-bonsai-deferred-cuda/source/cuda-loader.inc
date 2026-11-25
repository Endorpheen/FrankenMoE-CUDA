#include <dlfcn.h>
#include <pthread.h>

#define BONSAI_CUDA_API(X) \
    X(cuInit) \
    X(cuDeviceGet) \
    X(cuCtxCreate) \
    X(cuCtxDestroy) \
    X(cuCtxPushCurrent) \
    X(cuCtxPopCurrent) \
    X(cuCtxSynchronize) \
    X(cuModuleLoad) \
    X(cuModuleGetFunction) \
    X(cuMemAlloc) \
    X(cuMemGetInfo) \
    X(cuMemcpyHtoD) \
    X(cuMemcpyDtoH) \
    X(cuMemsetD8) \
    X(cuFuncSetAttribute) \
    X(cuGetErrorString) \
    X(cuLaunchKernel)

static struct bonsai_cuda_api {
#define BONSAI_DECLARE(name) __typeof__(name) *fn_##name;
    BONSAI_CUDA_API(BONSAI_DECLARE)
#undef BONSAI_DECLARE
    void *library;
} bonsai_cuda;

#define BONSAI_STRING_(name) #name
#define BONSAI_STRING(name) BONSAI_STRING_(name)

static void bonsai_bind_cuda(void) {
    void *library = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        fprintf(stderr, "Bonsai CUDA driver: %s\n", dlerror());
        return;
    }
#define BONSAI_RESOLVE(name) \
    do { \
        bonsai_cuda.fn_##name = (__typeof__(name) *)dlsym(library, BONSAI_STRING(name)); \
        if (!bonsai_cuda.fn_##name) { \
            fprintf(stderr, "Bonsai CUDA driver missing %s\n", BONSAI_STRING(name)); \
            dlclose(library); \
            return; \
        } \
    } while (0);
    BONSAI_CUDA_API(BONSAI_RESOLVE)
#undef BONSAI_RESOLVE
    bonsai_cuda.library = library;
}

static int bonsai_cuda_ready(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    return pthread_once(&once, bonsai_bind_cuda) == 0 && bonsai_cuda.library != NULL;
}
