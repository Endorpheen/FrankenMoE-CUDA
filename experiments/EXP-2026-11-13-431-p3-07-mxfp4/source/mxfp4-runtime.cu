#include "common.cuh"
#include "mmq.cuh"
#include "mxfp4-host-ffi.h"
#include <atomic>
#include <cstdlib>
#include <cstring>

static __device__ __forceinline__ float fg431_ld_f32(uint64_t p,uint64_t o){return *(const float*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ int64_t fg431_ld_i32(uint64_t p,uint64_t o){return (int64_t)*(const int32_t*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ void fg431_st_u16(uint64_t p,uint64_t o,int32_t v){*(uint16_t*)((char*)(uintptr_t)p+o)=(uint16_t)v;}
static __device__ __forceinline__ void fg431_st_i32(uint64_t p,uint64_t o,int32_t v){*(int32_t*)((char*)(uintptr_t)p+o)=v;}
static __device__ __forceinline__ float fg431_shfl_f32(float v,uint32_t m,int32_t o){return __shfl_xor_sync(m,v,o,32);}
static __device__ __forceinline__ int32_t fg431_shfl_i32(int32_t v,uint32_t m,int32_t s){return __shfl_sync(m,v,s,32);}
static __device__ __forceinline__ float fg431_fabs(float v){return fabsf(v);}
static __device__ __forceinline__ float fg431_log2(float v){return log2f(v);}
static __device__ __forceinline__ int32_t fg431_rint(float v){return __float2int_rn(v);}
static __device__ __forceinline__ float fg431_rcp(float v){return __frcp_rn(v);}
static __device__ __forceinline__ float fg431_bits_f32(uint32_t b){float f;memcpy(&f,&b,4);return f;}
static __device__ void fg431_fail(int64_t){asm("trap;");}
#if defined(__cplusplus) && !defined(_Alignas)
#define _Alignas(x) alignas(x)
#endif
#include "mxfp4-device-generated.cuh"

static __global__ void fg431_kernel(const float*x,const int32_t*ids,void*vy,
 int64_t ne00,int64_t s01,int64_t s02,int64_t s03,int64_t ne0,int64_t ne1,int64_t ne2,
 int32_t n_used,int32_t scatter){
 FG431D_KERNEL((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)ids,(uint64_t)(uintptr_t)vy,
  ne00,s01,s02,s03,ne0,ne1,ne2,n_used,
  (int32_t)blockIdx.x,(int32_t)blockIdx.y,(int32_t)blockIdx.z,
  (int32_t)threadIdx.x,(int32_t)threadIdx.y,(int32_t)blockDim.x,(int32_t)blockDim.y,scatter,0);
}

struct fg431_packet{uint64_t v[8][32]{};cudaStream_t stream{};};
static fg431_packet&fg431_p(int64_t p){GGML_ASSERT(p);return*(fg431_packet*)(uintptr_t)p;}
extern "C" int64_t fg431h_plan(int64_t p){return p;}
extern "C" void fg431h_put(int64_t p,int64_t k,int64_t i,uint64_t x){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);fg431_p(p).v[k][i]=x;}
extern "C" uint64_t fg431h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);return fg431_p(p).v[k][i];}
extern "C" void fg431h_fail(int64_t c){GGML_ABORT("Goose MXFP4 validation code %lld",(long long)c);}
extern "C" int64_t fg431h_launch(int64_t pp){
 auto&q=fg431_p(pp);auto&v=q.v;
 const dim3 grid((unsigned)v[1][0],(unsigned)v[1][1],(unsigned)v[1][2]);
 const dim3 block((unsigned)v[1][3],(unsigned)v[1][4],(unsigned)v[1][5]);
 fg431_kernel<<<grid,block,0,q.stream>>>(
  (const float*)(uintptr_t)v[0][0],(const int32_t*)(uintptr_t)v[0][1],(void*)(uintptr_t)v[0][2],
  (int64_t)v[4][0],(int64_t)v[3][0],(int64_t)v[3][1],(int64_t)v[3][2],
  (int64_t)v[4][1],(int64_t)v[3][3],(int64_t)v[3][4],(int32_t)v[4][2],(int32_t)v[2][0]);
 return cudaGetLastError();
}

void fg431_native_quantize_mmq_fp4_cuda(const float*,const int32_t*,void*,float*,ggml_type,bool,
 int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,cudaStream_t);
void fg431_native_quantize_scatter_mmq_fp4_cuda(const float*,const int32_t*,void*,float*,ggml_type,bool,
 int64_t,int64_t,int64_t,int64_t,int64_t,int,cudaStream_t);

static bool fg431_goose(){const char*f=getenv("GGML_GOOSE_CUDA_MXFP4");return !(f&&strcmp(f,"0")==0);}

void quantize_mmq_fp4_cuda(const float*x,const int32_t*ids,void*vy,float*scale,ggml_type type_src0,
 bool use_aligned_float8,int64_t ne00,int64_t s01,int64_t s02,int64_t s03,
 int64_t ne0,int64_t ne1,int64_t ne2,int64_t ne3,cudaStream_t stream){
 if(type_src0==GGML_TYPE_MXFP4&&fg431_goose()){
  fg431_packet p{};p.stream=stream;
  p.v[0][0]=(uint64_t)(uintptr_t)x;p.v[0][1]=(uint64_t)(uintptr_t)ids;p.v[0][2]=(uint64_t)(uintptr_t)vy;
  p.v[1][0]=(uint64_t)ne00;p.v[1][1]=(uint64_t)s01;p.v[1][2]=(uint64_t)s02;p.v[1][3]=(uint64_t)s03;
  p.v[1][4]=(uint64_t)ne0;p.v[1][5]=(uint64_t)ne1;p.v[1][6]=(uint64_t)ne2;p.v[1][7]=(uint64_t)ne3;
  p.v[2][0]=0;p.v[2][1]=0;p.v[2][2]=0;p.v[2][3]=0;
  if(fg431h_run((int64_t)(uintptr_t)&p))return;
 }
 fg431_native_quantize_mmq_fp4_cuda(x,ids,vy,scale,type_src0,use_aligned_float8,
  ne00,s01,s02,s03,ne0,ne1,ne2,ne3,stream);
}

void quantize_scatter_mmq_fp4_cuda(const float*x,const int32_t*ids_inv,void*vy,float*scale,
 ggml_type type_src0,bool use_aligned_float8,int64_t ne00,int64_t stride_token,
 int64_t ne0,int64_t n_tokens,int64_t nrows_dst,int n_expert_used,cudaStream_t stream){
 if(type_src0==GGML_TYPE_MXFP4&&fg431_goose()){
  fg431_packet p{};p.stream=stream;
  p.v[0][0]=(uint64_t)(uintptr_t)x;p.v[0][1]=(uint64_t)(uintptr_t)ids_inv;p.v[0][2]=(uint64_t)(uintptr_t)vy;
  p.v[1][0]=(uint64_t)ne00;p.v[1][1]=0;p.v[1][2]=(uint64_t)stride_token;p.v[1][3]=0;
  p.v[1][4]=(uint64_t)ne0;p.v[1][5]=(uint64_t)nrows_dst;p.v[1][6]=1;p.v[1][7]=1;
  p.v[2][0]=1;p.v[2][1]=(uint64_t)n_tokens;p.v[2][2]=(uint64_t)nrows_dst;p.v[2][3]=(uint64_t)n_expert_used;
  p.v[2][4]=(uint64_t)stride_token;
  if(fg431h_run((int64_t)(uintptr_t)&p))return;
 }
 fg431_native_quantize_scatter_mmq_fp4_cuda(x,ids_inv,vy,scale,type_src0,use_aligned_float8,
  ne00,stride_token,ne0,n_tokens,nrows_dst,n_expert_used,stream);
}
