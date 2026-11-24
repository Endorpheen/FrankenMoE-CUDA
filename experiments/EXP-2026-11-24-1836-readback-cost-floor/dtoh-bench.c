// Micro-benchmark: what does one small device-to-host read cost when the device
// is idle? Separates call overhead from queue wait.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

typedef void * CUdeviceptr;
typedef struct CUctx_st * CUcontext;
typedef struct CUevent_st * CUevent;
typedef struct CUstream_st * CUstream;

static int64_t now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t) t.tv_sec * 1000000000ll + t.tv_nsec; }

int main(void) {
    void * lib = dlopen("libcuda.so.1", RTLD_NOW);
    if (!lib) { printf("no libcuda\n"); return 1; }
    int (*cuInit)(unsigned) = dlsym(lib, "cuInit");
    int (*dev_get)(int *, int) = dlsym(lib, "cuDeviceGet");
    int (*ctx_create)(CUcontext *, unsigned, int) = dlsym(lib, "cuCtxCreate_v2");
    int (*mem_alloc)(CUdeviceptr *, size_t) = dlsym(lib, "cuMemAlloc_v2");
    int (*mem_free)(CUdeviceptr) = dlsym(lib, "cuMemFree_v2");
    int (*h2d)(CUdeviceptr, const void *, size_t) = dlsym(lib, "cuMemcpyHtoD_v2");
    int (*d2h)(void *, CUdeviceptr, size_t) = dlsym(lib, "cuMemcpyDtoH_v2");
    int (*d2h_async)(void *, CUdeviceptr, size_t, CUstream) = dlsym(lib, "cuMemcpyDtoHAsync_v2");
    int (*host_alloc)(void **, size_t, unsigned) = dlsym(lib, "cuMemHostAlloc");
    int (*host_free)(void *) = dlsym(lib, "cuMemFreeHost");
    int (*ev_create)(CUevent *, unsigned) = dlsym(lib, "cuEventCreate");
    int (*ev_record)(CUevent, CUstream) = dlsym(lib, "cuEventRecord");
    int (*ev_sync)(CUevent) = dlsym(lib, "cuEventSynchronize");
    if (!cuInit || !dev_get || !ctx_create || !mem_alloc || !d2h) { printf("symbols missing\n"); return 1; }
    cuInit(0);
    int dev = 0; dev_get(&dev, 0);
    CUcontext ctx = NULL; ctx_create(&ctx, 0, dev);
    enum { BYTES = 2048, N = 2000 };
    CUdeviceptr src = 0;
    if (mem_alloc(&src, 1 << 20) != 0) { printf("alloc failed\n"); return 1; }
    unsigned char * pageable = malloc(BYTES);
    unsigned char * pinned = NULL;
    host_alloc((void **) &pinned, 1 << 20, 0);
    unsigned char * send = calloc(1, 1 << 20);
    h2d(src, send, 1 << 20);

    // warm up
    for (int i = 0; i < 50; ++i) d2h(pageable, src, BYTES);

    int64_t t0 = now_ns();
    for (int i = 0; i < N; ++i) d2h(pageable, src, BYTES);
    int64_t pageable_ns = now_ns() - t0;

    t0 = now_ns();
    for (int i = 0; i < N; ++i) d2h(pinned, src, BYTES);
    int64_t pinned_ns = now_ns() - t0;

    CUevent ev = NULL;
    ev_create(&ev, 2);
    t0 = now_ns();
    for (int i = 0; i < N; ++i) { d2h_async(pinned, src, BYTES, NULL); ev_record(ev, NULL); ev_sync(ev); }
    int64_t async_ns = now_ns() - t0;

    // 16 reads gathered into one wait: one event for the batch
    t0 = now_ns();
    for (int i = 0; i < N / 16; ++i) {
        for (int k = 0; k < 16; ++k) d2h_async(pinned + k * BYTES, src + k * BYTES, BYTES, NULL);
        ev_record(ev, NULL); ev_sync(ev);
    }
    int64_t batch_ns = now_ns() - t0;

    printf("2 KB DtoH, device idle, %d calls:\n", N);
    printf("  pageable blocking   %8.2f us/call\n", pageable_ns / 1000.0 / N);
    printf("  pinned blocking     %8.2f us/call\n", pinned_ns / 1000.0 / N);
    printf("  pinned async+event  %8.2f us/call\n", async_ns / 1000.0 / N);
    printf("  16 gathered + 1 ev  %8.2f us/16 calls (%.2f us/call)\n", batch_ns / 1000.0 / (N / 16), batch_ns / 1000.0 / N);
    if (ev) { int (*ev_destroy)(CUevent) = dlsym(lib, "cuEventDestroy_v2"); if (ev_destroy) ev_destroy(ev); }
    if (pinned && host_free) host_free(pinned);
    if (mem_free) mem_free(src);
    return 0;
}
