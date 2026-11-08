#include "common.cuh"
#include "host-ffi.h"
#include "device-types-generated.cuh"
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>
static __device__ unsigned long long fg176r_paths,fg176r_bad_indices;
static __device__ __forceinline__ float fg176r_ld_f32(uint64_t p,uint64_t o){return*(const float*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ void fg176r_st_f32(uint64_t p,uint64_t o,float v){*(float*)((char*)(uintptr_t)p+o)=v;}
static __device__ __forceinline__ void fg176r_st_f16(uint64_t p,uint64_t o,float v){*(half*)((char*)(uintptr_t)p+o)=__float2half_rn(v);}
static __device__ __forceinline__ void fg176r_st_u8(uint64_t p,uint64_t o,uint32_t v){*((uint8_t*)(uintptr_t)p+o)=(uint8_t)v;}
static __device__ __forceinline__ int64_t fg176r_ld_i32(uint64_t p,uint64_t o){return(int64_t)*(const int32_t*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ int64_t fg176r_ld_i64(uint64_t p,uint64_t o){return*(const int64_t*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ float fg176r_fabs(float v){return fabsf(v);}
static __device__ __forceinline__ int32_t fg176r_i8trunc(float v){return(int8_t)v;}
static __device__ __forceinline__ void fg176r_pdl_sync(){ggml_cuda_pdl_sync();}
static __device__ __forceinline__ void fg176r_pdl_lc(){ggml_cuda_pdl_lc();}
static __device__ __forceinline__ uint32_t fg176r_mulhi(uint32_t a,uint32_t b){return __umulhi(a,b);}
static __device__ __forceinline__ void fg176r_bad_index(){atomicAdd(&fg176r_bad_indices,1ULL);}
#include "device-functions-generated.cuh"
template<bool Q4,bool I64>static __global__ __launch_bounds__(256) void fg176r_kernel(const float*__restrict__ a,const void*__restrict__ ix,void*__restrict__ dst,int64_t total,fg176r_FastDiv d00,fg176r_FastDiv d01,fg176r_FastDiv d02,fg176r_FastDiv d11,fg176r_FastDiv d12,int64_t s01,int64_t s02,int64_t s03,int64_t s10,int64_t s11,int64_t s12,int64_t s1,int64_t s2,int64_t s3,int64_t rows){
if(blockIdx.x==0&&threadIdx.x==0)atomicOr(&fg176r_paths,1ULL<<((Q4?2:0)+(I64?1:0)));
if constexpr(Q4)FG176R_Q4((uint64_t)(uintptr_t)a,(uint64_t)(uintptr_t)ix,(uint64_t)(uintptr_t)dst,total,d00,d01,d02,d11,d12,s01,s02,s03,s10,s11,s12,s1,s2,s3,rows,1,I64,blockIdx.x,threadIdx.x,blockDim.x,1);else FG176R_F32((uint64_t)(uintptr_t)a,(uint64_t)(uintptr_t)ix,(uint64_t)(uintptr_t)dst,total,d00,d01,d02,d11,d12,s01,s02,s03,s10,s11,s12,s1,s2,s3,rows,1,I64,blockIdx.x,threadIdx.x,blockDim.x,1);}

struct fg176r_packet{ggml_backend_cuda_context*ctx;ggml_tensor*dst;uint64_t v[10][32]{};bool launched=false;};
static std::atomic<uint64_t>fg176r_host_paths{0};
static fg176r_packet&fg176r_p(int64_t p){GGML_ASSERT(p);return*(fg176r_packet*)(uintptr_t)p;}
static const ggml_tensor&fg176r_t(int64_t p){GGML_ASSERT(p);return*(const ggml_tensor*)(uintptr_t)p;}
extern "C" int64_t fg175s_arg(int64_t p,int64_t k){GGML_ASSERT(k==0);return(int64_t)(uintptr_t)fg176r_p(p).dst;}
extern "C" int64_t fg175s_plan(int64_t p){return p;}
extern "C" void fg175s_put(int64_t p,int64_t k,int64_t i,uint64_t x){GGML_ASSERT(k>=0&&k<10&&i>=0&&i<32);fg176r_p(p).v[k][i]=x;}
extern "C" uint64_t fg175s_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<10&&i>=0&&i<32);return fg176r_p(p).v[k][i];}
extern "C" int64_t fg175s_ti(int64_t p,int64_t k,int64_t i){const auto&t=fg176r_t(p);switch(k){case 0:GGML_ASSERT(i>=0&&i<GGML_MAX_SRC);return(int64_t)(uintptr_t)t.src[i];case 1:return(int64_t)(uintptr_t)t.data;case 2:return t.type;case 3:GGML_ASSERT(i>=0&&i<4);return t.ne[i];case 4:GGML_ASSERT(i>=0&&i<4);GGML_ASSERT(t.nb[i]<=INT64_MAX);return t.nb[i];case 5:return t.op;case 6:return(int64_t)(uintptr_t)t.view_src;case 7:GGML_ASSERT(t.view_offs<=INT64_MAX);return t.view_offs;case 9:return ggml_nbytes(&t);default:GGML_ABORT("SET_ROWS tensor field");}}
extern "C" int64_t fg175s_tc(int64_t k){switch(k){case 1:return GGML_TYPE_F32;case 3:return GGML_TYPE_Q4_0;case 8:return GGML_TYPE_I32;case 9:return GGML_TYPE_I64;default:GGML_ABORT("SET_ROWS type query");}}
extern "C" int64_t fg175s_oc(int64_t k){GGML_ASSERT(k==0);return GGML_OP_SET_ROWS;}
extern "C" void fg175s_fail(int64_t code){GGML_ABORT("Goose SET_ROWS validation code %lld",(long long)code);}
static fg176r_FastDiv fg176r_fd(const fg176r_packet&p,int i){return{(uint32_t)p.v[8][i*3],(uint32_t)p.v[8][i*3+1],(uint32_t)p.v[8][i*3+2]};}
extern "C" int64_t fg175s_launch(int64_t pp){auto&p=fg176r_p(pp);GGML_ASSERT(p.ctx);p.launched=true;auto&v=p.v;const auto stream=p.ctx->stream();dim3 grid(v[7][5],v[7][6],v[7][7]);dim3 block(v[7][8]);ggml_cuda_kernel_launch_params lp(grid,block,v[7][9],stream);switch(v[7][12]){
case 0:ggml_cuda_kernel_launch(fg176r_kernel<0,0>,lp,(const float*)(uintptr_t)v[0][0],(const void*)(uintptr_t)v[0][1],(void*)(uintptr_t)v[0][2],(int64_t)v[7][0],fg176r_fd(p,0),fg176r_fd(p,1),fg176r_fd(p,2),fg176r_fd(p,3),fg176r_fd(p,4),(int64_t)v[4][0],(int64_t)v[4][1],(int64_t)v[4][2],(int64_t)v[5][0],(int64_t)v[5][1],(int64_t)v[5][2],(int64_t)v[6][0],(int64_t)v[6][1],(int64_t)v[6][2],(int64_t)v[7][14]);break;
case 1:ggml_cuda_kernel_launch(fg176r_kernel<0,1>,lp,(const float*)(uintptr_t)v[0][0],(const void*)(uintptr_t)v[0][1],(void*)(uintptr_t)v[0][2],(int64_t)v[7][0],fg176r_fd(p,0),fg176r_fd(p,1),fg176r_fd(p,2),fg176r_fd(p,3),fg176r_fd(p,4),(int64_t)v[4][0],(int64_t)v[4][1],(int64_t)v[4][2],(int64_t)v[5][0],(int64_t)v[5][1],(int64_t)v[5][2],(int64_t)v[6][0],(int64_t)v[6][1],(int64_t)v[6][2],(int64_t)v[7][14]);break;
case 2:fg176r_kernel<1,0><<<grid,block,0,stream>>>((const float*)(uintptr_t)v[0][0],(const void*)(uintptr_t)v[0][1],(void*)(uintptr_t)v[0][2],(int64_t)v[7][0],fg176r_fd(p,0),fg176r_fd(p,1),fg176r_fd(p,2),fg176r_fd(p,3),fg176r_fd(p,4),(int64_t)v[4][0],(int64_t)v[4][1],(int64_t)v[4][2],(int64_t)v[5][0],(int64_t)v[5][1],(int64_t)v[5][2],(int64_t)v[6][0],(int64_t)v[6][1],(int64_t)v[6][2],(int64_t)v[7][14]);break;
case 3:fg176r_kernel<1,1><<<grid,block,0,stream>>>((const float*)(uintptr_t)v[0][0],(const void*)(uintptr_t)v[0][1],(void*)(uintptr_t)v[0][2],(int64_t)v[7][0],fg176r_fd(p,0),fg176r_fd(p,1),fg176r_fd(p,2),fg176r_fd(p,3),fg176r_fd(p,4),(int64_t)v[4][0],(int64_t)v[4][1],(int64_t)v[4][2],(int64_t)v[5][0],(int64_t)v[5][1],(int64_t)v[5][2],(int64_t)v[6][0],(int64_t)v[6][1],(int64_t)v[6][2],(int64_t)v[7][14]);break;
default:return cudaErrorInvalidValue;}return cudaGetLastError();}

extern "C" uint64_t fg176r_cuda_host_seen(){return fg176r_host_paths.load(std::memory_order_relaxed);}
extern "C" int fg176r_cuda_device_read(uint64_t*out){if(!out)return cudaErrorInvalidValue;auto e=cudaMemcpyFromSymbol(out,fg176r_paths,sizeof(uint64_t));if(e!=cudaSuccess)return e;return cudaMemcpyFromSymbol(out+1,fg176r_bad_indices,sizeof(uint64_t));}
static void fg176r_report(){uint64_t x[2]={};int e=fg176r_cuda_device_read(x);fprintf(stderr,"FG176R_CUDA_SET_ROWS host=0x%llx,cuda_status=%d,paths=0x%llx,invalid_index_threads=%llu\n",(unsigned long long)fg176r_cuda_host_seen(),e,(unsigned long long)x[0],(unsigned long long)x[1]);}
void fg176r_native_ggml_cuda_op_set_rows(ggml_backend_cuda_context&,ggml_tensor*);
void ggml_cuda_op_set_rows(ggml_backend_cuda_context&ctx,ggml_tensor*dst){const char*flag=getenv("GGML_GOOSE_CUDA_SET_ROWS");if(flag&&strcmp(flag,"0")==0){fg176r_native_ggml_cuda_op_set_rows(ctx,dst);return;}static std::once_flag once;std::call_once(once,[]{std::atexit(fg176r_report);});fg176r_packet p{&ctx,dst};if(fg176rh_run((int64_t)(uintptr_t)&p)){fg176r_host_paths.fetch_or(p.launched?1ULL<<p.v[7][12]:16ULL,std::memory_order_relaxed);}else{fg176r_host_paths.fetch_or(32ULL,std::memory_order_relaxed);fg176r_native_ggml_cuda_op_set_rows(ctx,dst);}}
