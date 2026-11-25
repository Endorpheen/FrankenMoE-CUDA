#include <assert.h>
#include <cuda.h>
#include <dlfcn.h>
#include <stdio.h>
#include <time.h>

static double seconds(void) {
    struct timespec ts;
    assert(clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

int main(void) {
    void *api = dlopen("libcuda.so.1", RTLD_NOW);
    assert(api);
    CUresult (*init)(unsigned) = dlsym(api, "cuInit");
    CUresult (*get)(CUdevice *, int) = dlsym(api, "cuDeviceGet");
    CUresult (*create)(CUcontext *, unsigned, CUdevice) = dlsym(api, "cuCtxCreate_v2");
    CUresult (*push)(CUcontext) = dlsym(api, "cuCtxPushCurrent_v2");
    CUresult (*pop)(CUcontext *) = dlsym(api, "cuCtxPopCurrent_v2");
    CUresult (*destroy)(CUcontext) = dlsym(api, "cuCtxDestroy_v2");
    assert(init && get && create && push && pop && destroy);
    CUdevice device;
    CUcontext context, previous;
    assert(init(0) == CUDA_SUCCESS);
    assert(get(&device, 0) == CUDA_SUCCESS);
    assert(create(&context, 0, device) == CUDA_SUCCESS);
    assert(pop(&previous) == CUDA_SUCCESS && previous == context);
    for (int repeat = 0; repeat < 4; ++repeat) {
        double start = seconds();
        for (int i = 0; i < 100000; ++i) {
            assert(push(context) == CUDA_SUCCESS);
            assert(pop(&previous) == CUDA_SUCCESS && previous == context);
        }
        printf("%.3f\n", (seconds() - start) * 1e6 / 100000);
    }
    assert(destroy(context) == CUDA_SUCCESS);
    dlclose(api);
}
