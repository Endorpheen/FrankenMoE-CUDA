// Differential for the Goose MMQ tile loop against the native one (EXP235).
//
// Both run on identical buffers in the same kernel: native writes sum_n, Goose writes sum_g, then the
// host compares. This is the check the tile port needs before any performance claim, because its indexing
// is dense enough that three defects were found by inspection alone.
#include "common.cuh"
#include "mmq.cuh"
#include "vecdotq.cuh"
#include "ggml-common.h"

#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

// ---- Goose device services (names exactly as the emission calls them) ----
static __device__ __forceinline__ void fg235t_fail(int64_t){ asm("trap;"); }
static __device__ __forceinline__ int64_t fg235t_ld_i32(uint64_t b, int64_t i){ return (int64_t)((const int32_t*)(uintptr_t)b)[i]; }
static __device__ __forceinline__ float   fg235t_ld_f32(uint64_t b, int64_t i){ return ((const float*)(uintptr_t)b)[i]; }
static __device__ __forceinline__ void    fg235t_st_f32(uint64_t b, int64_t i, float v){ ((float*)(uintptr_t)b)[i] = v; }
static __device__ __forceinline__ int64_t gg_div(int64_t a, int64_t b){ if(!b) fg235t_fail(1); return a/b; }
static __device__ __forceinline__ int64_t gg_mod(int64_t a, int64_t b){ if(!b) fg235t_fail(1); return a%b; }
static __device__ __forceinline__ int64_t fg236t_fail(int64_t){ asm("trap;"); }
static __device__ __forceinline__ int64_t fg236t_ld_i32(uint64_t b,int64_t i){ return (int64_t)((const int32_t*)(uintptr_t)b)[i]; }
static __device__ __forceinline__ int64_t fg236t_ld_u8 (uint64_t b,int64_t i){ return (int64_t)((const uint8_t*)(uintptr_t)b)[i]; }
static __device__ __forceinline__ float   fg236t_ld_f32(uint64_t b,int64_t i){ return ((const float*)(uintptr_t)b)[i]; }
static __device__ __forceinline__ void    fg236t_st_f32(uint64_t b,int64_t i,float v){ ((float*)(uintptr_t)b)[i]=v; }
static __device__ __forceinline__ float   fg236t_to_f32(int64_t v){ return (float)v; }
static __device__ __forceinline__ int64_t fg236t_dp4a(int64_t a,int64_t b,int64_t c){ return (int64_t)__dp4a((int)a,(int)b,(int)c); }
static __device__ __forceinline__ float   fg236t_dm_x(uint64_t t,int64_t i){ return __half22float2(((const half2*)(uintptr_t)t)[i]).x; }
static __device__ __forceinline__ float   fg236t_dm_y(uint64_t t,int64_t i){ return __half22float2(((const half2*)(uintptr_t)t)[i]).y; }
static __device__ __forceinline__ int64_t fg235t_dp4a(int64_t a,int64_t b,int64_t c){ return (int64_t)__dp4a((int)a,(int)b,(int)c); }
static __device__ __forceinline__ void fg235t_split_h2_unused(uint64_t src, int64_t idx, uint64_t dx, uint64_t dy){
    const float2 f = __half22float2(((const half2*)(uintptr_t)src)[idx]);
    *(float*)(uintptr_t)dx = f.x;
    *(float*)(uintptr_t)dy = f.y;
}
// The inner product, already proven bit-exact against native, declared as a primitive so this file tests
// the TILE logic only.
static __device__ __forceinline__ float fg235t_vd(uint64_t v,uint64_t u,uint64_t sc,uint64_t m,
                                                  float dm4x,float dm4y,uint64_t dsx,uint64_t dsy){
    const float * sx = (const float*)(uintptr_t)dsx;
    const float * sy = (const float*)(uintptr_t)dsy;
    half2 dm4 = __floats2half2_rn(dm4x, dm4y);
    half2 ds8[2];
    ds8[0] = __floats2half2_rn(sx[0], sy[0]);
    ds8[1] = __floats2half2_rn(sx[1], sy[1]);
    return vec_dot_q4_K_q8_1_impl_mmq((const int*)(uintptr_t)v, (const int*)(uintptr_t)u,
                                      (const uint8_t*)(uintptr_t)sc, (const uint8_t*)(uintptr_t)m,
                                      dm4, ds8);
}

#include "tile2-generated.cuh"

// ---- the two arms ----
// Fixed geometry chosen to exercise the loop bounds: Q4_K dp4a config on sm_89 gives I and nthreads;
// we do not depend on the exact values, we merely pass consistent ones through.
struct Geo { int J, nwarps, I, warp_size, tile_ne_k, i8_1; };

static __global__ void fg235t_run(const int* x_qs, const half2* x_dm, const int* x_sc,
                                  const int* y_qs, const half2* y_ds,
                                  float* sum_n, float* sum_g,
                                  int k00, Geo g, int tx, int ty, float* scratch) {
    if (threadIdx.x != tx || threadIdx.y != ty) return;
    // native
    const int   * x_qs_n = x_qs;
    const half2 * x_dm_n = x_dm;
    // Native computes y_qs = (const int*)y + 4. Both arms must see the SAME pointer, so the Goose arm is
    // handed this one too -- my first harness gave then different bases and "found" differences that were
    // its own bug.
    const int   * y_qs_n = y_qs + 4;
    const half2 * y_ds_n = y_ds;
    constexpr int TILE = 32;
    for (int k01 = 0; k01 < TILE; k01 += 16) {
        const int k0 = k00 + k01;
        for (int j0 = 0; j0 < g.J; j0 += g.nwarps) {
            const int j = j0 + threadIdx.y;
            for (int i0 = 0; i0 < g.I; i0 += g.warp_size) {
                const int i = i0 + threadIdx.x;
                const uint8_t * sc = (const uint8_t *) &x_sc[i * (TILE/8) + i/8 + k0/32] + 2*(k01/16);
                sum_n[j0/g.nwarps*g.I/g.warp_size + i0/g.warp_size] += vec_dot_q4_K_q8_1_impl_mmq(
                    &x_qs_n[i*(TILE + 1) + k0/2], &y_qs_n[j*(TILE + TILE/8) + k01], sc, sc+8,
                    x_dm_n[i], &y_ds_n[j*(TILE + TILE/8) + k01/8]);
            }
        }
    }
    // goose
    uint64_t sc_base = (uint64_t)(uintptr_t) x_sc;
    gg236_function_1((uint64_t)(uintptr_t) x_qs, (uint64_t)(uintptr_t) x_dm, sc_base,
                  (uint64_t)(uintptr_t)(y_qs + 4), (uint64_t)(uintptr_t) y_ds,
                  (uint64_t)(uintptr_t) sum_g, (int64_t) k00, (int64_t) g.J, (int64_t) g.nwarps,
                  (int64_t) g.I, (int64_t) g.warp_size, (int64_t) g.tile_ne_k, (int64_t) g.i8_1,
                  (int64_t) tx, (int64_t) ty, 0);
}

// Timing: repeat the whole tile many times in one launch so the measurement is not dominated by launch.
static __global__ void fg235t_time(const int* x_qs, const half2* x_dm, const int* x_sc,
                                   const int* y_qs, const half2* y_ds, float* sum,
                                   int k00, Geo g, int tx, int ty, float* scratch, int rep, int arm) {
    if (threadIdx.x != tx || threadIdx.y != ty) return;
    constexpr int TILE = 32;
    for (int r = 0; r < rep; ++r) {
        if (arm == 0) {
            const int* yb = y_qs + 4;
            for (int k01 = 0; k01 < TILE; k01 += 16) {
                const int k0 = k00 + k01;
                for (int j0 = 0; j0 < g.J; j0 += g.nwarps) {
                    const int j = j0 + threadIdx.y;
                    for (int i0 = 0; i0 < g.I; i0 += g.warp_size) {
                        const int i = i0 + threadIdx.x;
                        const uint8_t * sc = (const uint8_t *) &x_sc[i*(TILE/8) + i/8 + k0/32] + 2*(k01/16);
                        sum[j0/g.nwarps*g.I/g.warp_size + i0/g.warp_size] += vec_dot_q4_K_q8_1_impl_mmq(
                            &x_qs[i*(TILE + 1) + k0/2], &yb[j*(TILE + TILE/8) + k01], sc, sc+8,
                            x_dm[i], &y_ds[j*(TILE + TILE/8) + k01/8]);
                    }
                }
            }
        } else {
            gg236_function_1((uint64_t)(uintptr_t) x_qs, (uint64_t)(uintptr_t) x_dm, (uint64_t)(uintptr_t) x_sc,
                          (uint64_t)(uintptr_t)(y_qs + 4), (uint64_t)(uintptr_t) y_ds,
                          (uint64_t)(uintptr_t) sum, (int64_t) k00, (int64_t) g.J, (int64_t) g.nwarps,
                          (int64_t) g.I, (int64_t) g.warp_size, (int64_t) g.tile_ne_k, (int64_t) g.i8_1,
                          (int64_t) tx, (int64_t) ty, 0);
        }
    }
}

int main() {
    { int n = 0; if (cudaGetDeviceCount(&n) != cudaSuccess || n < 1) { fprintf(stderr,"no CUDA device\n"); return 77; } }
    cudaSetDevice(0);

    constexpr int TILE = 32, NE = TILE + TILE/8;
    const int I = 8, J = 8, nwarps = 1, warp_size = 32, i8_1 = 8;
    const int max_off = 4096;

    std::vector<int32_t> h_qs((size_t)max_off*4 + 64), h_sc((size_t)max_off*4 + 64);
    std::vector<half2> h_dm((size_t)max_off + 64), h_yds((size_t)max_off + 64);
    std::mt19937 rng(235999);
    for (auto& v : h_qs) v = (int32_t) rng();
    for (auto& v : h_sc) v = (int32_t) rng();
    for (auto& v : h_dm)  v = __floats2half2_rn((float)((int)(rng()%2001)-1000)/64.f, (float)((int)(rng()%2001)-1000)/64.f);
    for (auto& v : h_yds) v = __floats2half2_rn((float)((int)(rng()%2001)-1000)/64.f, (float)((int)(rng()%2001)-1000)/64.f);

    int32_t *d_qs,*d_sc; half2 *d_dm,*d_yds; float *s_n,*s_g,*s_scr;
    cudaMalloc(&d_qs, h_qs.size()*4); cudaMalloc(&d_sc, h_sc.size()*4);
    cudaMalloc(&d_dm, h_dm.size()*4); cudaMalloc(&d_yds, h_yds.size()*4);
    cudaMalloc(&s_n, 256*4); cudaMalloc(&s_g, 256*4); cudaMalloc(&s_scr, 256*4);
    cudaMemcpy(d_qs,h_qs.data(),h_qs.size()*4,cudaMemcpyHostToDevice);
    cudaMemcpy(d_sc,h_sc.data(),h_sc.size()*4,cudaMemcpyHostToDevice);
    cudaMemcpy(d_dm,h_dm.data(),h_dm.size()*4,cudaMemcpyHostToDevice);
    cudaMemcpy(d_yds,h_yds.data(),h_yds.size()*4,cudaMemcpyHostToDevice);

    Geo g{J, nwarps, I, warp_size, TILE, i8_1};
    int bad = 0, cases = 0;
    for (int k00 = 0; k00 < 32; k00 += 16) {
        cudaMemset(s_n, 0, 256*4); cudaMemset(s_g, 0, 256*4);
        // y_qs in native is `(const int*)y + 4`, so pass the base such that the +4 offset is safe.
        fg235t_run<<<1, dim3(32,1)>>>(d_qs, d_dm, d_sc, d_qs, d_yds, s_n, s_g, k00, g, 0, 0, s_scr);
        auto e = cudaDeviceSynchronize();
        if (e != cudaSuccess) { fprintf(stderr, "launch %d failed: %s\n", k00, cudaGetErrorString(e)); return 1; }
        std::vector<float> hn(256), hg(256);
        cudaMemcpy(hn.data(), s_n, 256*4, cudaMemcpyDeviceToHost);
        cudaMemcpy(hg.data(), s_g, 256*4, cudaMemcpyDeviceToHost);
        for (int i = 0; i < 256; ++i) {
            ++cases;
            const double d = std::fabs((double)hn[i] - (double)hg[i]);
            if (d > 1e-4 * (1.0 + std::fabs((double)hn[i]))) {
                if (bad < 6) printf("  k00=%d idx=%d native=%.6g goose=%.6g\n", k00, i, hn[i], hg[i]);
                ++bad;
            }
        }
    }
    printf("FG235_TILE_MATCH cases=%d bad=%d\n", cases, bad);

    if (getenv("FG235_TILE_TIME")) {
        const int rep = 20000;
        std::vector<double> samples;
        for (int arm = 0; arm < 2; ++arm) {
            for (int it = 0; it < 20; ++it) {
                cudaMemset(s_n, 0, 256*4);
                cudaEvent_t a, b; cudaEventCreate(&a); cudaEventCreate(&b);
                cudaEventRecord(a);
                fg235t_time<<<1, dim3(32,1)>>>(d_qs, d_dm, d_sc, d_qs, d_yds, s_n, 0, g, 0, 0, s_scr, rep, arm);
                cudaEventRecord(b); cudaEventSynchronize(b);
                float ms = 0; cudaEventElapsedTime(&ms, a, b);
                samples.push_back(ms * 1e6 / rep);
                cudaEventDestroy(a); cudaEventDestroy(b);
            }
            std::sort(samples.begin(), samples.end());
            printf("FG235_TILE_TIMING arm=%s median_ns_per_tile=%.1f min=%.1f max=%.1f rep=%d\n",
                   arm ? "goose" : "native", samples[samples.size()/2], samples.front(), samples.back(), rep);
        }
    }
    return bad ? 1 : 0;
}
