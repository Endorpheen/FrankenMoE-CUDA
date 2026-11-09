#include "common.cuh"
#include "fwht.cuh"
#include "convert.cuh"
#include "host-ffi.h"
#include "device-types-generated.cuh"
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>

// Raw device services for the Goose FWHT. Float loads and stores plus the shuffles
// the butterfly uses; nothing else is reachable.
static __device__ unsigned long long fg183f_paths,fg183f_guards;
static __device__ __forceinline__ float fg183f_ld_f32(uint64_t p,uint64_t o){return *(const float*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ void  fg183f_st_f32(uint64_t p,uint64_t o,float v){*(float*)((char*)(uintptr_t)p+o)=v;}
static __device__ __forceinline__ float fg183f_shfl_xor(float v,uint32_t m,int32_t o,int32_t w){return __shfl_xor_sync(m,v,o,w);}
static __device__ __forceinline__ void  fg183f_fail(int64_t){atomicAdd(&fg183f_guards,1ULL);asm("trap;");}
#define FG183F_ROW fg183fd_gg_function_1
#include "device-functions-generated.cuh"

// One warp per row, 4 rows per block, exactly like the native launch.
static __global__ __launch_bounds__(128) void fg183f_kernel(
 const char * src,float * dst,int32_t n,int64_t rows,float scale){
 if(blockIdx.x==0&&threadIdx.y==0&&threadIdx.x==0)atomicOr(&fg183f_paths,1);
 const uint64_t S=(uint64_t)(uintptr_t)src,D=(uint64_t)(uintptr_t)dst;
 FG183F_ROW(S,D,n,rows,scale,
  (int32_t)blockIdx.x,(int32_t)blockDim.y,(int32_t)threadIdx.y,(int32_t)threadIdx.x,(int32_t)blockDim.x,0);
}

struct fg183f_packet{cudaStream_t stream{};uint64_t v[8][32]{};bool launched=false;};
static std::atomic<uint64_t>fg183f_host_seen{0};
static fg183f_packet&fg183f_p(int64_t p){GGML_ASSERT(p);return*(fg183f_packet*)(uintptr_t)p;}

extern "C" int64_t fg183h_arg(int64_t p,int64_t k){GGML_UNUSED(k);return p;}
extern "C" int64_t fg183h_plan(int64_t p){return p;}
extern "C" void fg183h_put(int64_t p,int64_t k,int64_t i,uint64_t x){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);fg183f_p(p).v[k][i]=x;}
extern "C" uint64_t fg183h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);return fg183f_p(p).v[k][i];}
extern "C" void fg183h_fail(int64_t c){GGML_ABORT("Goose FWHT validation code %lld",(long long)c);}
extern "C" int64_t fg183h_launch(int64_t pp){
 auto&q=fg183f_p(pp);q.launched=true;auto&v=q.v;
 const auto stream=q.stream;
 GGML_ASSERT(stream!=nullptr);
 const dim3 grid((unsigned)v[1][0],1u,1u);
 const dim3 block((unsigned)v[1][1],(unsigned)v[1][2],1u);
 const auto lp=ggml_cuda_kernel_launch_params(grid,block,0,stream);
 // scale = 1/sqrt(n), computed here because it is a launch parameter, not an
 // algorithmic choice: the native host derives it the same way.
 const float scale=1.0f/sqrtf((float)v[2][0]);
 ggml_cuda_kernel_launch(fg183f_kernel,lp,
   (const char*)(uintptr_t)v[0][0],(float*)(uintptr_t)v[0][1],
   (int32_t)v[2][0],(int64_t)v[2][1],scale);
 return cudaGetLastError();
}
extern "C" uint64_t fg183f_cuda_host_seen(){return fg183f_host_seen.load(std::memory_order_relaxed);}
extern "C" int fg183f_cuda_device_read(uint64_t*out){
 if(!out)return cudaErrorInvalidValue;
 auto e=cudaMemcpyFromSymbol(out,fg183f_paths,sizeof(uint64_t));if(e!=cudaSuccess)return e;
 return cudaMemcpyFromSymbol(out+1,fg183f_guards,sizeof(uint64_t));
}
static void fg183f_report(){
 uint64_t x[2]={};int e=fg183f_cuda_device_read(x);
 fprintf(stderr,"FG183F_CUDA_FWHT host=0x%llx,cuda_status=%d,paths=0x%llx,guards=%llu\n",
  (unsigned long long)fg183f_cuda_host_seen(),e,(unsigned long long)x[0],(unsigned long long)x[1]);
}
bool fg183f_native_ggml_cuda_op_fwht(ggml_backend_cuda_context&,const ggml_tensor*,ggml_tensor*);
bool ggml_cuda_op_fwht(ggml_backend_cuda_context &ctx,const ggml_tensor*src,ggml_tensor*dst){
 const char*flag=getenv("GGML_GOOSE_CUDA_FWHT");
 if(flag&&strcmp(flag,"0")==0)return fg183f_native_ggml_cuda_op_fwht(ctx,src,dst);
 static std::once_flag once;std::call_once(once,[]{std::atexit(fg183f_report);});
 fg183f_packet p{};
 p.stream=ctx.stream();
 p.v[0][0]=(uint64_t)(uintptr_t)src->data;
 p.v[0][1]=(uint64_t)(uintptr_t)dst->data;
 p.v[0][2]=(uint64_t)src->ne[0];
 p.v[0][3]=(uint64_t)ggml_nrows(src);
 p.v[0][4]=(src->type==GGML_TYPE_F32)?1ULL:0ULL;
 const int dev=ggml_cuda_get_device();
 p.v[0][5]=(uint64_t)ggml_cuda_info().devices[dev].warp_size;
 if(fg183fh_run((int64_t)(uintptr_t)&p)){
  fg183f_host_seen.fetch_or(p.launched?1ULL:16ULL,std::memory_order_relaxed);
  return true;
 }
 fg183f_host_seen.fetch_or(32ULL,std::memory_order_relaxed);
 return fg183f_native_ggml_cuda_op_fwht(ctx,src,dst);
}
