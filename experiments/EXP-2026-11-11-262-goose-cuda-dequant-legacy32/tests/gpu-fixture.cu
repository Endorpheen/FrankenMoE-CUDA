// Differential fixture for the 32-element legacy dequantization callbacks.
// For every (block, iqs) pair both arms compute a float2; the gate is bitwise
// equality of both components. Failure reporting goes through an explicit
// device buffer passed to the kernels (device symbols are not reliably visible
// to cudaMemcpyFromSymbol in this build).
#include "common.cuh"
#include "dequantize.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <string>

static __device__ __forceinline__ uint32_t fg262d_ld_u8(uint64_t p,uint64_t o){ return *(const uint8_t*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ int32_t  fg262d_ld_i8(uint64_t p,uint64_t o){ return (int32_t)*(const int8_t*)((const char*)(uintptr_t)p+o); }
// The native qh read is memcpy, i.e. unaligned-safe; block strides make the qh
// offset 4-byte unaligned for q5_0, so assemble the word from bytes.
static __device__ __forceinline__ uint32_t fg262d_ld_u32(uint64_t p,uint64_t o){ const unsigned char * b=(const unsigned char*)((const char*)(uintptr_t)p+o); return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24); }
static __device__ __forceinline__ float    fg262d_ld_half(uint64_t p,uint64_t o){ return __half2float(*(const half*)((const char*)(uintptr_t)p+o)); }
static __device__ __forceinline__ void     fg262d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
static __device__ __forceinline__ void     fg262d_fail(int64_t){ asm("trap;"); }
#include "device-functions-generated.cuh"
#include "entries.h"

static __device__ void compare(int * fc, unsigned * info,int kind,int ib,int iqs,float2 a,float2 b){
    uint32_t ax, ay, bx, by;
    memcpy(&ax,&a.x,4); memcpy(&ay,&a.y,4); memcpy(&bx,&b.x,4); memcpy(&by,&b.y,4);
    if (ax!=bx || ay!=by) {
        int old = atomicAdd(fc,1);
        if (old==0) {
            info[0]=kind; info[1]=ib; info[2]=iqs;
            info[3]=ax; info[4]=ay; info[5]=bx; info[6]=by;
        }
    }
}

#define CASE(NAME,KIND,STEP,N) \
__global__ void cmp_##NAME(const void * vx, int * fc, unsigned * info){ \
    int ib=blockIdx.x; int t=threadIdx.x; if(t>=(N)) return; int iqs=t*(STEP); \
    float2 a,b; dequantize_##NAME(vx,ib,iqs,a); \
    FG262D_CB_##NAME((uint64_t)(uintptr_t)vx,(int64_t)ib,(int64_t)iqs,(uint64_t)(uintptr_t)&b,0); \
    compare(fc,info,KIND,ib,iqs,a,b); }

CASE(q1_0,0,2,64)
CASE(q2_0,1,2,32)
CASE(q4_0,2,1,16)
CASE(q4_1,3,1,16)
CASE(q5_0,4,1,16)
CASE(q5_1,5,1,16)
CASE(q8_0,6,2,16)

static int checks;
static int launch_fail;
static int total_fails;
static unsigned first_info[8];

static void run(const char * name, void (*k)(const void*,int*,unsigned*), int N, size_t bytes,
                std::mt19937_64 & rng, int nblocks){
    std::vector<uint8_t> raw(bytes);
    for (auto & v : raw) v = (uint8_t)rng();
    void * d=nullptr; int * fc=nullptr; unsigned * info=nullptr;
    cudaMalloc(&d, bytes); cudaMalloc(&fc,sizeof(int)); cudaMalloc(&info,8*sizeof(unsigned));
    cudaMemcpy(d, raw.data(), bytes, cudaMemcpyHostToDevice);
    cudaMemset(fc,0,sizeof(int)); cudaMemset(info,0,8*sizeof(unsigned));
    k<<<nblocks,N>>>(d,fc,info);
    cudaError_t e = cudaDeviceSynchronize();
    if (e!=cudaSuccess) { printf("LAUNCH FAIL %s: %s\n",name,cudaGetErrorString(e)); ++launch_fail; }
    int f=0;
    if (cudaMemcpy(&f,fc,sizeof(int),cudaMemcpyDeviceToHost)==cudaSuccess && f>0) {
        total_fails += f;
        if (total_fails==f) cudaMemcpy(first_info,info,8*sizeof(unsigned),cudaMemcpyDeviceToHost);
    }
    cudaFree(d); cudaFree(fc); cudaFree(info);
    checks += nblocks*N;
    printf("case %-6s blocks=%d pairs=%d\n",name,nblocks,nblocks*N);
}

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1) { fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(262011);
    const int NB=8;
    run("q1_0",cmp_q1_0,64,18*NB,rng,NB);
    run("q2_0",cmp_q2_0,32,18*NB,rng,NB);
    run("q4_0",cmp_q4_0,16,18*NB,rng,NB);
    run("q4_1",cmp_q4_1,16,20*NB,rng,NB);
    run("q5_0",cmp_q5_0,16,22*NB,rng,NB);
    run("q5_1",cmp_q5_1,16,24*NB,rng,NB);
    run("q8_0",cmp_q8_0,16,34*NB,rng,NB);

    if (total_fails) {
        printf("FIRST FAIL kind=%u block=%u iqs=%u native=(%08x,%08x) goose=(%08x,%08x)\n",
               first_info[0],first_info[1],first_info[2],first_info[3],first_info[4],first_info[5],first_info[6]);
    }
    printf("FG262D_GPU_FIXTURE pairs=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
