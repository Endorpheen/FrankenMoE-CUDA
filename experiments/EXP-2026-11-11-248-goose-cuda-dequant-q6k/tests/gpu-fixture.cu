// GPU differential fixture for the Goose Q6_K dequantizer.
//
// The native oracle is the real `dequantize_q6_K` template compiled from the
// read-only source tree (ggml-cuda/dequantize.cuh). Both arms run on the GPU on
// the same random block bytes; the gate is bit-exact equality of the whole F32
// destination. Keeping the oracle in the test harness, not in the shipped DSO,
// is deliberate: this module is a prerequisite for MMQ and is not integrated.
#include "common.cuh"
#include "dequantize.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <string>

// Raw device services for the Goose module. The same primitives the native body
// uses: byte loads, a signed-byte load, a half unpack and a float store.
static __device__ __forceinline__ uint32_t fg248d_ld_u8(uint64_t p){ return *(const uint8_t*)(uintptr_t)p; }
static __device__ __forceinline__ int32_t  fg248d_ld_i8(uint64_t p){ return *(const int8_t*)(uintptr_t)p; }
static __device__ __forceinline__ float    fg248d_ld_half(uint64_t p){ return __half2float(*(const half*)(uintptr_t)p); }
static __device__ __forceinline__ void     fg248d_st_f32(uint64_t p,float v){ *(float*)(uintptr_t)p = v; }
static __device__ __forceinline__ void     fg248d_fail(int64_t){ asm("trap;"); }
#define FG248D_Q6K fg248dd_gg_function_1
#include "device-functions-generated.cuh"

static __global__ void native_kernel(const void * vx, float * yy){
    const int64_t i = blockIdx.x;
    dequantize_q6_K<float>(vx, i, yy + i*QK_K, threadIdx.x);
}
static __global__ void goose_kernel(const void * vx, float * yy){
    FG248D_Q6K((uint64_t)(uintptr_t)vx,(uint64_t)(uintptr_t)yy,
               (int64_t)blockIdx.x,(int64_t)threadIdx.x,0);
}

static int g_checks, g_fail;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fail; std::fprintf(stderr, "FAIL %s\n", what); }
}

static const size_t Q6K_BYTES = 210;
struct Case { int64_t nblocks; const char * name; };
static const Case CASES[] = {
    {  1, "one-block" },
    {  4, "four-blocks" },
    { 64, "sixty-four" },
    {256, "two-fifty-six" },
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    int ndev = 0;
    if (cudaGetDeviceCount(&ndev) != cudaSuccess || ndev < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937 rng(248011);
    const char * dump = getenv("FG248D_DUMP");

    for (const Case & c : CASES) {
        const size_t src_bytes = (size_t)c.nblocks * Q6K_BYTES;
        const size_t dst_n = (size_t)c.nblocks * (size_t)QK_K;
        std::vector<uint8_t> raw(src_bytes);
        for (auto & v : raw) v = (uint8_t)rng();
        // Random raw bytes exercise every 6-bit pattern and every signed scale,
        // including scales that make the affine term negative.
        void * dsrc = nullptr; float * dnat = nullptr; float * dgoo = nullptr;
        cudaMalloc(&dsrc, src_bytes);
        cudaMalloc(&dnat, dst_n*sizeof(float));
        cudaMalloc(&dgoo, dst_n*sizeof(float));
        cudaMemcpy(dsrc, raw.data(), src_bytes, cudaMemcpyHostToDevice);
        cudaMemset(dnat, 0xFF, dst_n*sizeof(float));
        cudaMemset(dgoo, 0xFF, dst_n*sizeof(float));

        cudaStream_t st; cudaStreamCreate(&st);
        native_kernel<<<(unsigned)c.nblocks,64,0,st>>>(dsrc,dnat);
        goose_kernel<<<(unsigned)c.nblocks,64,0,st>>>(dsrc,dgoo);
        check(cudaStreamSynchronize(st) == cudaSuccess, (std::string("sync ")+c.name).c_str());

        std::vector<float> nat(dst_n), goo(dst_n);
        cudaMemcpy(nat.data(), dnat, dst_n*sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(goo.data(), dgoo, dst_n*sizeof(float), cudaMemcpyDeviceToHost);

        size_t untouched = 0, diff = 0;
        for (size_t i = 0; i < dst_n; ++i) {
            int32_t bits; std::memcpy(&bits, &goo[i], 4);
            if (bits == -1) ++untouched;
            if (std::memcmp(&nat[i], &goo[i], 4) != 0) ++diff;
        }
        check(untouched == 0, (std::string("all written ")+c.name).c_str());
        check(diff == 0, (std::string("bit-exact ")+c.name).c_str());
        if (diff && dump) {
            for (size_t i = 0; i < dst_n; ++i) {
                if (std::memcmp(&nat[i], &goo[i], 4) != 0) {
                    std::fprintf(stderr, "first diff %s idx=%zu native=%a goose=%a\n", c.name, i, nat[i], goo[i]);
                    break;
                }
            }
        }
        if (dump) {
            std::string f = std::string(dump) + "/" + c.name + ".bin";
            if (FILE * fp = fopen(f.c_str(), "wb")) { fwrite(goo.data(), 4, goo.size(), fp); fclose(fp); }
        }
        cudaFree(dsrc); cudaFree(dnat); cudaFree(dgoo); cudaStreamDestroy(st);
    }
    std::printf("FG248D_GPU_FIXTURE checks=%d failures=%d\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
