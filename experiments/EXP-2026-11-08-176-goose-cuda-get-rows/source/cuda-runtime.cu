#include "common.cuh"
#include "host-ffi.h"
#include "device-types-generated.cuh"
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>

// Raw scalar device services for the Goose GET_ROWS bodies. Loads and stores are
// plain typed accesses of the element or 16-byte vector width the native kernels
// use, so the Goose copy is byte-exact. __umulhi matches the native fastdiv.
// No quantization, cast helper, GGML struct or C++ routine is reachable from an
// enabled path.
static __device__ unsigned long long fg176g_paths,fg176g_guards;
static __device__ __forceinline__ float   fg176g_ld_f32(uint64_t p,uint64_t o){return *(const float   *)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ void    fg176g_st_f32(uint64_t p,uint64_t o,float v){*(float *)((char*)(uintptr_t)p+o)=v;}
static __device__ __forceinline__ int64_t fg176g_ld_i32(uint64_t p,uint64_t o){return (int64_t)*(const int32_t*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ int32_t fg176g_ld_v4(uint64_t p,uint64_t o,int32_t k){return ((const int32_t*)((const char*)(uintptr_t)p+o))[k];}
static __device__ __forceinline__ void    fg176g_st_v4(uint64_t p,uint64_t o,int32_t k,int32_t v){((int32_t*)((char*)(uintptr_t)p+o))[k]=v;}
static __device__ __forceinline__ uint32_t fg176g_mulhi(uint32_t a,uint32_t b){return __umulhi(a,b);}
static __device__ __forceinline__ void    fg176g_pdl_sync(){ggml_cuda_pdl_sync();}
static __device__ __forceinline__ void    fg176g_pdl_lc(){ggml_cuda_pdl_lc();}
static __device__ __forceinline__ void    fg176g_fail(int64_t){atomicAdd(&fg176g_guards,1ULL);asm("trap;");}
#define FG176G_VEC fg176gd_gg_function_1
#define FG176G_SCALAR fg176gd_gg_function_9
#include "device-functions-generated.cuh"

// One GPU thread runs one Goose body invocation. The Goose wrapper keeps the z
// loop and the i00 loop internally; blockIdx/gridDim/blockDim arrive as integers
// so the thread mapping is reconstructed exactly as the native kernel does.
template<int KIND> static __global__ __launch_bounds__(64) void fg176g_kernel(
 const char * a,const char * ix,char * d,
 int64_t n00,int64_t n11,int64_t n12,int32_t es,
 int64_t s1,int64_t s2,int64_t s3,
 int64_t nb01,int64_t nb02,int64_t nb03,
 int64_t s10,int64_t s11,int64_t s12,
 uint32_t mp,uint32_t sh,uint32_t dv){
 if(blockIdx.x==0&&blockIdx.y==0&&blockIdx.z==0&&threadIdx.x==0)atomicOr(&fg176g_paths,1ULL<<KIND);
 const uint64_t A=(uint64_t)(uintptr_t)a,B=(uint64_t)(uintptr_t)ix,D=(uint64_t)(uintptr_t)d;
 const int32_t bx=(int32_t)blockIdx.x,by=(int32_t)blockIdx.y,bz=(int32_t)blockIdx.z;
 const int32_t gx=(int32_t)gridDim.x,gy=(int32_t)gridDim.y,gz=(int32_t)gridDim.z;
 const int32_t tx=(int32_t)threadIdx.x,nt=(int32_t)blockDim.x;
 if constexpr(KIND==0){
  FG176G_VEC(A,B,D,n00,n11,n12,fg176g_FastDiv{mp,sh,dv},
   s1,s2,s3,nb01,nb02,nb03,s10,s11,s12,bx,by,bz,gx,gy,gz,tx,nt,0);
 }else{
  FG176G_SCALAR(A,B,D,n00,n11,n12,fg176g_FastDiv{mp,sh,dv},
   s1,s2,s3,nb01,nb02,nb03,s10,s11,s12,es,bx,by,bz,gy,gz,tx,nt,0);
 }
}

struct fg176g_packet{ggml_backend_cuda_context*ctx;ggml_tensor*dst;uint64_t v[8][32]{};bool launched=false;};
static std::atomic<uint64_t>fg176g_host_paths{0};
static fg176g_packet&fg176g_p(int64_t p){GGML_ASSERT(p);return*(fg176g_packet*)(uintptr_t)p;}
// `t` is a raw ggml_tensor address from fg176h_arg, not the packet.
static const ggml_tensor&fg176g_t(int64_t t){GGML_ASSERT(t);return*(const ggml_tensor*)(uintptr_t)t;}

extern "C" int64_t fg176h_arg(int64_t p,int64_t k){GGML_ASSERT(k==0);return(int64_t)(uintptr_t)fg176g_p(p).dst;}
extern "C" int64_t fg176h_plan(int64_t p){return p;}
extern "C" void fg176h_put(int64_t p,int64_t k,int64_t i,uint64_t x){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);fg176g_p(p).v[k][i]=x;}
extern "C" uint64_t fg176h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);return fg176g_p(p).v[k][i];}
extern "C" int64_t fg176h_ti(int64_t p,int64_t k,int64_t i){
 // k==9 is a packet-level query, not a tensor field: are both device pointers of
 // src0 and dst 16-byte aligned, which the native vector predicate requires.
 if(k==9)return fg176g_p(p).v[6][8]!=0?(int64_t)1:(int64_t)0;
 const auto&t=fg176g_t(p);
 switch(k){
  case 0:GGML_ASSERT(i>=0&&i<GGML_MAX_SRC);return(int64_t)(uintptr_t)t.src[i];
  case 1:return(int64_t)(uintptr_t)t.data;
  case 2:return t.type;
  case 3:GGML_ASSERT(i>=0&&i<4);return t.ne[i];
  case 4:GGML_ASSERT(i>=0&&i<4);GGML_ASSERT(t.nb[i]<=INT64_MAX);return(int64_t)t.nb[i];
  case 5:return t.op;
  default:GGML_ABORT("GET_ROWS tensor field");
 }
}
extern "C" int64_t fg176h_tc(int64_t k){switch(k){case 8:return GGML_TYPE_I32;default:GGML_ABORT("GET_ROWS type query");}}
extern "C" int64_t fg176h_oc(int64_t k){GGML_ASSERT(k==0);return GGML_OP_GET_ROWS;}
extern "C" int64_t fg176h_tsz(int64_t ty){return(int64_t)ggml_type_size((ggml_type)ty);}
extern "C" void fg176h_fail(int64_t c){GGML_ABORT("Goose GET_ROWS validation code %lld",(long long)c);}

// KIND 0: k_get_rows_float_vec. KIND 1: k_get_rows_float, F32->F32.
extern "C" int64_t fg176h_launch(int64_t pp){
 auto&p=fg176g_p(pp);GGML_ASSERT(p.ctx);p.launched=true;auto&v=p.v;
 const auto stream=p.ctx->stream();
 const uint64_t kind=v[6][0];
 const dim3 grid((unsigned)v[7][1],(unsigned)v[7][2],(unsigned)v[7][3]);
 const dim3 block((unsigned)v[7][4]);
 const char*s0=(const char*)(uintptr_t)v[6][1];
 const char*ix=(const char*)(uintptr_t)v[6][2];
 char*dst=(char*)(uintptr_t)v[6][3];
 const auto lp=ggml_cuda_kernel_launch_params(grid,block,0,stream);
 const uint32_t mp=(uint32_t)v[0][0],sh=(uint32_t)v[0][1],dv=(uint32_t)v[0][2];
 auto args=[&](auto k){return ggml_cuda_kernel_launch(k,lp,s0,ix,dst,
   (int64_t)v[7][0],(int64_t)v[7][5],(int64_t)v[7][6],(int32_t)v[7][7],
   (int64_t)v[1][0],(int64_t)v[1][1],(int64_t)v[1][2],
   (int64_t)v[3][0],(int64_t)v[3][1],(int64_t)v[3][2],
   (int64_t)v[2][0],(int64_t)v[2][1],(int64_t)v[2][2],mp,sh,dv);};
 // Host route constants: 0 = scalar element path, 1 = 16-byte vector path.
 if(kind==1){args(fg176g_kernel<0>);return cudaGetLastError();}
 if(kind==0){args(fg176g_kernel<1>);return cudaGetLastError();}
 return cudaErrorInvalidValue;
}

extern "C" uint64_t fg176g_cuda_host_seen(){return fg176g_host_paths.load(std::memory_order_relaxed);}
extern "C" int fg176g_cuda_device_read(uint64_t*out){
 if(!out)return cudaErrorInvalidValue;
 auto e=cudaMemcpyFromSymbol(out,fg176g_paths,sizeof(uint64_t));if(e!=cudaSuccess)return e;
 return cudaMemcpyFromSymbol(out+1,fg176g_guards,sizeof(uint64_t));
}
static void fg176g_report(){
 uint64_t x[2]={};int e=fg176g_cuda_device_read(x);
 fprintf(stderr,"FG176G_CUDA_GET_ROWS host=0x%llx,cuda_status=%d,paths=0x%llx,guards=%llu\n",
  (unsigned long long)fg176g_cuda_host_seen(),e,(unsigned long long)x[0],(unsigned long long)x[1]);
}
void fg176g_native_ggml_cuda_op_get_rows(ggml_backend_cuda_context&,ggml_tensor*);
void ggml_cuda_op_get_rows(ggml_backend_cuda_context&ctx,ggml_tensor*dst){
 const char*flag=getenv("GGML_GOOSE_CUDA_GET_ROWS");
 if(flag&&strcmp(flag,"0")==0){fg176g_native_ggml_cuda_op_get_rows(ctx,dst);return;}
 static std::once_flag once;std::call_once(once,[]{std::atexit(fg176g_report);});
 fg176g_packet p{&ctx,dst};
 p.v[6][1]=(uint64_t)(uintptr_t)(dst->src[0]?dst->src[0]->data:nullptr);
 p.v[6][2]=(uint64_t)(uintptr_t)(dst->src[1]?dst->src[1]->data:nullptr);
 p.v[6][3]=(uint64_t)(uintptr_t)dst->data;
 // Native vector predicate requires both device pointers to be 16-byte aligned.
 p.v[6][8]=(((uintptr_t)p.v[6][1]%16)==0&&((uintptr_t)p.v[6][3]%16)==0)?1:0;
 if(fg176gh_run((int64_t)(uintptr_t)&p)){
  fg176g_host_paths.fetch_or(p.launched?1ULL<<p.v[6][0]:16ULL,std::memory_order_relaxed);
 }else{
  fg176g_host_paths.fetch_or(32ULL,std::memory_order_relaxed);
  fg176g_native_ggml_cuda_op_get_rows(ctx,dst);
 }
}
