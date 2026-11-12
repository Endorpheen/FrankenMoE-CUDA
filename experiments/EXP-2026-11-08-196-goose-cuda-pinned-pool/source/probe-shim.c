// Probe: count the CUDA pinned-memory entry points so the port is only undertaken if the
// path executes in this profile. Loaded with LD_PRELOAD into the real server process.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

static _Atomic uint64_t c_mallochost, c_freehost, c_hostreg, c_hostunreg, c_memcpy_h2d, c_memcpy_d2h, c_d2d, c_peer, c_2d;
static _Atomic uint64_t b_mallochost;

static void report(void) {
    fprintf(stderr,
            "FG196_PINNED cudaMallocHost=%llu bytes=%llu cudaFreeHost=%llu "
            "cudaHostRegister=%llu cudaHostUnregister=%llu "
            "cudaMemcpyAsync_h2d=%llu cudaMemcpyAsync_d2h=%llu "
            "cudaMemcpyAsync_d2d=%llu cudaMemcpyPeerAsync=%llu cudaMemcpy2DAsync=%llu\n",
            (unsigned long long) atomic_load(&c_mallochost),
            (unsigned long long) atomic_load(&b_mallochost),
            (unsigned long long) atomic_load(&c_freehost),
            (unsigned long long) atomic_load(&c_hostreg),
            (unsigned long long) atomic_load(&c_hostunreg),
            (unsigned long long) atomic_load(&c_memcpy_h2d),
            (unsigned long long) atomic_load(&c_memcpy_d2h),
            (unsigned long long) atomic_load(&c_d2d),
            (unsigned long long) atomic_load(&c_peer),
            (unsigned long long) atomic_load(&c_2d));
}

static _Atomic int once;
static void arm(void) { if (!atomic_exchange(&once, 1)) atexit(report); }

int cudaMallocHost(void ** p, size_t n) {
    arm();
    atomic_fetch_add(&c_mallochost, 1);
    atomic_fetch_add(&b_mallochost, n);
    static int (*nx)(void **, size_t);
    if (!nx) nx = (int (*)(void **, size_t)) dlsym(RTLD_NEXT, "cudaMallocHost");
    return nx(p, n);
}

int cudaFreeHost(void * p) {
    arm();
    atomic_fetch_add(&c_freehost, 1);
    static int (*nx)(void *);
    if (!nx) nx = (int (*)(void *)) dlsym(RTLD_NEXT, "cudaFreeHost");
    return nx(p);
}

int cudaHostRegister(void * p, size_t n, unsigned f) {
    arm();
    atomic_fetch_add(&c_hostreg, 1);
    static int (*nx)(void *, size_t, unsigned);
    if (!nx) nx = (int (*)(void *, size_t, unsigned)) dlsym(RTLD_NEXT, "cudaHostRegister");
    return nx(p, n, f);
}

int cudaHostUnregister(void * p) {
    arm();
    atomic_fetch_add(&c_hostunreg, 1);
    static int (*nx)(void *);
    if (!nx) nx = (int (*)(void *)) dlsym(RTLD_NEXT, "cudaHostUnregister");
    return nx(p);
}

// Direction is the third argument of cudaMemcpyAsync: 1 = host to device, 2 = device to host.
int cudaMemcpyAsync(void * d, const void * s, size_t n, int kind, void * stream) {
    arm();
    if (kind == 1) atomic_fetch_add(&c_memcpy_h2d, 1);
    if (kind == 2) atomic_fetch_add(&c_memcpy_d2h, 1);
    if (kind == 3) atomic_fetch_add(&c_d2d, 1);
    static int (*nx)(void *, const void *, size_t, int, void *);
    if (!nx) nx = (int (*)(void *, const void *, size_t, int, void *)) dlsym(RTLD_NEXT, "cudaMemcpyAsync");
    return nx(d, s, n, kind, stream);
}

int cudaMemcpyPeerAsync(void * d, int dd, const void * s, int sd, size_t n, void * st) {
    arm();
    atomic_fetch_add(&c_peer, 1);
    static int (*nx)(void *, int, const void *, int, size_t, void *);
    if (!nx) nx = (int (*)(void *, int, const void *, int, size_t, void *)) dlsym(RTLD_NEXT, "cudaMemcpyPeerAsync");
    return nx(d, dd, s, sd, n, st);
}

int cudaMemcpy2DAsync(void * d, size_t dp, const void * s, size_t sp, size_t w, size_t h, int kind, void * st) {
    arm();
    atomic_fetch_add(&c_2d, 1);
    static int (*nx)(void *, size_t, const void *, size_t, size_t, size_t, int, void *);
    if (!nx) nx = (int (*)(void *, size_t, const void *, size_t, size_t, size_t, int, void *)) dlsym(RTLD_NEXT, "cudaMemcpy2DAsync");
    return nx(d, dp, s, sp, w, h, kind, st);
}
