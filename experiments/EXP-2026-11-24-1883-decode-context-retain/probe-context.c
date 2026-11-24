#include <cuda.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static uint64_t timestamp(void) {
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC_RAW, &value);
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) + value.tv_nsec;
}

int main(void) {
    CUdevice device;
    CUcontext context, popped;
    if (cuInit(0) || cuDeviceGet(&device, 0) ||
        cuDevicePrimaryCtxRetain(&context, device)) return 1;
    enum { iterations = 200000 };
    for (int block = 0; block < 6; ++block) {
        const uint64_t start = timestamp();
        for (int i = 0; i < iterations; ++i)
            if (cuCtxPushCurrent(context) || cuCtxPopCurrent(&popped) || popped != context) return 2;
        printf("block=%d ns_per_pair=%.3f\n", block, (timestamp() - start) / (double)iterations);
    }
    return cuDevicePrimaryCtxRelease(device) != CUDA_SUCCESS;
}
