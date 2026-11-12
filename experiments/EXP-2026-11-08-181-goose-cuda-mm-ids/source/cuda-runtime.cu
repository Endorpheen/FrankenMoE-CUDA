#include "common.cuh"
#include "host-ffi.h"
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>

// Raw device services for the Goose MM IDs helper.
//
// The shared buffer is the kernel's own `extern __shared__` window; the accessor
// takes the block index (ignored, because each block owns its own window) and a
// byte offset. The shuffle primitives are the ones the native reductions use,
// including the width argument so the 8-wide group and the 32-wide warp ladder are
// both expressible.
//
// No original kernel, GGML struct or C++ routine is reachable.
// The kernel's dynamic shared window. Declared first so every accessor below sees
// the same symbol.
extern __shared__ char fg181m_shm[];
static __device__ __forceinline__ char * fg181m_shm_base(){return fg181m_shm;}
static __device__ unsigned long long fg181m_paths,fg181m_guards;
static __device__ __forceinline__ int64_t fg181m_ld_i32(uint64_t p,uint64_t o){return (int64_t)*(const int32_t*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ void    fg181m_st_i32(uint64_t p,uint64_t o,int32_t v){*(int32_t*)((char*)(uintptr_t)p+o)=v;}
static __device__ __forceinline__ int32_t fg181m_shfl_up(int32_t v,uint32_t m,int32_t o,int32_t w){return __shfl_up_sync(m,v,o,w);}
static __device__ __forceinline__ int32_t fg181m_shfl(int32_t v,uint32_t m,int32_t s,int32_t w){return __shfl_sync(m,v,s,w);}
static __device__ __forceinline__ int32_t fg181m_shfl_xor(int32_t v,uint32_t m,int32_t o,int32_t w){return __shfl_xor_sync(m,v,o,w);}
static __device__ __forceinline__ void    fg181m_syncwarp(){ggml_cuda_syncwarp();}
static __device__ __forceinline__ void    fg181m_fail(int64_t){atomicAdd(&fg181m_guards,1ULL);asm("trap;");}
// The Goose body addresses shared memory by (slot, byte) where slot identifies the
// block's window; one window per block, so the base is constant per launch.
static __device__ __forceinline__ void fg181m_shm_st(int32_t slot,int64_t byte,int32_t v){
 GGML_UNUSED(slot);*(int32_t*)(fg181m_shm_base()+byte)=v;}
static __device__ __forceinline__ int32_t fg181m_shm_ld(int32_t slot,int64_t byte){
 GGML_UNUSED(slot);return *(const int32_t*)(fg181m_shm_base()+byte);}
#define FG181M_HELPER fg181md_gg_function_1
#include "device-functions-generated.cuh"

// The kernel runs one warp per expert, exactly like native.
static __global__ __launch_bounds__(32,1) void fg181m_kernel(
 const char * ids,char * src1,char * dst,char * bounds,
 int32_t n_tokens,int32_t n_used,int32_t nch,int32_t si1,int32_t sis1,int32_t write_inverse){
 if(blockIdx.x==0&&threadIdx.x==0)atomicOr(&fg181m_paths,1);
 const uint64_t I=(uint64_t)(uintptr_t)ids,S=(uint64_t)(uintptr_t)src1;
 const uint64_t D=(uint64_t)(uintptr_t)dst,B=(uint64_t)(uintptr_t)bounds;
 FG181M_HELPER(I,S,D,B,n_tokens,n_used,nch,si1,sis1,write_inverse,n_used,
   (int32_t)gridDim.x-1,(int32_t)blockIdx.x,(int32_t)threadIdx.x,(int32_t)blockDim.x,0);
}

struct fg181m_packet{ggml_backend_cuda_context*ctx;cudaStream_t stream{};uint64_t v[8][32]{};bool launched=false;};
static std::atomic<uint64_t>fg181m_host_seen{0};
static fg181m_packet&fg181m_p(int64_t p){GGML_ASSERT(p);return*(fg181m_packet*)(uintptr_t)p;}

extern "C" int64_t fg181h_arg(int64_t p,int64_t k){GGML_UNUSED(k);return p;}
extern "C" int64_t fg181h_plan(int64_t p){return p;}
extern "C" void fg181h_put(int64_t p,int64_t k,int64_t i,uint64_t x){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);fg181m_p(p).v[k][i]=x;}
extern "C" uint64_t fg181h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);return fg181m_p(p).v[k][i];}
extern "C" void fg181h_fail(int64_t c){GGML_ABORT("Goose MM IDs validation code %lld",(long long)c);}
extern "C" int64_t fg181h_launch(int64_t pp){
 auto&q=fg181m_p(pp);q.launched=true;auto&v=q.v;
 const auto stream=q.stream;
 GGML_ASSERT(stream!=nullptr);
 const dim3 grid((unsigned)v[1][0],1u,1u);
 const dim3 block((unsigned)v[1][1]);
 const size_t shmem=(size_t)v[1][2];
 const auto lp=ggml_cuda_kernel_launch_params(grid,block,shmem,stream);
 ggml_cuda_kernel_launch(fg181m_kernel,lp,
   (const char*)(uintptr_t)v[0][0],(char*)(uintptr_t)v[0][1],
   (char*)(uintptr_t)v[0][2],(char*)(uintptr_t)v[0][3],
   (int32_t)v[2][0],(int32_t)v[2][1],(int32_t)v[2][2],
   (int32_t)v[2][3],(int32_t)v[2][4],(int32_t)v[2][5]);
 return cudaGetLastError();
}
extern "C" uint64_t fg181m_cuda_host_seen(){return fg181m_host_seen.load(std::memory_order_relaxed);}
extern "C" int fg181m_cuda_device_read(uint64_t*out){
 if(!out)return cudaErrorInvalidValue;
 auto e=cudaMemcpyFromSymbol(out,fg181m_paths,sizeof(uint64_t));if(e!=cudaSuccess)return e;
 return cudaMemcpyFromSymbol(out+1,fg181m_guards,sizeof(uint64_t));
}
static void fg181m_report(){
 uint64_t x[2]={};int e=fg181m_cuda_device_read(x);
 fprintf(stderr,"FG181M_CUDA_MM_IDS host=0x%llx,cuda_status=%d,paths=0x%llx,guards=%llu\n",
  (unsigned long long)fg181m_cuda_host_seen(),e,(unsigned long long)x[0],(unsigned long long)x[1]);
}
void fg181m_native_ggml_cuda_launch_mm_ids_helper(const int32_t*,int32_t*,int32_t*,int32_t*,
 int,int,int,int,int,int,bool,cudaStream_t);
void ggml_cuda_launch_mm_ids_helper(const int32_t*ids,int32_t*ids_src1,int32_t*ids_dst,int32_t*expert_bounds,
 int n_experts,int n_tokens,int n_expert_used,int nchannels_y,int si1,int sis1,bool write_inverse,cudaStream_t stream){
 const char*flag=getenv("GGML_GOOSE_CUDA_MM_IDS");
 if(flag&&strcmp(flag,"0")==0){
  fg181m_native_ggml_cuda_launch_mm_ids_helper(ids,ids_src1,ids_dst,expert_bounds,
    n_experts,n_tokens,n_expert_used,nchannels_y,si1,sis1,write_inverse,stream);
  return;
 }
 static std::once_flag once;std::call_once(once,[]{std::atexit(fg181m_report);});
 fg181m_packet p{};p.stream=stream;
 p.v[0][0]=(uint64_t)(uintptr_t)ids;p.v[0][1]=(uint64_t)(uintptr_t)ids_src1;
 p.v[0][2]=(uint64_t)(uintptr_t)ids_dst;p.v[0][3]=(uint64_t)(uintptr_t)expert_bounds;
 p.v[1][0]=(uint64_t)n_experts;p.v[1][1]=(uint64_t)n_tokens;
 p.v[1][2]=(uint64_t)n_expert_used;p.v[1][3]=(uint64_t)nchannels_y;
 p.v[1][4]=(uint64_t)si1;p.v[1][5]=(uint64_t)sis1;
 p.v[1][6]=write_inverse?1ULL:0ULL;
 const int dev=ggml_cuda_get_device();
 const int warp=ggml_cuda_info().devices[dev].warp_size;
 p.v[1][7]=(uint64_t)warp;
 size_t smpbo=0;
 cudaDeviceGetAttribute((int*)&smpbo,cudaDevAttrMaxSharedMemoryPerBlockOptin,dev);
 p.v[1][8]=(uint64_t)smpbo;
 if(fg181mh_run((int64_t)(uintptr_t)&p)){
  fg181m_host_seen.fetch_or(1ULL,std::memory_order_relaxed);
 }else{
  fg181m_host_seen.fetch_or(32ULL,std::memory_order_relaxed);
  fg181m_native_ggml_cuda_launch_mm_ids_helper(ids,ids_src1,ids_dst,expert_bounds,
    n_experts,n_tokens,n_expert_used,nchannels_y,si1,sis1,write_inverse,stream);
 }
}
