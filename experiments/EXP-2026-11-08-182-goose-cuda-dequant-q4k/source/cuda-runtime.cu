#include "common.cuh"
#include "convert.cuh"
#include "host-ffi.h"
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>

// Raw device services for the Goose Q4_K dequantizer. Only byte loads, the two
// half-to-float conversions and a float store, which are the same primitives the
// native body uses.
static __device__ unsigned long long fg182d_paths,fg182d_guards;
static __device__ __forceinline__ uint32_t fg182d_ld_u8(uint64_t p,uint64_t o){return *(const uint8_t*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ float    fg182d_ld_half(uint64_t p,uint64_t o){return __half2float(*(const half*)((const char*)(uintptr_t)p+o));}
static __device__ __forceinline__ void     fg182d_st_f32(uint64_t p,uint64_t o,float v){*(float*)((char*)(uintptr_t)p+o)=v;}
static __device__ __forceinline__ void     fg182d_fail(int64_t){atomicAdd(&fg182d_guards,1ULL);asm("trap;");}
#define FG182D_Q4_K fg182dd_gg_function_1
#include "device-functions-generated.cuh"

// One block of 256 values per CTA, 32 threads, exactly like the native kernel.
static __global__ __launch_bounds__(32) void fg182d_kernel(const char * vx,float * yy){
 if(blockIdx.x==0&&threadIdx.x==0)atomicOr(&fg182d_paths,1);
 const uint64_t X=(uint64_t)(uintptr_t)vx,Y=(uint64_t)(uintptr_t)yy;
 FG182D_Q4_K(X,Y,(int64_t)blockIdx.x,(int32_t)threadIdx.x,0);
}

struct fg182d_packet{cudaStream_t stream{};uint64_t v[8][32]{};bool launched=false;};
static std::atomic<uint64_t>fg182d_host_seen{0};
static fg182d_packet&fg182d_p(int64_t p){GGML_ASSERT(p);return*(fg182d_packet*)(uintptr_t)p;}

extern "C" int64_t fg182h_arg(int64_t p,int64_t k){GGML_UNUSED(k);return p;}
extern "C" int64_t fg182h_plan(int64_t p){return p;}
extern "C" void fg182h_put(int64_t p,int64_t k,int64_t i,uint64_t x){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);fg182d_p(p).v[k][i]=x;}
extern "C" uint64_t fg182h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);return fg182d_p(p).v[k][i];}
extern "C" void fg182h_fail(int64_t c){GGML_ABORT("Goose Q4_K dequant validation code %lld",(long long)c);}
extern "C" int64_t fg182h_launch(int64_t pp){
 auto&q=fg182d_p(pp);q.launched=true;auto&v=q.v;
 const auto stream=q.stream;
 GGML_ASSERT(stream!=nullptr);
 const dim3 grid((unsigned)v[0][1]);
 const dim3 block(32u,1u,1u);
 const auto lp=ggml_cuda_kernel_launch_params(grid,block,0,stream);
 ggml_cuda_kernel_launch(fg182d_kernel,lp,
   (const char*)(uintptr_t)v[0][0],(float*)(uintptr_t)v[0][2]);
 return cudaGetLastError();
}
extern "C" uint64_t fg182d_cuda_host_seen(){return fg182d_host_seen.load(std::memory_order_relaxed);}
extern "C" int fg182d_cuda_device_read(uint64_t*out){
 if(!out)return cudaErrorInvalidValue;
 auto e=cudaMemcpyFromSymbol(out,fg182d_paths,sizeof(uint64_t));if(e!=cudaSuccess)return e;
 return cudaMemcpyFromSymbol(out+1,fg182d_guards,sizeof(uint64_t));
}
static void fg182d_report(){
 uint64_t x[2]={};int e=fg182d_cuda_device_read(x);
 fprintf(stderr,"FG182D_CUDA_DEQUANT_Q4_K host=0x%llx,cuda_status=%d,paths=0x%llx,guards=%llu\n",
  (unsigned long long)fg182d_cuda_host_seen(),e,(unsigned long long)x[0],(unsigned long long)x[1]);
}

// The Goose row dequantizer: same signature and launch shape as the native one.
// It publishes its inputs through the host module so the validation and the grid
// sizing stay in Goose.
static void fg182d_row(const void * vx, float * y, const int64_t k, cudaStream_t stream){
 static std::once_flag once;std::call_once(once,[]{std::atexit(fg182d_report);});
 // k is the element count; native derives nb = k/QK_K.
 if(k<=0||k%256!=0){
  fg182d_host_seen.fetch_or(32ULL,std::memory_order_relaxed);
  return;
 }
 fg182d_packet p{};p.stream=stream;
 p.v[0][0]=(uint64_t)(uintptr_t)vx;
 p.v[0][1]=(uint64_t)(k/256);
 p.v[0][2]=(uint64_t)(uintptr_t)y;
 if(fg182dh_run((int64_t)(uintptr_t)&p)){
  fg182d_host_seen.fetch_or(p.launched?1ULL:16ULL,std::memory_order_relaxed);
 }else{
  fg182d_host_seen.fetch_or(64ULL,std::memory_order_relaxed);
 }
}

// Interception point: native ggml_get_to_fp32_cuda returns a static per-type
// function pointer, so the only seam is this switch. Q4_K/F32 is serviced by Goose;
// every other type and destination keeps the native selector.
to_fp32_cuda_t fg182d_native_ggml_get_to_fp32_cuda(ggml_type);
to_fp32_cuda_t ggml_get_to_fp32_cuda(ggml_type type){
 const char*flag=getenv("GGML_GOOSE_CUDA_DEQUANT_Q4K");
 if(flag&&strcmp(flag,"0")==0)return fg182d_native_ggml_get_to_fp32_cuda(type);
 if(type==GGML_TYPE_Q4_K)return fg182d_row;
 return fg182d_native_ggml_get_to_fp32_cuda(type);
}
