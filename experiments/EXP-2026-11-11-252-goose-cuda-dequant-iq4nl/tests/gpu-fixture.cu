// GPU differential fixture for the Goose IQ4_NL dequantizer.
//
// The native oracle is the real `dequantize_iq4_nl` template plus the exact
// launch wrapper from the read-only source tree. Both arms run on the GPU on the
// same random block bytes; the gate is bit-exact equality of the whole F32
// destination, including the nb32 tail guard.
#include "common.cuh"
#include "dequantize.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <string>

static __device__ __forceinline__ uint32_t fg252d_ld_u8(uint64_t p,uint64_t o){ return *(const uint8_t*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ float    fg252d_ld_half(uint64_t p,uint64_t o){ return __half2float(*(const half*)((const char*)(uintptr_t)p+o)); }
static __device__ __forceinline__ void     fg252d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
static __device__ __forceinline__ void     fg252d_fail(int64_t){ asm("trap;"); }
#define FG252D_IQ4NL fg252dd_gg_function_1
#include "device-functions-generated.cuh"

static __global__ void native_kernel(const void * vx, float * yy, int nb32){
    const int64_t i = blockIdx.x;
    const int64_t ib = 8*i + threadIdx.x%8;
    if (ib >= nb32) { return; }
    dequantize_iq4_nl<float>(vx, i, yy + i*QK_K, threadIdx.x);
}
static __global__ void goose_kernel(const void * vx, float * yy, int nb32){
    FG252D_IQ4NL((uint64_t)(uintptr_t)vx,(uint64_t)(uintptr_t)yy,
                 (int64_t)blockIdx.x,(int64_t)nb32,(int64_t)threadIdx.x,0);
}

static int g_checks, g_fail;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fail; std::fprintf(stderr, "FAIL %s\n", what); }
}

// block_iq4_nl is 18 bytes: half d at 0, uint8 qs[16] at 2. QK4_NL = 32.
static const size_t IQ4NL_BYTES = 18;
struct Case { int64_t nblocks32; const char * name; };   // in 32-element blocks
static const Case CASES[] = {
    {   8, "one-cta" },
    {  32, "four-cta" },
    { 512, "sixty-four-cta" },
    {2048, "two-fifty-six-cta" },
    {   7, "tail-seven-blocks" },
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    int ndev = 0;
    if (cudaGetDeviceCount(&ndev) != cudaSuccess || ndev < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937 rng(252011);
    const char * dump = getenv("FG252D_DUMP");

    for (const Case & c : CASES) {
        const int64_t nb32 = c.nblocks32;
        const int64_t nb = (nb32 + 7) / 8;
        const size_t src_bytes = (size_t)nb32 * IQ4NL_BYTES;
        const size_t dst_n = (size_t)nb32 * 32;
        std::vector<uint8_t> raw(src_bytes);
        for (auto & v : raw) v = (uint8_t)rng();
        void * dsrc = nullptr; float * dnat = nullptr; float * dgoo = nullptr;
        cudaMalloc(&dsrc, src_bytes);
        cudaMalloc(&dnat, dst_n*sizeof(float));
        cudaMalloc(&dgoo, dst_n*sizeof(float));
        cudaMemcpy(dsrc, raw.data(), src_bytes, cudaMemcpyHostToDevice);
        cudaMemset(dnat, 0xFF, dst_n*sizeof(float));
        cudaMemset(dgoo, 0xFF, dst_n*sizeof(float));

        cudaStream_t st; cudaStreamCreate(&st);
        native_kernel<<<(unsigned)nb,32,0,st>>>(dsrc,dnat,(int)nb32);
        goose_kernel<<<(unsigned)nb,32,0,st>>>(dsrc,dgoo,(int)nb32);
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
    std::printf("FG252D_GPU_FIXTURE checks=%d failures=%d\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
