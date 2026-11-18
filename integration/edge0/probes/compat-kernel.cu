// Probe only: Goose integer dot pieces with CPU-packed activation blocks.
#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <cstdint>
#define GGML_COMMON_DECL_CUDA
#define GGML_COMMON_IMPL_CUDA
#include "ggml-common.h"
static __device__ __forceinline__ uint32_t fg251d_ld_u8(uint64_t p,uint64_t o){return ((const uint8_t*)p)[o];}
static __device__ __forceinline__ int32_t e0c_ld_i8(uint64_t p,uint64_t o){return ((const int8_t*)p)[o];}
static __device__ __forceinline__ int32_t e0c_iq4_value(int32_t i){return kvalues_iq4nl[i];}
#include "device.cuh"
template<int TYPE> __global__ void gemv(const uint8_t*w,const uint8_t*a,float*out,int n,int rows){
 int row=(blockIdx.x*blockDim.x+threadIdx.x)/32,lane=threadIdx.x&31;if(row>=rows)return;
 constexpr int wb=TYPE==11?110:18,ab=TYPE==11?292:34,qk=TYPE==11?256:32;
 const uint8_t*r=w+(size_t)row*(n/qk)*wb;float acc=0;
 for(int b=0;b<n/qk;++b){
  int part=TYPE==11?e0c_q3_part_g((uint64_t)r,(uint64_t)a,b,lane):TYPE==2?e0c_q4_part_g((uint64_t)r,(uint64_t)a,b,lane):e0c_iq4_part_g((uint64_t)r,(uint64_t)a,b,lane);
  part+=__shfl_down_sync(0xffffffff,part,16);part+=__shfl_down_sync(0xffffffff,part,8);
  float d=__half2float(*(const half*)(r+b*wb+(TYPE==11?108:0)));
  float ad=TYPE==11?*(const float*)(a+b*ab):__half2float(*(const half*)(a+b*ab));
  if(lane<8)acc=__fmaf_rn(__fmul_rn(d,ad),(float)part,acc);
 }
 acc+=__shfl_down_sync(0xffffffff,acc,4);acc+=__shfl_down_sync(0xffffffff,acc,2);acc+=__shfl_down_sync(0xffffffff,acc,1);
 if(lane==0)out[row]=acc;
}
extern "C" int e0c_gemv(int type,const void*w,size_t wn,const void*a,size_t an,float*out,int n,int rows){
 uint8_t *dw=nullptr,*da=nullptr;float*dy=nullptr;cudaError_t e=cudaSuccess;
 #define TRY(x) do{e=(x);if(e!=cudaSuccess)goto done;}while(0)
 TRY(cudaMalloc(&dw,wn));TRY(cudaMalloc(&da,an));TRY(cudaMalloc(&dy,rows*sizeof(float)));
 TRY(cudaMemcpy(dw,w,wn,cudaMemcpyHostToDevice));TRY(cudaMemcpy(da,a,an,cudaMemcpyHostToDevice));
 if(type==11)gemv<11><<<(rows+3)/4,128>>>(dw,da,dy,n,rows);
 else if(type==2)gemv<2><<<(rows+3)/4,128>>>(dw,da,dy,n,rows);
 else if(type==20)gemv<20><<<(rows+3)/4,128>>>(dw,da,dy,n,rows);
 else {e=cudaErrorInvalidValue;goto done;}
 TRY(cudaGetLastError());TRY(cudaMemcpy(out,dy,rows*sizeof(float),cudaMemcpyDeviceToHost));
 done:cudaFree(dw);cudaFree(da);cudaFree(dy);return (int)e;
}

// Native CUDA resource debt: fixed residency and bounded tagged record slots.
#include "compat-api.h"
#include <cstdio>
#include <array>
struct resident_state {
 uint8_t *du=nullptr,*experts=nullptr,*activation=nullptr;float*output=nullptr;
 cudaStream_t stream=nullptr;std::array<uint64_t,512>tags;
 uint64_t hits=0,misses=0,calls=0,bytes=0;
};
extern "C" void e0gpu_destroy(void *v){auto*s=(resident_state*)v;if(!s)return;if(s->stream)cudaStreamSynchronize(s->stream);cudaFree(s->du);cudaFree(s->experts);cudaFree(s->activation);cudaFree(s->output);if(s->stream)cudaStreamDestroy(s->stream);delete s;}
extern "C" void* e0gpu_create(const void*du,size_t bytes){
 if(bytes!=1415577600ULL)return nullptr;auto*s=new resident_state;s->tags.fill(UINT64_MAX);
 cudaError_t e=cudaStreamCreateWithFlags(&s->stream,cudaStreamNonBlocking);
 if(e==cudaSuccess)e=cudaMalloc(&s->du,bytes);
 if(e==cudaSuccess)e=cudaMalloc(&s->experts,512ULL*1164800);
 if(e==cudaSuccess)e=cudaMalloc(&s->activation,4096);
 if(e==cudaSuccess)e=cudaMalloc(&s->output,2560*sizeof(float));
 if(e==cudaSuccess)e=cudaMemcpy(s->du,du,bytes,cudaMemcpyHostToDevice);
 if(e!=cudaSuccess){std::fprintf(stderr,"EDGE0_GPU create failed: %s\n",cudaGetErrorString(e));e0gpu_destroy(s);return nullptr;}
 std::fprintf(stderr,"EDGE0_GPU DU=%zu expert_cache=%llu activation=4096 output=10240 slots=512\n",bytes,512ULL*1164800);return s;
}
extern "C" int e0gpu_dot(void*v,int type,uint64_t key,int matrix,const void*record,const void*a,size_t an,float*out,int n,int rows){
 auto*s=(resident_state*)v;if(!s||an>4096||rows>2560)return (int)cudaErrorInvalidValue;
 const uint8_t*w=nullptr;cudaError_t e=cudaSuccess;
 if(type==2){if(key+(size_t)n*rows*18/32>1415577600ULL)return (int)cudaErrorInvalidValue;w=s->du+key;}
 else {
  if(!record||key>=24576||matrix<2||matrix>4)return (int)cudaErrorInvalidValue;
  uint64_t hash=key*2654435761ULL;unsigned slot=(unsigned)((hash^(hash>>16))&511);auto*dst=s->experts+(size_t)slot*1164800;
  if(s->tags[slot]!=key){e=cudaMemcpyAsync(dst,record,1164800,cudaMemcpyHostToDevice,s->stream);if(e!=cudaSuccess)return (int)e;s->tags[slot]=key;++s->misses;s->bytes+=1164800;}else ++s->hits;
  w=dst+(matrix==2?0:(matrix==3?352000:704000));
 }
 e=cudaMemcpyAsync(s->activation,a,an,cudaMemcpyHostToDevice,s->stream);if(e!=cudaSuccess)return (int)e;
 if(type==11)gemv<11><<<(rows+3)/4,128,0,s->stream>>>(w,s->activation,s->output,n,rows);
 else if(type==2)gemv<2><<<(rows+3)/4,128,0,s->stream>>>(w,s->activation,s->output,n,rows);
 else if(type==20)gemv<20><<<(rows+3)/4,128,0,s->stream>>>(w,s->activation,s->output,n,rows);
 else return (int)cudaErrorInvalidValue;
 e=cudaGetLastError();if(e!=cudaSuccess)return(int)e;
 e=cudaMemcpyAsync(out,s->output,rows*sizeof(float),cudaMemcpyDeviceToHost,s->stream);if(e!=cudaSuccess)return(int)e;
 e=cudaStreamSynchronize(s->stream);++s->calls;return(int)e;
}
extern "C" void e0gpu_stats(void*v){auto*s=(resident_state*)v;if(s)std::fprintf(stderr,"EDGE0_GPU_STATS calls=%llu hits=%llu misses=%llu h2d_weight_bytes=%llu\n",(unsigned long long)s->calls,(unsigned long long)s->hits,(unsigned long long)s->misses,(unsigned long long)s->bytes);}
