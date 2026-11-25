#include <cuda.h>
#include <fenv.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void check_cuda(CUresult result, const char *call) {
    if (result == CUDA_SUCCESS) return;
    const char *message = NULL;
    cuGetErrorString(result, &message);
    fprintf(stderr, "%s: %d (%s)\n", call, result, message ? message : "unknown");
    exit(EXIT_FAILURE);
}
#define CUDA(call) check_cuda((call), #call)

static float f32_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static uint32_t f32_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

int main(int argc, char **argv) {
    enum { chunk = 1 << 20, lanes = 128 };
    const uint32_t last = 0x7f800000;
    if (argc != 2 || fegetround() != FE_TONEAREST) return EXIT_FAILURE;
    uint32_t *actual = malloc(chunk * sizeof *actual);
    if (!actual) return EXIT_FAILURE;
    CUdevice device;
    CUcontext context;
    CUmodule module;
    CUfunction kernel;
    CUdeviceptr output;
    CUDA(cuInit(0));
    CUDA(cuDeviceGet(&device, 0));
    CUDA(cuCtxCreate(&context, 0, device));
    CUDA(cuModuleLoad(&module, argv[1]));
    CUDA(cuModuleGetFunction(&kernel, module, "i2_check_depth_expf"));
    CUDA(cuMemAlloc(&output, chunk * sizeof *actual));
    struct timespec begin, end;
    clock_gettime(CLOCK_MONOTONIC, &begin);
    uint64_t differences = 0;
    const uint32_t one = f32_bits(expf(0.0f));
    for (uint32_t base = 0; base <= last;) {
        const uint32_t remaining = last - base + 1;
        uint32_t count = remaining < chunk ? remaining : chunk;
        void *args[] = { &output, &base, &count };
        CUDA(cuLaunchKernel(kernel, (count + lanes - 1) / lanes, 1, 1, lanes, 1, 1, 0, NULL, args, NULL));
        CUDA(cuMemcpyDtoH(actual, output, count * sizeof *actual));
        if (base == 0 && actual[0] != one) {
            fprintf(stderr, "Negative zero disagrees with the CPU result for positive zero\n");
            ++differences;
        }
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t bits = UINT32_C(0x80000000) | (base + i);
            const uint32_t expected = f32_bits(expf(f32_from_bits(bits)));
            if (expected == actual[i]) continue;
            if (differences < 10)
                fprintf(stderr, "%08" PRIx32 ": host %08" PRIx32 ", device %08" PRIx32 "\n",
                        bits, expected, actual[i]);
            ++differences;
        }
        base += count;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    const double seconds = (end.tv_sec - begin.tv_sec) + 1e-9 * (end.tv_nsec - begin.tv_nsec);
    printf("{\"negative_encodings_checked\":%" PRIu32 ",\"positive_zero_checked\":true,"
           "\"differences\":%" PRIu64 ",\"seconds\":%.6f}\n", last + 1, differences, seconds);
    CUDA(cuMemFree(output));
    CUDA(cuModuleUnload(module));
    CUDA(cuCtxDestroy(context));
    free(actual);
    return differences ? EXIT_FAILURE : EXIT_SUCCESS;
}
