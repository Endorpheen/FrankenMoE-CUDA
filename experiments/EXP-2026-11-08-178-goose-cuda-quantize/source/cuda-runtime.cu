#include "common.cuh"
#include "host-ffi.h"
#include "device-types-generated.cuh"
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>

// Raw scalar device services for the Goose Q8_1 quantizer. The shuffles are the
// same __shfl_xor_sync the native reductions use, the half stores are the same
// round-to-nearest conversions, and the int8 store matches the native `q` write.
// No quantizer, cast helper, GGML struct or C++ routine is reachable.
static __device__ unsigned long long fg178q_paths,fg178q_guards;
static __device__ __forceinline__ float fg178q_ld_f32(uint64_t p,uint64_t o){return *(const float*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ void  fg178q_st_i8(uint64_t p,uint64_t o,int32_t v){*((int8_t*)((char*)(uintptr_t)p+o))=(int8_t)v;}
static __device__ __forceinline__ void  fg178q_st_f16(uint64_t p,uint64_t o,float v){*(half*)((char*)(uintptr_t)p+o)=__float2half_rn(v);}
static __device__ __forceinline__ float fg178q_shfl_xor(float v,uint32_t mask,int32_t off){return __shfl_xor_sync(mask,v,off);}
static __device__ __forceinline__ float fg178q_round(float v){return roundf(v);}
static __device__ __forceinline__ float fg178q_fabs(float v){return fabsf(v);}
static __device__ __forceinline__ int32_t fg178q_trunc_i32(float v){return (int32_t)v;}
static __device__ __forceinline__ uint32_t fg178q_mulhi(uint32_t a,uint32_t b){return __umulhi(a,b);}
static __device__ __forceinline__ void fg178q_pdl_sync(){ggml_cuda_pdl_sync();}
static __device__ __forceinline__ void fg178q_pdl_lc(){ggml_cuda_pdl_lc();}
static __device__ __forceinline__ void fg178q_fail(int64_t){atomicAdd(&fg178q_guards,1ULL);asm("trap;");}
#define FG178Q_QUANT fg178qd_gg_function_1
#include "device-functions-generated.cuh"

// One GPU thread runs one Goose body invocation. The Goose wrapper keeps the
// guarded load, both warp reductions and the two stores internally.
static __global__ __launch_bounds__(256) void fg178q_kernel(
 const char * x,char * y,
 int64_t ne00,int64_t s01,int64_t s02,int64_t s03,
 int64_t ne0,int64_t ne1,int64_t ne2,uint32_t mp,uint32_t sh,uint32_t dv){
 if(blockIdx.x==0&&blockIdx.y==0&&blockIdx.z==0&&threadIdx.x==0)atomicOr(&fg178q_paths,1);
 const uint64_t X=(uint64_t)(uintptr_t)x,Y=(uint64_t)(uintptr_t)y;
 FG178Q_QUANT(X,Y,ne00,s01,s02,s03,ne0,ne1,ne2,fg178q_FastDiv{mp,sh,dv},
  (int32_t)blockIdx.x,(int32_t)blockIdx.y,(int32_t)blockIdx.z,(int32_t)threadIdx.x,(int32_t)blockDim.x,0);
}

struct fg178q_packet{ggml_backend_cuda_context*ctx;ggml_tensor*src;ggml_tensor*dst;cudaStream_t stream{};uint64_t v[8][32]{};bool launched=false;};
static std::atomic<uint64_t>fg178q_host_seen{0};
static fg178q_packet&fg178q_p(int64_t p){GGML_ASSERT(p);return*(fg178q_packet*)(uintptr_t)p;}

extern "C" int64_t fg178h_arg(int64_t p,int64_t k){
 auto&q=fg178q_p(p);
 return(int64_t)(uintptr_t)(k==0?q.src:q.dst);
}
extern "C" int64_t fg178h_plan(int64_t p){return p;}
extern "C" void fg178h_put(int64_t p,int64_t k,int64_t i,uint64_t x){
 GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);
 fg178q_p(p).v[k][i]=x;}
extern "C" uint64_t fg178h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);return fg178q_p(p).v[k][i];}
extern "C" void fg178h_fail(int64_t c){GGML_ABORT("Goose Q8_1 validation code %lld",(long long)c);}
extern "C" int64_t fg178h_launch(int64_t pp){
 auto&q=fg178q_p(pp);q.launched=true;auto&v=q.v;
 // This entry has no ggml_backend_cuda_context: the native API receives the
 // stream directly, so the packet carries it as a typed field. Storing the
 // address of a caller local in a plan slot made the adapter read a stale
 // pointer, which corrupted every fastdiv value it saw.
 const cudaStream_t stream=q.stream;
 GGML_ASSERT(stream!=nullptr);
 const dim3 grid((unsigned)v[1][0],(unsigned)v[1][1],(unsigned)v[1][2]);
 const dim3 block((unsigned)v[1][3]);
 const auto lp=ggml_cuda_kernel_launch_params(grid,block,0,stream);
 const uint32_t mp=(uint32_t)v[6][0],sh=(uint32_t)v[6][1],dv=(uint32_t)v[6][2];
 ggml_cuda_kernel_launch(fg178q_kernel,lp,
   (const char*)(uintptr_t)v[0][3],(char*)(uintptr_t)v[0][4],
   (int64_t)v[2][0],(int64_t)v[2][1],(int64_t)v[2][2],(int64_t)v[2][3],
   (int64_t)v[2][4],(int64_t)v[2][5],(int64_t)v[2][6],mp,sh,dv);
 return cudaGetLastError();
}
extern "C" uint64_t fg178q_cuda_host_seen(){return fg178q_host_seen.load(std::memory_order_relaxed);}
extern "C" int fg178q_cuda_device_read(uint64_t*out){
 if(!out)return cudaErrorInvalidValue;
 auto e=cudaMemcpyFromSymbol(out,fg178q_paths,sizeof(uint64_t));if(e!=cudaSuccess)return e;
 return cudaMemcpyFromSymbol(out+1,fg178q_guards,sizeof(uint64_t));
}
static void fg178q_report(){
 uint64_t x[2]={};int e=fg178q_cuda_device_read(x);
 fprintf(stderr,"FG178Q_CUDA_Q8_1 host=0x%llx,cuda_status=%d,paths=0x%llx,guards=%llu\n",
  (unsigned long long)fg178q_cuda_host_seen(),e,(unsigned long long)x[0],(unsigned long long)x[1]);
}
void fg178q_native_quantize_row_q8_1_cuda(const float*,const int32_t*,void*,ggml_type,
 int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,cudaStream_t);
void quantize_row_q8_1_cuda(const float*x,const int32_t*ids,void*vy,ggml_type type_src0,
 int64_t ne00,int64_t s01,int64_t s02,int64_t s03,
 int64_t ne0,int64_t ne1,int64_t ne2,int64_t ne3,cudaStream_t stream){
 const char*flag=getenv("GGML_GOOSE_CUDA_Q8_1");
 if(flag&&strcmp(flag,"0")==0){
  fg178q_native_quantize_row_q8_1_cuda(x,ids,vy,type_src0,ne00,s01,s02,s03,ne0,ne1,ne2,ne3,stream);
  return;
 }
 static std::once_flag once;std::call_once(once,[]{std::atexit(fg178q_report);});
 // No ggml_tensor exists here: the native entry is scalar. The packet carries the
 // raw pointers and every extent/stride; plan slots 0..2 are read by the host
 // module, which owns all validation and the launch geometry.
 fg178q_packet p{};p.ctx=nullptr;
 p.v[0][0]=(uint64_t)(uintptr_t)x;
 p.v[0][1]=(uint64_t)(uintptr_t)vy;
 p.v[0][2]=(uint64_t)(uintptr_t)ids;
 p.v[1][0]=(uint64_t)ne00;p.v[1][1]=(uint64_t)s01;p.v[1][2]=(uint64_t)s02;p.v[1][3]=(uint64_t)s03;
 p.v[1][4]=(uint64_t)ne0;p.v[1][5]=(uint64_t)ne1;p.v[1][6]=(uint64_t)ne2;p.v[1][7]=(uint64_t)ne3;
 // The launch adapter needs the raw pointers and the stream, so place them where
 // the untyped plan reader expects them.
 p.v[0][3]=(uint64_t)(uintptr_t)x;
 p.v[0][4]=(uint64_t)(uintptr_t)vy;
 p.stream=stream;
 if(fg178qh_run((int64_t)(uintptr_t)&p)){
  fg178q_host_seen.fetch_or(1ULL,std::memory_order_relaxed);
 }else{
  fg178q_host_seen.fetch_or(32ULL,std::memory_order_relaxed);
  fg178q_native_quantize_row_q8_1_cuda(x,ids,vy,type_src0,ne00,s01,s02,s03,ne0,ne1,ne2,ne3,stream);
 }
}
