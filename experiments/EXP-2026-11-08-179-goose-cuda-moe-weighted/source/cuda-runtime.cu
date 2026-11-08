#include "common.cuh"
#include "host-ffi.h"
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>

// Raw scalar device services for the Goose MoE weighted reduction. Only a float
// load and store, matching the native operator's element accesses exactly.
static __device__ unsigned long long fg179w_paths,fg179w_guards;
static __device__ __forceinline__ float fg179w_ld_f32(uint64_t p,uint64_t o){return *(const float*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ void  fg179w_st_f32(uint64_t p,uint64_t o,float v){*(float*)((char*)(uintptr_t)p+o)=v;}
static __device__ __forceinline__ void  fg179w_fail(int64_t){atomicAdd(&fg179w_guards,1ULL);asm("trap;");}
#define FG179W_REDUCE fg179wd_gg_function_1
#include "device-functions-generated.cuh"

// One GPU thread produces one output element. The Goose body keeps the expert
// loop and the multiply-add order internally.
static __global__ __launch_bounds__(256) void fg179w_kernel(
 const char * e,const char * sc,const char * w,char * d,
 int64_t n_embd,int64_t n_used){
 if(blockIdx.x==0&&blockIdx.y==0&&threadIdx.x==0)atomicOr(&fg179w_paths,1);
 const uint64_t E=(uint64_t)(uintptr_t)e,S=(uint64_t)(uintptr_t)sc;
 const uint64_t W=(uint64_t)(uintptr_t)w,D=(uint64_t)(uintptr_t)d;
 FG179W_REDUCE(E,S,W,D,n_embd,n_used,
  (int32_t)blockIdx.x,(int32_t)blockIdx.y,(int32_t)threadIdx.x,(int32_t)blockDim.x,0);
}

struct fg179w_packet{ggml_backend_cuda_context*ctx;const ggml_tensor*experts;const ggml_tensor*scale;const ggml_tensor*weights;ggml_tensor*dst;uint64_t v[8][32]{};bool launched=false;};
static std::atomic<uint64_t>fg179w_host_seen{0};
static fg179w_packet&fg179w_p(int64_t p){GGML_ASSERT(p);return*(fg179w_packet*)(uintptr_t)p;}

extern "C" int64_t fg179h_arg(int64_t p,int64_t k){
 auto&q=fg179w_p(p);
 switch(k){case 0:return(int64_t)(uintptr_t)q.experts;case 1:return(int64_t)(uintptr_t)q.scale;
  case 2:return(int64_t)(uintptr_t)q.weights;default:return(int64_t)(uintptr_t)q.dst;}
}
extern "C" int64_t fg179h_plan(int64_t p){return p;}
extern "C" void fg179h_put(int64_t p,int64_t k,int64_t i,uint64_t x){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);fg179w_p(p).v[k][i]=x;}
extern "C" uint64_t fg179h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);return fg179w_p(p).v[k][i];}
extern "C" void fg179h_fail(int64_t c){GGML_ABORT("Goose MoE weighted validation code %lld",(long long)c);}
extern "C" int64_t fg179h_launch(int64_t pp){
 auto&q=fg179w_p(pp);GGML_ASSERT(q.ctx);q.launched=true;auto&v=q.v;
 const auto stream=q.ctx->stream();
 const dim3 grid((unsigned)v[1][0],(unsigned)v[1][1],1u);
 const dim3 block((unsigned)v[1][2]);
 const auto lp=ggml_cuda_kernel_launch_params(grid,block,0,stream);
 ggml_cuda_kernel_launch(fg179w_kernel,lp,
   (const char*)q.experts->data,(const char*)(q.scale?q.scale->data:nullptr),
   (const char*)q.weights->data,(char*)q.dst->data,
   (int64_t)v[2][0],(int64_t)v[2][1]);
 return cudaGetLastError();
}
extern "C" uint64_t fg179w_cuda_host_seen(){return fg179w_host_seen.load(std::memory_order_relaxed);}
extern "C" int fg179w_cuda_device_read(uint64_t*out){
 if(!out)return cudaErrorInvalidValue;
 auto e=cudaMemcpyFromSymbol(out,fg179w_paths,sizeof(uint64_t));if(e!=cudaSuccess)return e;
 return cudaMemcpyFromSymbol(out+1,fg179w_guards,sizeof(uint64_t));
}
static void fg179w_report(){
 uint64_t x[2]={};int e=fg179w_cuda_device_read(x);
 fprintf(stderr,"FG179W_CUDA_MOE_WEIGHTED host=0x%llx,cuda_status=%d,paths=0x%llx,guards=%llu\n",
  (unsigned long long)fg179w_cuda_host_seen(),e,(unsigned long long)x[0],(unsigned long long)x[1]);
}
void fg179w_native_ggml_cuda_op_moe_weighted_reduction(ggml_backend_cuda_context&,
 const ggml_tensor*,const ggml_tensor*,const ggml_tensor*,ggml_tensor*);
void ggml_cuda_op_moe_weighted_reduction(ggml_backend_cuda_context &ctx,
 const ggml_tensor*experts,const ggml_tensor*expert_scale,const ggml_tensor*weights,ggml_tensor*dst){
 const char*flag=getenv("GGML_GOOSE_CUDA_MOE_WEIGHTED");
 if(flag&&strcmp(flag,"0")==0){
  fg179w_native_ggml_cuda_op_moe_weighted_reduction(ctx,experts,expert_scale,weights,dst);
  return;
 }
 // Native asserts all operands float; refuse rather than launch a mismatched body.
 auto f32=[](const ggml_tensor*t){return t&&t->type==GGML_TYPE_F32;};
 if(!f32(experts)||!f32(weights)||!f32(dst)||(expert_scale&&!f32(expert_scale))){
  fg179w_host_seen.fetch_or(32ULL,std::memory_order_relaxed);
  fg179w_native_ggml_cuda_op_moe_weighted_reduction(ctx,experts,expert_scale,weights,dst);
  return;
 }
 static std::once_flag once;std::call_once(once,[]{std::atexit(fg179w_report);});
 fg179w_packet p{&ctx,experts,expert_scale,weights,dst};
 auto&v=p.v;
 // The host reads the four pointers from slot 0 and the extents/flags from slot 1.
 v[0][0]=(uint64_t)(uintptr_t)experts;
 v[0][1]=(uint64_t)(uintptr_t)expert_scale;
 v[0][2]=(uint64_t)(uintptr_t)weights;
 v[0][3]=(uint64_t)(uintptr_t)dst;
 v[1][0]=(uint64_t)experts->ne[0];v[1][1]=(uint64_t)experts->ne[1];
 v[1][2]=(uint64_t)experts->ne[2];v[1][3]=(uint64_t)experts->ne[3];
 v[1][4]=ggml_is_contiguous(experts)?1:0;
 v[1][5]=ggml_is_contiguous(weights)?1:0;
 v[1][6]=ggml_is_contiguous(dst)?1:0;
 v[1][7]=(expert_scale?(ggml_is_contiguous(expert_scale)?1:0):1);
 if(fg179wh_run((int64_t)(uintptr_t)&p)){
  fg179w_host_seen.fetch_or(1ULL<<(p.launched?0:4),std::memory_order_relaxed);
 }else{
  fg179w_host_seen.fetch_or(64ULL,std::memory_order_relaxed);
  fg179w_native_ggml_cuda_op_moe_weighted_reduction(ctx,experts,expert_scale,weights,dst);
 }
}
