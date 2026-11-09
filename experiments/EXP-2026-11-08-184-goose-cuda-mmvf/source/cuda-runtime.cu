#include "common.cuh"
#include "mmvf.cuh"
#include "convert.cuh"
#include "host-ffi.h"
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>

// Raw device services for the Goose mul_mat_vec_f. Float and int32 loads, a float
// store, the shuffle/barrier pair and shared staging; nothing else is reachable.
static __device__ unsigned long long fg184m_paths,fg184m_guards;
static __device__ __forceinline__ float fg184m_ld_f32(uint64_t p,uint64_t o){return *(const float*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ int32_t fg184m_ld_i32(uint64_t p,uint64_t o){return *(const int32_t*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ void fg184m_st_f32(uint64_t p,uint64_t o,float v){*(float*)((char*)(uintptr_t)p+o)=v;}
static __device__ __forceinline__ float fg184m_mad(float a,float v,float u){return __fmaf_rn(v,u,a);}
static __device__ __forceinline__ float fg184m_nmad(float a,float v,float u){return __fadd_rn(a,__fmul_rn(v,u));}
static __device__ __forceinline__ float fg184m_shfl_xor(float v,int32_t o,int32_t w){return __shfl_xor_sync(0xFFFFFFFFu,v,o,w);}
static __device__ __forceinline__ void fg184m_barrier(){__syncthreads();}
static __device__ float * fg184m_buf;
static __device__ __forceinline__ void fg184m_shm_put(int32_t s,float v){fg184m_buf[s]=v;}
static __device__ __forceinline__ float fg184m_shm_get(int32_t s){return fg184m_buf[s];}
static __device__ __forceinline__ void fg184m_fail(int64_t){atomicAdd(&fg184m_guards,1ULL);asm("trap;");}
#define FG184M_ROW fg184md_gg_function_1
#include "device-functions-generated.cuh"

// Same grid as native: (nrows, nchannels_dst, nsamples) with block_size threads.
static __global__ __launch_bounds__(256) void fg184m_kernel(
 const char * x,const float * y,const char * ids,float * dst,
 int64_t ncols2,int64_t stride_row,int64_t stride_col_y2,int64_t stride_col_dst,
 int64_t scx,int64_t scy,int64_t scd,int64_t ssx,int64_t ssy,int64_t ssd,
 int64_t nchannels_y,int64_t channel_ratio,int32_t use_ids,int64_t ids_stride,int64_t block_size){
 if(blockIdx.x==0&&blockIdx.y==0&&blockIdx.z==0&&threadIdx.x==0)atomicOr(&fg184m_paths,1);
 extern __shared__ char fg184m_data[];
 fg184m_buf=(float*)fg184m_data;
 const uint64_t X=(uint64_t)(uintptr_t)x,Y=(uint64_t)(uintptr_t)y,D=(uint64_t)(uintptr_t)dst,I=(uint64_t)(uintptr_t)ids;
 const bool u=use_ids!=0;
 FG184M_ROW(X,Y,D,I,ncols2,stride_row,stride_col_y2,stride_col_dst,
  scx,scy,scd,ssx,ssy,ssd,nchannels_y,channel_ratio,u,ids_stride,
  (int64_t)blockIdx.x,(int64_t)blockIdx.y,(int64_t)blockIdx.z,block_size,(int64_t)threadIdx.x,0);
}

struct fg184m_packet{cudaStream_t stream{};uint64_t v[8][32]{};bool launched=false;};
static std::atomic<uint64_t>fg184m_host_seen{0};
// Diagnostic: whether calls arrive with a non-null ids table or a fusion struct, and how
// many distinct dst->ne[1] values are seen. A single-case run then answers the question
// directly instead of needing another fixture variant.
static std::atomic<uint64_t>fg184m_with_ids{0},fg184m_with_fusion{0},fg184m_ne1{0};
static fg184m_packet&fg184m_p(int64_t p){GGML_ASSERT(p);return*(fg184m_packet*)(uintptr_t)p;}

extern "C" int64_t fg184h_arg(int64_t p,int64_t k){GGML_UNUSED(k);return p;}
extern "C" int64_t fg184h_plan(int64_t p){return p;}
extern "C" void fg184h_put(int64_t p,int64_t k,int64_t i,uint64_t x){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);fg184m_p(p).v[k][i]=x;}
extern "C" uint64_t fg184h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);return fg184m_p(p).v[k][i];}
extern "C" void fg184h_fail(int64_t c){GGML_ABORT("Goose mul_mat_vec_f validation code %lld",(long long)c);}
extern "C" int64_t fg184h_launch(int64_t pp){
 auto&q=fg184m_p(pp);q.launched=true;auto&v=q.v;
 const auto stream=q.stream;
 GGML_ASSERT(stream!=nullptr);
 const dim3 grid((unsigned)v[7][0],(unsigned)v[7][1],(unsigned)v[7][2]);
 const dim3 block((unsigned)v[7][3],1u,1u);
 const int nbytes=(int)(v[7][4]*sizeof(float));
 const auto lp=ggml_cuda_kernel_launch_params(grid,block,(size_t)nbytes,stream);
 ggml_cuda_kernel_launch(fg184m_kernel,lp,
   (const char*)(uintptr_t)v[0][0],(const float*)(uintptr_t)v[0][1],
   v[0][3]?(const char*)(uintptr_t)v[0][3]:nullptr,(float*)(uintptr_t)v[0][2],
   (int64_t)(v[1][0]/2),(int64_t)v[1][1],(int64_t)v[1][2],(int64_t)v[1][3],
   (int64_t)v[2][0],(int64_t)v[2][1],(int64_t)v[2][2],
   (int64_t)v[3][0],(int64_t)v[3][1],(int64_t)v[3][2],
   (int64_t)v[4][3],(int64_t)v[5][4],(int32_t)v[5][2],(int64_t)v[5][3],(int64_t)v[7][3]);
 return cudaGetLastError();
}
extern "C" uint64_t fg184m_cuda_host_seen(){return fg184m_host_seen.load(std::memory_order_relaxed);}
extern "C" int fg184m_cuda_device_read(uint64_t*out){
 if(!out)return cudaErrorInvalidValue;
 auto e=cudaMemcpyFromSymbol(out,fg184m_paths,sizeof(uint64_t));if(e!=cudaSuccess)return e;
 return cudaMemcpyFromSymbol(out+1,fg184m_guards,sizeof(uint64_t));
}
static void fg184m_report(){
 uint64_t x[2]={};int e=fg184m_cuda_device_read(x);
 fprintf(stderr,"FG184M_CUDA_MMVF host=0x%llx,cuda_status=%d,paths=0x%llx,guards=%llu\n",
  (unsigned long long)fg184m_cuda_host_seen(),e,(unsigned long long)x[0],(unsigned long long)x[1]);
 fprintf(stderr,"FG184M_ARGS ids=%llu fusion=%llu ne1_bits=0x%llx\n",
  (unsigned long long)fg184m_with_ids.load(),(unsigned long long)fg184m_with_fusion.load(),
  (unsigned long long)fg184m_ne1.load());
}


// Capture: with FG184M_CAP=<dir> every call's destination is written out, so the same harness
// case can be run on both routes and the two outputs diffed directly instead of guessed at.
static void fg184m_capture(cudaStream_t s, const ggml_tensor * dst, const char * route) {
 const char * dir = getenv("FG184M_CAP");
 if (!dir || !dst || !dst->data) return;
 static std::atomic<int> seq{0};
 const int i = seq.fetch_add(1);
 if (i >= 64) return;
 cudaStreamSynchronize(s);
 const size_t nb = ggml_nbytes(dst);
 void * host = malloc(nb);
 if (!host) return;
 if (cudaMemcpy(host, dst->data, nb, cudaMemcpyDeviceToHost) == cudaSuccess) {
  char path[600];
  snprintf(path, sizeof path, "%s/%03d_%s_%lldx%lldx%lldx%lld.bin", dir, i, route,
           (long long)dst->ne[0],(long long)dst->ne[1],(long long)dst->ne[2],(long long)dst->ne[3]);
  if (FILE * f = fopen(path, "wb")) { fwrite(host, 1, nb, f); fclose(f); }
 }
 free(host);
}

void fg184m_native_ggml_cuda_mul_mat_vec_f(ggml_backend_cuda_context&,const ggml_tensor*,const ggml_tensor*,
 const ggml_tensor*,ggml_tensor*,const ggml_cuda_mm_fusion_args_host*);
void ggml_cuda_mul_mat_vec_f(ggml_backend_cuda_context &ctx,const ggml_tensor*src0,const ggml_tensor*src1,
 const ggml_tensor*ids,ggml_tensor*dst,const ggml_cuda_mm_fusion_args_host*fusion){
 const char*flag=getenv("GGML_GOOSE_CUDA_MMVF");
 if(flag&&strcmp(flag,"0")==0){fg184m_native_ggml_cuda_mul_mat_vec_f(ctx,src0,src1,ids,dst,fusion);fg184m_capture(ctx.stream(),dst,"natflag");return;}
 // Recorded for every call, whatever route it takes, so a single-case run answers what the
 // arguments were without needing another fixture variant.
 if(ids&&ids->data)fg184m_with_ids.fetch_add(1,std::memory_order_relaxed);
 if(fusion)fg184m_with_fusion.fetch_add(1,std::memory_order_relaxed);
 {const unsigned n1=(unsigned)(dst&&dst->ne[1]<64?dst->ne[1]:63);fg184m_ne1.fetch_or(1ULL<<n1,std::memory_order_relaxed);}
 static std::once_flag once;std::call_once(once,[]{std::atexit(fg184m_report);});
 const int dev=ggml_cuda_get_device();
 const int warp=ggml_cuda_info().devices[dev].warp_size;
 const int cc=ggml_cuda_info().devices[dev].cc;
 const int maxb=(cc>GGML_CUDA_CC_OFFSET_AMD&&cc<GGML_CUDA_CC_RDNA1)?128:256;
 GGML_TENSOR_BINARY_OP_LOCALS;
 fg184m_packet p{};
 p.stream=ctx.stream();
 p.v[0][0]=(uint64_t)(uintptr_t)src0->data;
 p.v[0][1]=(uint64_t)(uintptr_t)src1->data;
 p.v[0][2]=(uint64_t)(uintptr_t)dst->data;
 p.v[0][3]=ids?(uint64_t)(uintptr_t)ids->data:0ULL;
 p.v[1][0]=(uint64_t)ne00;
 p.v[1][1]=(uint64_t)(src0->nb[1]/ggml_type_size(src0->type));
 // native passes stride_col_y/2 because the kernel indexes float2 pairs
 p.v[1][2]=(uint64_t)((ids?src1->nb[2]/4:src1->nb[1]/4)/2);
 p.v[1][3]=(uint64_t)(ids?dst->nb[2]/4:dst->nb[1]/4);
 p.v[2][0]=(uint64_t)(src0->nb[2]/ggml_type_size(src0->type));
 p.v[2][1]=(uint64_t)(ids?src1->nb[1]/4:src1->nb[2]/4);
 p.v[2][2]=(uint64_t)(ids?dst->nb[1]/4:dst->nb[2]/4);
 p.v[3][0]=(uint64_t)(src0->nb[3]/ggml_type_size(src0->type));
 p.v[3][1]=(uint64_t)(src1->nb[3]/4);
 p.v[3][2]=(uint64_t)(dst->nb[3]/4);
 p.v[4][0]=(uint64_t)ne01;
 p.v[4][1]=(uint64_t)(ids?ne1:ne2);
 // third grid dim is nsamples_or_ntokens: ncols_dst with ids, nsamples_dst otherwise
 p.v[4][2]=(uint64_t)(ids?ne2:ne3);
 p.v[4][3]=(uint64_t)(ids?ne11:ne12);
 p.v[5][0]=(uint64_t)warp;
 p.v[5][1]=(uint64_t)maxb;
 p.v[5][3]=(uint64_t)(ids?ids->nb[1]/4:0);
 // channel_ratio = nchannels_dst / nchannels_x; src0 carries nchannels_x in ne[2] with
 // no separate stride on this profile, so the ratio is dst channels over src0 channels.
 // channel_ratio = nchannels_dst / nchannels_x, taken from the tensor shapes.
 p.v[5][4]=(uint64_t)(ids?0:((ids?ne1:ne2)/src0->ne[2]));
 p.v[6][1]=(uint64_t)(ids?ne2:ne1);
 // Eligibility: the Goose body implements only F32 source, one destination column,
 // no fusion, and the non-multi-token ids shape.
 const bool eligible = src0->type==GGML_TYPE_F32 && src1->type==GGML_TYPE_F32 &&
  dst->type==GGML_TYPE_F32 && (!ids||ids->type==GGML_TYPE_I32) &&
  (!ids||dst->ne[2]==1) && (ids||dst->ne[1]==1) &&
  (fusion==nullptr||(fusion->gate==nullptr&&fusion->x_bias==nullptr&&fusion->gate_bias==nullptr)) &&
  ne13==ne3 && ne3==1 && (ne10%2==0);
 p.v[6][0]=eligible?1ULL:0ULL;
 p.v[5][2]=ids?1ULL:0ULL;
 if(fg184hh_run((int64_t)(uintptr_t)&p)&&eligible){
  fg184m_host_seen.fetch_or(p.launched?1ULL:16ULL,std::memory_order_relaxed);
  fg184m_capture(p.stream,dst,"goose");
  return;
 }
 fg184m_host_seen.fetch_or(32ULL,std::memory_order_relaxed);
 fg184m_native_ggml_cuda_mul_mat_vec_f(ctx,src0,src1,ids,dst,fusion);
 fg184m_capture(ctx.stream(),dst,"native");
}
