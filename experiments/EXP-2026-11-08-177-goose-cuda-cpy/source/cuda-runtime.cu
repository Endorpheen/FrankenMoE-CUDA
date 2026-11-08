#include "common.cuh"
#include "cpy.cuh"
#include "host-ffi.h"
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>

// Raw scalar device services for the Goose CPY bodies. Loads and stores are plain
// typed accesses of the element width the native scalar kernel uses, so the copy
// is byte-exact. No cast helper, quantizer, GGML struct or C++ routine is
// reachable from an enabled path.
static __device__ unsigned long long fg177c_paths,fg177c_guards;
static __device__ __forceinline__ float fg177c_ld_f32(uint64_t p,uint64_t o){return *(const float*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ void  fg177c_st_f32(uint64_t p,uint64_t o,float v){*(float*)((char*)(uintptr_t)p+o)=v;}
static __device__ __forceinline__ void  fg177c_pdl_sync(){ggml_cuda_pdl_sync();}
static __device__ __forceinline__ void  fg177c_pdl_lc(){ggml_cuda_pdl_lc();}
static __device__ __forceinline__ void  fg177c_fail(int64_t){atomicAdd(&fg177c_guards,1ULL);asm("trap;");}
#define FG177C_SCALAR fg177cd_gg_function_1
#define FG177C_CONTIGUOUS fg177cd_gg_function_9
#include "device-functions-generated.cuh"

// One GPU thread runs one Goose body invocation; the Goose wrapper keeps the
// index decode and every loop internally.
template<int KIND> static __global__ __launch_bounds__(64) void fg177c_kernel(
 const char * a,char * d,int64_t ne,
 int64_t n00,int64_t n01,int64_t n02,
 int64_t b00,int64_t b01,int64_t b02,int64_t b03,
 int64_t m00,int64_t m01,int64_t m02,
 int64_t c00,int64_t c01,int64_t c02,int64_t c03){
 if(blockIdx.x==0&&threadIdx.x==0)atomicOr(&fg177c_paths,1ULL<<KIND);
 const uint64_t A=(uint64_t)(uintptr_t)a,D=(uint64_t)(uintptr_t)d;
 const int32_t bx=(int32_t)blockIdx.x,tx=(int32_t)threadIdx.x,nt=(int32_t)blockDim.x;
 if constexpr(KIND==0){
  FG177C_SCALAR(A,D,ne,n00,n01,n02,b00,b01,b02,b03,m00,m01,m02,c00,c01,c02,c03,bx,tx,nt,0);
 }else{
  FG177C_CONTIGUOUS(A,D,ne,bx,tx,nt,0);
 }
}

struct fg177c_packet{ggml_backend_cuda_context*ctx;const ggml_tensor*src;ggml_tensor*dst;uint64_t v[8][32]{};bool launched=false;};
static std::atomic<uint64_t>fg177c_host_paths{0};
static fg177c_packet&fg177c_p(int64_t p){GGML_ASSERT(p);return*(fg177c_packet*)(uintptr_t)p;}
static const ggml_tensor&fg177c_t(int64_t t){GGML_ASSERT(t);return*(const ggml_tensor*)(uintptr_t)t;}

extern "C" int64_t fg177h_arg(int64_t p,int64_t k){
 auto&q=fg177c_p(p);
 return(int64_t)(uintptr_t)(k==0?q.dst:q.src);
}
extern "C" int64_t fg177h_plan(int64_t p){return p;}
extern "C" void fg177h_put(int64_t p,int64_t k,int64_t i,uint64_t x){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);fg177c_p(p).v[k][i]=x;}
extern "C" uint64_t fg177h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);return fg177c_p(p).v[k][i];}
extern "C" int64_t fg177h_ti(int64_t p,int64_t k,int64_t i){
 // k==9 is a packet query: byte size of the source payload for the memcpy route.
 if(k==9)return(int64_t)fg177c_p(p).v[0][1];
 const auto&t=fg177c_t(p);
 switch(k){
  case 0:GGML_ASSERT(i>=0&&i<GGML_MAX_SRC);return(int64_t)(uintptr_t)t.src[i];
  case 2:return t.type;
  case 3:GGML_ASSERT(i>=0&&i<4);return t.ne[i];
  case 4:GGML_ASSERT(i>=0&&i<4);GGML_ASSERT(t.nb[i]<=INT64_MAX);return(int64_t)t.nb[i];
  case 5:return(int64_t)ggml_element_size(&t);
  case 6:return t.op;
  case 7:return ggml_nelements(&t);
  case 8:return ggml_is_contiguous(&t)?1:0;
  case 10:GGML_ASSERT(ggml_nbytes(&t)<=INT64_MAX);return(int64_t)ggml_nbytes(&t);
  default:GGML_ABORT("CPY tensor field");
 }
}
extern "C" int64_t fg177h_tc(int64_t k){GGML_ABORT("CPY type query %lld",(long long)k);}
extern "C" int64_t fg177h_oc(int64_t k){GGML_ABORT("CPY op query");}
extern "C" int64_t fg177h_tsz(int64_t ty){return(int64_t)ggml_type_size((ggml_type)ty);}
extern "C" void fg177h_fail(int64_t c){GGML_ABORT("Goose CPY validation code %lld",(long long)c);}

// Launch adapter. Route 0: same-type contiguous device memcpy. Route 1: the
// 2D memcpy the native serialiser chose. Route 2: the Goose scalar element walk.
// Route 3: the Goose contiguous element walk. Route 4 is reserved for the
// transposed tile kernel, which this stage leaves on the native path.
extern "C" int64_t fg177h_launch(int64_t pp){
 auto&q=fg177c_p(pp);GGML_ASSERT(q.ctx);q.launched=true;auto&v=q.v;
 const auto stream=q.ctx->stream();
 const uint64_t route=v[0][0];
 if(route==0){
  auto e=cudaMemcpyAsync(q.dst->data,q.src->data,(size_t)v[0][1],cudaMemcpyDeviceToDevice,stream);
  return e==cudaSuccess?cudaGetLastError():(int64_t)e;
 }
 if(route==1){
  auto e=cudaMemcpy2DAsync(q.dst->data,(size_t)v[5][3],q.src->data,(size_t)v[5][2],
      (size_t)v[5][0],(size_t)v[5][1],cudaMemcpyDeviceToDevice,stream);
  return e==cudaSuccess?cudaGetLastError():(int64_t)e;
 }
 const dim3 grid((unsigned)v[0][1]);
 const dim3 block((unsigned)v[0][2]);
 const auto lp=ggml_cuda_kernel_launch_params(grid,block,0,stream);
 const char*s0=(const char*)q.src->data;
 char*d=(char*)q.dst->data;
 auto args=[&](auto k){return ggml_cuda_kernel_launch(k,lp,s0,d,(int64_t)v[1][0],
   (int64_t)v[1][1],(int64_t)v[1][2],(int64_t)v[1][3],
   (int64_t)v[1][4],(int64_t)v[1][5],(int64_t)v[1][6],(int64_t)v[1][7],
   (int64_t)v[2][0],(int64_t)v[2][1],(int64_t)v[2][2],
   (int64_t)v[2][3],(int64_t)v[2][4],(int64_t)v[2][5],(int64_t)v[2][6]);};
 if(route==2){args(fg177c_kernel<0>);return cudaGetLastError();}
 if(route==3){args(fg177c_kernel<1>);return cudaGetLastError();}
 return cudaErrorInvalidValue;
}

extern "C" uint64_t fg177c_cuda_host_seen(){return fg177c_host_paths.load(std::memory_order_relaxed);}
extern "C" int fg177c_cuda_device_read(uint64_t*out){
 if(!out)return cudaErrorInvalidValue;
 auto e=cudaMemcpyFromSymbol(out,fg177c_paths,sizeof(uint64_t));if(e!=cudaSuccess)return e;
 return cudaMemcpyFromSymbol(out+1,fg177c_guards,sizeof(uint64_t));
}
static void fg177c_report(){
 uint64_t x[2]={};int e=fg177c_cuda_device_read(x);
 fprintf(stderr,"FG177C_CUDA_CPY host=0x%llx,cuda_status=%d,paths=0x%llx,guards=%llu\n",
  (unsigned long long)fg177c_cuda_host_seen(),e,(unsigned long long)x[0],(unsigned long long)x[1]);
}
// DUP, CPY and CONT all funnel into ggml_cuda_dup / ggml_cuda_cpy. Both native
// symbols are private-renamed, so every route reaches this switch.
void fg177c_native_ggml_cuda_cpy(ggml_backend_cuda_context&,const ggml_tensor*,ggml_tensor*);
void fg177c_native_ggml_cuda_dup(ggml_backend_cuda_context&,ggml_tensor*);
void ggml_cuda_cpy(ggml_backend_cuda_context&ctx,const ggml_tensor*src0,ggml_tensor*src1){
 const char*flag=getenv("GGML_GOOSE_CUDA_CPY");
 if(flag&&strcmp(flag,"0")==0){fg177c_native_ggml_cuda_cpy(ctx,src0,src1);return;}
 static std::once_flag once;std::call_once(once,[]{std::atexit(fg177c_report);});
 fg177c_packet p{&ctx,src0,src1};
 p.v[0][1]=(uint64_t)ggml_nbytes(src0);
 if(fg177ch_run((int64_t)(uintptr_t)&p)){
  fg177c_host_paths.fetch_or(p.launched?1ULL<<p.v[0][0]:16ULL,std::memory_order_relaxed);
 }else{
  fg177c_host_paths.fetch_or(32ULL,std::memory_order_relaxed);
  fg177c_native_ggml_cuda_cpy(ctx,src0,src1);
 }
}
void ggml_cuda_dup(ggml_backend_cuda_context&ctx,ggml_tensor*dst){
 const char*flag=getenv("GGML_GOOSE_CUDA_CPY");
 if(flag&&strcmp(flag,"0")==0){fg177c_native_ggml_cuda_dup(ctx,dst);return;}
 ggml_cuda_cpy(ctx,dst->src[0],dst);
}
