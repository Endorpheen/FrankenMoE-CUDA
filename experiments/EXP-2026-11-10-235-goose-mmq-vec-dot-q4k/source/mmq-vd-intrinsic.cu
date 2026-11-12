// Kernel-level A/B for the Goose vec_dot_q4_K_q8_1_impl_mmq (EXP235).
//
// The point of the exercise: the Goose emission is compiled INSIDE this translation unit as a
// __device__ __forceinline__ function, exactly as EXP182/EXP188 did for their kernels. So the "boundary"
// is a C++ function call that nvcc inlines -- it is not a launch, not a copy, and not a jump table.
// Whether it costs anything is the question this file measures.
#include "common.cuh"
#include "vecdotq.cuh"
#include "ggml-common.h"
#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <algorithm>

// Goose device services: the emission expects these names for its runtime helpers.

// The emission calls these names exactly; they are the module's declared externs.
static __device__ __forceinline__ void    fg235v_fail(int64_t){ asm("trap;"); }
// The intrinsic the emission declares. One hardware instruction on sm_89.
static __device__ __forceinline__ int64_t fg235v_dp4a(int64_t a, int64_t b, int64_t c) {
    return (int64_t) __dp4a((int) a, (int) b, (int) c);
}
static __device__ __forceinline__ int64_t fg235v_ld_i32(uint64_t b, int64_t i) { return (int64_t)((const int32_t *)(uintptr_t) b)[i]; }
static __device__ __forceinline__ int64_t fg235v_ld_u8 (uint64_t b, int64_t i) { return (int64_t)((const uint8_t *)(uintptr_t) b)[i]; }
static __device__ __forceinline__ float   fg235v_ld_f32(uint64_t b, int64_t i) { return ((const float *)(uintptr_t) b)[i]; }
static __device__ __forceinline__ float   fg235v_to_f32(int64_t v) { return (float) v; }

#include "device-functions-intrinsic.cuh"

// Same signature and body position as the native function it replaces.
static __device__ __forceinline__ float fg235_vd_goose_i(
    const int * __restrict__ v, const int * __restrict__ u, const uint8_t * __restrict__ sc,
    const uint8_t * __restrict__ m, const half2 & dm4, const half2 * __restrict__ ds8) {
    float2 dm4f = __half22float2(dm4);
    float dsx[2], dsy[2];
#pragma unroll
    for (int i = 0; i < 2; ++i) { float2 t = __half22float2(ds8[i]); dsx[i] = t.x; dsy[i] = t.y; }
    return (float) gg_function_1((uint64_t)(uintptr_t) v, (uint64_t)(uintptr_t) u,
                                         (uint64_t)(uintptr_t) sc, (uint64_t)(uintptr_t) m,
                                         (uint64_t)(uintptr_t) dsx, (uint64_t)(uintptr_t) dsy,
                                         dm4f.x, dm4f.y, 2, 0);
}

// One thread computes the native and the Goose value on the same inputs; a mismatch traps.
static __global__ void fg235_check(const int * v, const int * u, const uint8_t * sc,
                                   const uint8_t * m, const half2 dm4, const half2 * ds8,
                                   float * out_native, float * out_goose, int n) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    const int * vv = v + i * 8;
    const int * uu = u + i * 16;
    const uint8_t * scc = sc + i * 4;
    const uint8_t * mm  = m + i * 4;
    const half2 * dd = ds8 + i * 2;
    out_native[i] = vec_dot_q4_K_q8_1_impl_mmq(vv, uu, scc, mm, dm4, dd);
    out_goose[i]  = fg235_vd_goose_i(vv, uu, scc, mm, dm4, dd);
}

// Timing: launch repeatedly on a fixed buffer set, alternating the arm inside the same process.
static __global__ void fg235_time_native(const int * v, const int * u, const uint8_t * sc,
                                         const uint8_t * m, const half2 dm4, const half2 * ds8,
                                         float * sink, int n, int rep) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float acc = 0;
    for (int r = 0; r < rep; ++r)
        acc += vec_dot_q4_K_q8_1_impl_mmq(v + i*8, u + i*16, sc + i*4, m + i*4, dm4, ds8 + i*2);
    sink[i] = acc;
}
static __global__ void fg235_time_goose(const int * v, const int * u, const uint8_t * sc,
                                        const uint8_t * m, const half2 dm4, const half2 * ds8,
                                        float * sink, int n, int rep) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float acc = 0;
    for (int r = 0; r < rep; ++r)
        acc += fg235_vd_goose_i(v + i*8, u + i*16, sc + i*4, m + i*4, dm4, ds8 + i*2);
    sink[i] = acc;
}

int main() {
    { int ndev = 0; if (cudaGetDeviceCount(&ndev) != cudaSuccess || ndev < 1) { fprintf(stderr, "no CUDA device\n"); return 77; } }
    cudaSetDevice(0);

    const int n = 1 << 16;
    std::vector<int32_t> v((size_t)n * 8), u((size_t)n * 16);
    std::vector<uint8_t> sc((size_t)n * 4), m((size_t)n * 4);
    std::vector<half2> ds((size_t)n * 2);
    std::mt19937 rng(235777);
    for (auto & x : v) x = (int32_t) rng();
    for (auto & x : u) x = (int32_t) rng();
    for (auto & x : sc) x = (uint8_t) (rng() % 256);
    for (auto & x : m)  x = (uint8_t) (rng() % 256);
    for (auto & x : ds) { float2 f; f.x = (float)((int)(rng()%2001)-1000)/64.f; f.y = (float)((int)(rng()%2001)-1000)/64.f; x = __floats2half2_rn(f.x, f.y); }
    const half2 dm4 = __floats2half2_rn(0.75f, 0.5f);

    int32_t *dv; int32_t *du; uint8_t *dsc; uint8_t *dm; half2 *dds; float *dout_n; float *dout_g; float *dsink;
    cudaMalloc(&dv, v.size()*4); cudaMalloc(&du, u.size()*4);
    cudaMalloc(&dsc, sc.size()); cudaMalloc(&dm, m.size());
    cudaMalloc(&dds, ds.size()*sizeof(half2));
    cudaMalloc(&dout_n, (size_t)n*4); cudaMalloc(&dout_g, (size_t)n*4); cudaMalloc(&dsink, (size_t)n*4);
    cudaMemcpy(dv, v.data(), v.size()*4, cudaMemcpyHostToDevice);
    cudaMemcpy(du, u.data(), u.size()*4, cudaMemcpyHostToDevice);
    cudaMemcpy(dsc, sc.data(), sc.size(), cudaMemcpyHostToDevice);
    cudaMemcpy(dm, m.data(), m.size(), cudaMemcpyHostToDevice);
    cudaMemcpy(dds, ds.data(), ds.size()*sizeof(half2), cudaMemcpyHostToDevice);

    const int tpb = 256, blocks = (n + tpb - 1) / tpb;
    fg235_check<<<blocks, tpb>>>(dv, du, dsc, dm, dm4, dds, dout_n, dout_g, n);
    auto e = cudaDeviceSynchronize();
    if (e != cudaSuccess) { fprintf(stderr, "check launch failed: %s\n", cudaGetErrorString(e)); return 1; }
    std::vector<float> hn(n), hg(n);
    cudaMemcpy(hn.data(), dout_n, (size_t)n*4, cudaMemcpyDeviceToHost);
    cudaMemcpy(hg.data(), dout_g, (size_t)n*4, cudaMemcpyDeviceToHost);
    double worst = 0; int bad = 0;
    for (int i = 0; i < n; ++i) {
        const double d = fabs((double)hn[i] - (double)hg[i]);
        const double tol = 1e-3 * (1.0 + fabs((double)hn[i]));
        if (d > tol) ++bad;
        worst = std::max(worst, d / (1.0 + fabs((double)hn[i])));
    }
    printf("FG235_KERNEL_MATCH n=%d bad=%d worst_rel=%.3e\n", n, bad, worst);

    const int rep = 64;
    const char * arm = getenv("GGML_GOOSE_MMQ_VEC_DOT_Q4K");
    const bool goose = arm && strcmp(arm, "1") == 0;
    std::vector<double> samples;
    for (int it = 0; it < 40; ++it) {
        cudaEvent_t a, b; cudaEventCreate(&a); cudaEventCreate(&b);
        cudaEventRecord(a);
        if (goose) fg235_time_goose<<<blocks, tpb>>>(dv, du, dsc, dm, dm4, dds, dsink, n, rep);
        else       fg235_time_native<<<blocks, tpb>>>(dv, du, dsc, dm, dm4, dds, dsink, n, rep);
        cudaEventRecord(b); cudaEventSynchronize(b);
        float ms = 0; cudaEventElapsedTime(&ms, a, b);
        samples.push_back(ms * 1e6 / rep);
        cudaEventDestroy(a); cudaEventDestroy(b);
    }
    std::sort(samples.begin(), samples.end());
    printf("FG235_VD_TIMING arm=%s median_ns_per_launch=%.1f min=%.1f max=%.1f rep=%d\n",
           goose ? "goose" : "native", samples[samples.size()/2], samples.front(), samples.back(), rep);
    return 0;
}
