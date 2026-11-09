#include "common.cuh"
#include "mmq.cuh"
#include "host-ffi.h"
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>

// Raw scalar device services for the Goose MMQ Q8_1 quantizer. The float4 load is
// four plain float reads at consecutive offsets, the char4 store is one 32-bit
// write, and the half2 store uses the same round-to-nearest conversion as native.
static __device__ unsigned long long fg180q_paths,fg180q_guards;
// One float4 load per call site; the four accessors read the same aligned address,
// so the compiler folds them into a single LDG.128 exactly like the native kernel.
static __device__ __forceinline__ float4 fg180q_f4(uint64_t p,uint64_t o){return *(const float4*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ float fg180q_ld_f4x(uint64_t p,uint64_t o){return fg180q_f4(p,o).x;}
static __device__ __forceinline__ float fg180q_ld_f4y(uint64_t p,uint64_t o){return fg180q_f4(p,o).y;}
static __device__ __forceinline__ float fg180q_ld_f4z(uint64_t p,uint64_t o){return fg180q_f4(p,o).z;}
static __device__ __forceinline__ float fg180q_ld_f4w(uint64_t p,uint64_t o){return fg180q_f4(p,o).w;}
static __device__ __forceinline__ int64_t fg180q_ld_i32(uint64_t p,uint64_t o){return (int64_t)*(const int32_t*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ void    fg180q_st_i32(uint64_t p,uint64_t o,int32_t v){*(int32_t*)((char*)(uintptr_t)p+o)=v;}
static __device__ __forceinline__ void    fg180q_st_f32(uint64_t p,uint64_t o,float v){*(float*)((char*)(uintptr_t)p+o)=v;}
static __device__ __forceinline__ void    fg180q_st_half2(uint64_t p,uint64_t o,float a,float b){*(half2*)((char*)(uintptr_t)p+o)=__floats2half2_rn(a,b);}
static __device__ __forceinline__ float   fg180q_shfl_xor(float v,uint32_t m,int32_t o){return __shfl_xor_sync(m,v,o);}
static __device__ __forceinline__ float   fg180q_round(float v){return roundf(v);}
static __device__ __forceinline__ float   fg180q_fabs(float v){return fabsf(v);}
static __device__ __forceinline__ int32_t fg180q_i32(float v){return (int32_t)v;}
static __device__ __forceinline__ void    fg180q_pdl_sync(){ggml_cuda_pdl_sync();}
static __device__ __forceinline__ void    fg180q_fail(int64_t){atomicAdd(&fg180q_guards,1ULL);asm("trap;");}
#define FG180Q_DS4_DIRECT fg180qd_gg_function_1
#define FG180Q_DS4_SCATTER fg180qd_gg_function_22
#include "device-functions-generated.cuh"

// One GPU thread handles four consecutive values. The Goose body keeps the
// reductions, the layout branches and both the direct and the scatter block index.
template<int SCATTER> static __global__ __launch_bounds__(128) void fg180q_kernel(
 const char * x,const char * ids,char * y,
 int64_t ne00,int64_t s01,int64_t s02,int64_t s03,
 int64_t ne0,int64_t ne1,int64_t ne2,
 int32_t n_used){
 if(blockIdx.x==0&&blockIdx.y==0&&blockIdx.z==0&&threadIdx.x==0)atomicOr(&fg180q_paths,1ULL<<SCATTER);
 const uint64_t X=(uint64_t)(uintptr_t)x,I=(uint64_t)(uintptr_t)ids,Y=(uint64_t)(uintptr_t)y;
 if constexpr(SCATTER){FG180Q_DS4_SCATTER(X,I,Y,ne00,s01,s02,s03,ne0,ne1,ne2,n_used,
   (int32_t)blockIdx.x,(int32_t)blockIdx.y,(int32_t)blockIdx.z,
   (int32_t)threadIdx.x,(int32_t)blockDim.x,(int32_t)gridDim.x,(int32_t)gridDim.y,0);}
 else{FG180Q_DS4_DIRECT(X,I,Y,ne00,s01,s02,s03,ne0,ne1,ne2,n_used,
   (int32_t)blockIdx.x,(int32_t)blockIdx.y,(int32_t)blockIdx.z,
   (int32_t)threadIdx.x,(int32_t)blockDim.x,(int32_t)gridDim.x,(int32_t)gridDim.y,0);}
}

struct fg180q_packet{ggml_backend_cuda_context*ctx;cudaStream_t stream{};uint64_t v[8][32]{};bool launched=false;};
static std::atomic<uint64_t>fg180q_host_seen{0};
static fg180q_packet&fg180q_p(int64_t p){GGML_ASSERT(p);return*(fg180q_packet*)(uintptr_t)p;}

extern "C" int64_t fg180h_arg(int64_t p,int64_t k){GGML_UNUSED(k);return p;}
extern "C" int64_t fg180h_plan(int64_t p){return p;}
extern "C" void fg180h_put(int64_t p,int64_t k,int64_t i,uint64_t x){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);fg180q_p(p).v[k][i]=x;}
extern "C" uint64_t fg180h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);return fg180q_p(p).v[k][i];}
extern "C" void fg180h_fail(int64_t c){GGML_ABORT("Goose MMQ Q8_1 validation code %lld",(long long)c);}
extern "C" int64_t fg180h_launch(int64_t pp){
 auto&q=fg180q_p(pp);q.launched=true;auto&v=q.v;
 const auto stream=q.stream;
 GGML_ASSERT(stream!=nullptr);
 const dim3 grid((unsigned)v[1][0],(unsigned)v[1][1],(unsigned)v[1][2]);
 const dim3 block((unsigned)v[1][3]);
 const auto lp=ggml_cuda_kernel_launch_params(grid,block,0,stream);
 auto args=[&](auto k){return ggml_cuda_kernel_launch(k,lp,
   (const char*)(uintptr_t)v[0][0],(const char*)(uintptr_t)v[0][1],(char*)(uintptr_t)v[0][2],
   (int64_t)v[5][0],(int64_t)v[5][1],(int64_t)v[4][0],(int64_t)v[5][2],
   (int64_t)v[5][3],(int64_t)v[5][4],(int64_t)v[5][5],(int32_t)v[3][0]);};
 if((int32_t)v[2][0]){args(fg180q_kernel<1>);}else{args(fg180q_kernel<0>);}
 return cudaGetLastError();
}
extern "C" uint64_t fg180q_cuda_host_seen(){return fg180q_host_seen.load(std::memory_order_relaxed);}
extern "C" int fg180q_cuda_device_read(uint64_t*out){
 if(!out)return cudaErrorInvalidValue;
 auto e=cudaMemcpyFromSymbol(out,fg180q_paths,sizeof(uint64_t));if(e!=cudaSuccess)return e;
 return cudaMemcpyFromSymbol(out+1,fg180q_guards,sizeof(uint64_t));
}
static void fg180q_report(){
 uint64_t x[2]={};int e=fg180q_cuda_device_read(x);
 fprintf(stderr,"FG180Q_CUDA_MMQ_Q8_1 host=0x%llx,cuda_status=%d,paths=0x%llx,guards=%llu\n",
  (unsigned long long)fg180q_cuda_host_seen(),e,(unsigned long long)x[0],(unsigned long long)x[1]);
}
void fg180q_native_quantize_mmq_q8_1_cuda(const float*,const int32_t*,void*,ggml_type,
 int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,cudaStream_t);
void fg180q_native_quantize_scatter_mmq_q8_1_cuda(const float*,const int32_t*,void*,ggml_type,
 int64_t,int64_t,int64_t,int64_t,int64_t,int32_t,cudaStream_t);

// EXP180 was REJECTED: the kernel is 12-14% slower per launch (2065 vs 1819 ns) and
// the paired Apex showed a regression. A rejected candidate must not be on by default,
// so this is opt-IN: the Goose path runs only when the flag is explicitly "1". Any
// other value, including unset, keeps native.
static bool fg180q_goose(const char*flag){
 return flag&&strcmp(flag,"1")==0;
}
void quantize_mmq_q8_1_cuda(const float*x,const int32_t*ids,void*vy,ggml_type type_src0,
 int64_t ne00,int64_t s01,int64_t s02,int64_t s03,
 int64_t ne0,int64_t ne1,int64_t ne2,int64_t ne3,cudaStream_t stream){
 const char*flag=getenv("GGML_GOOSE_CUDA_MMQ_Q8_1");
 if(!fg180q_goose(flag)){
  fg180q_native_quantize_mmq_q8_1_cuda(x,ids,vy,type_src0,ne00,s01,s02,s03,ne0,ne1,ne2,ne3,stream);
  return;
 }
 static std::once_flag once;std::call_once(once,[]{std::atexit(fg180q_report);});
 fg180q_packet p{};p.stream=stream;
 p.v[0][0]=(uint64_t)(uintptr_t)x;
 p.v[0][1]=(uint64_t)(uintptr_t)ids;
 p.v[0][2]=(uint64_t)(uintptr_t)vy;
 p.v[0][3]=(uint64_t)mmq_get_q8_1_ds_layout(type_src0);
 p.v[1][0]=(uint64_t)ne00;p.v[1][1]=(uint64_t)s01;p.v[1][2]=(uint64_t)s02;p.v[1][3]=(uint64_t)s03;
 p.v[1][4]=(uint64_t)ne0;p.v[1][5]=(uint64_t)ne1;p.v[1][6]=(uint64_t)ne2;p.v[1][7]=(uint64_t)ne3;
 p.v[6][0]=(uint64_t)ne00;p.v[6][1]=(uint64_t)s01;p.v[6][2]=(uint64_t)s02;p.v[6][3]=(uint64_t)s03;
 p.v[6][4]=(uint64_t)ne0;p.v[6][5]=(uint64_t)ne1;p.v[6][6]=(uint64_t)ne2;
 p.v[2][0]=0;
 if(fg180qh_run((int64_t)(uintptr_t)&p)){
  fg180q_host_seen.fetch_or(1ULL,std::memory_order_relaxed);
 }else{
  fg180q_host_seen.fetch_or(32ULL,std::memory_order_relaxed);
  fg180q_native_quantize_mmq_q8_1_cuda(x,ids,vy,type_src0,ne00,s01,s02,s03,ne0,ne1,ne2,ne3,stream);
 }
}
void quantize_scatter_mmq_q8_1_cuda(const float*x,const int32_t*ids_inv,void*vy,ggml_type type_src0,
 int64_t ne00,int64_t stride_token,int64_t ne0,
 int64_t n_tokens,int64_t nrows_dst,const int n_expert_used,cudaStream_t stream){
 const char*flag=getenv("GGML_GOOSE_CUDA_MMQ_Q8_1");
 if(!fg180q_goose(flag)){
  fg180q_native_quantize_scatter_mmq_q8_1_cuda(x,ids_inv,vy,type_src0,ne00,stride_token,ne0,
    n_tokens,nrows_dst,n_expert_used,stream);
  return;
 }
 static std::once_flag once;std::call_once(once,[]{std::atexit(fg180q_report);});
 fg180q_packet p{};p.stream=stream;
 p.v[0][0]=(uint64_t)(uintptr_t)x;
 p.v[0][1]=(uint64_t)(uintptr_t)ids_inv;
 p.v[0][2]=(uint64_t)(uintptr_t)vy;
 p.v[0][3]=(uint64_t)mmq_get_q8_1_ds_layout(type_src0);
 // The scatter entry has no ne1..ne3; it passes nrows_dst as ne1 and ne2 = 1, with
 // s01 = 0, s02 = stride_token, s03 = 0, matching the native call.
 p.v[1][0]=(uint64_t)ne00;p.v[1][1]=0;p.v[1][2]=(uint64_t)stride_token;p.v[1][3]=0;
 p.v[1][4]=(uint64_t)ne0;p.v[1][5]=(uint64_t)nrows_dst;p.v[1][6]=1;p.v[1][7]=1;
 p.v[6][0]=(uint64_t)ne00;p.v[6][1]=0;p.v[6][2]=(uint64_t)stride_token;p.v[6][3]=0;
 p.v[6][4]=(uint64_t)ne0;p.v[6][5]=(uint64_t)nrows_dst;p.v[6][6]=1;
 p.v[2][0]=1;
 p.v[2][1]=(uint64_t)n_tokens;p.v[2][2]=(uint64_t)nrows_dst;p.v[2][3]=(uint64_t)n_expert_used;
 if(fg180qh_run((int64_t)(uintptr_t)&p)){
  fg180q_host_seen.fetch_or(2ULL,std::memory_order_relaxed);
 }else{
  fg180q_host_seen.fetch_or(64ULL,std::memory_order_relaxed);
  fg180q_native_quantize_scatter_mmq_q8_1_cuda(x,ids_inv,vy,type_src0,ne00,stride_token,ne0,
    n_tokens,nrows_dst,n_expert_used,stream);
 }
}
