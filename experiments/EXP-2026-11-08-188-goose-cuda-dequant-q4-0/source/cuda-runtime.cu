#include "common.cuh"
#include "convert.cuh"
#include "host-ffi.h"
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>

// Raw device services for the Goose Q4_0 dequantizer: byte load, half unpack, half
// store. The same three primitives the native body uses.
static __device__ unsigned long long fg188d_paths,fg188d_guards;
static __device__ __forceinline__ uint32_t fg188d_ld_u8(uint64_t a){return *(const uint8_t*)(uintptr_t)a;}
static __device__ __forceinline__ float    fg188d_ld_half(uint64_t a){return __half2float(*(const half*)(uintptr_t)a);}
static __device__ __forceinline__ void     fg188d_st_half(uint64_t a,float v){*(half*)(uintptr_t)a=__float2half(v);}
static __device__ __forceinline__ void     fg188d_fail(int64_t){atomicAdd(&fg188d_guards,1ULL);asm("trap;");}
#define FG188D_BLOCK fg188dd_gg_function_1
#include "device-functions-generated.cuh"

// Same launch shape as native: 32 threads, one q4_0 block per ir.
static __global__ __launch_bounds__(32) void fg188d_kernel(const char * vx,half * yy,int nb32){
 if(blockIdx.x==0&&threadIdx.x==0)atomicOr(&fg188d_paths,1);
 FG188D_BLOCK((uint64_t)(uintptr_t)vx,(uint64_t)(uintptr_t)yy,
  (int64_t)nb32,(int64_t)blockIdx.x,(int64_t)threadIdx.x,0);
}

struct fg188d_packet{cudaStream_t stream{};uint64_t v[8][32]{};bool launched=false;};
static std::atomic<uint64_t>fg188d_host_seen{0};
static fg188d_packet&fg188d_p(int64_t p){GGML_ASSERT(p);return*(fg188d_packet*)(uintptr_t)p;}

extern "C" int64_t fg188h_arg(int64_t p,int64_t k){GGML_UNUSED(k);return p;}
extern "C" int64_t fg188h_plan(int64_t p){return p;}
extern "C" void fg188h_put(int64_t p,int64_t k,int64_t i,uint64_t x){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);fg188d_p(p).v[k][i]=x;}
extern "C" uint64_t fg188h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);return fg188d_p(p).v[k][i];}
extern "C" void fg188h_fail(int64_t c){GGML_ABORT("Goose Q4_0 dequant validation code %lld",(long long)c);}
extern "C" int64_t fg188h_launch(int64_t pp){
 auto&q=fg188d_p(pp);q.launched=true;auto&v=q.v;
 const auto stream=q.stream;
 GGML_ASSERT(stream!=nullptr);
 const dim3 grid((unsigned)v[1][0],1u,1u);
 const dim3 block((unsigned)v[1][1],1u,1u);
 const auto lp=ggml_cuda_kernel_launch_params(grid,block,0,stream);
 ggml_cuda_kernel_launch(fg188d_kernel,lp,
   (const char*)(uintptr_t)v[0][0],(half*)(uintptr_t)v[0][2],(int)v[0][3]);
 return cudaGetLastError();
}
extern "C" uint64_t fg188d_cuda_host_seen(){return fg188d_host_seen.load(std::memory_order_relaxed);}
extern "C" int fg188d_cuda_device_read(uint64_t*out){
 if(!out)return cudaErrorInvalidValue;
 auto e=cudaMemcpyFromSymbol(out,fg188d_paths,sizeof(uint64_t));if(e!=cudaSuccess)return e;
 return cudaMemcpyFromSymbol(out+1,fg188d_guards,sizeof(uint64_t));
}
__attribute__((destructor)) static void fg188d_report(){
 uint64_t x[2]={};int e=fg188d_cuda_device_read(x);
 fprintf(stderr,"FG188D_CUDA_DEQUANT_Q4_0 host=0x%llx,cuda_status=%d,paths=0x%llx,guards=%llu\n",
  (unsigned long long)fg188d_cuda_host_seen(),e,(unsigned long long)x[0],(unsigned long long)x[1]);
}

// The Goose row dequantizer: same signature as the native F16 selector target.
static void fg188d_row(const void * vx, half * y, const int64_t k, cudaStream_t stream){
 if(k<=0||k%32!=0){
  fg188d_host_seen.fetch_or(32ULL,std::memory_order_relaxed);
  return;
 }
 fg188d_packet p{};p.stream=stream;
 p.v[0][0]=(uint64_t)(uintptr_t)vx;
 p.v[0][1]=(uint64_t)((k+255)/256);
 p.v[0][2]=(uint64_t)(uintptr_t)y;
 p.v[0][3]=(uint64_t)(k/32);
 if(fg188h_run((int64_t)(uintptr_t)&p)){
  fg188d_host_seen.fetch_or(p.launched?1ULL:16ULL,std::memory_order_relaxed);
 }else{
  fg188d_host_seen.fetch_or(64ULL,std::memory_order_relaxed);
 }
}

// Interception point: native ggml_get_to_fp16_cuda returns a static per-type pointer.
to_fp16_cuda_t fg188d_native_ggml_get_to_fp16_cuda(ggml_type);
to_fp16_cuda_t ggml_get_to_fp16_cuda(ggml_type type){
 const char*flag=getenv("GGML_GOOSE_CUDA_DEQUANT_Q4_0");
 if(flag&&strcmp(flag,"0")==0)return fg188d_native_ggml_get_to_fp16_cuda(type);
 if(type==GGML_TYPE_Q4_0)return fg188d_row;
 return fg188d_native_ggml_get_to_fp16_cuda(type);
}
