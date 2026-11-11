#include "common.cuh"
#include "mmvq.cuh"
#include "convert.cuh"
#include "host-ffi.h"
#include "quantize.cuh"
#include "ggml-cuda.h"
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>

// Raw device services for the Goose mul_mat_vec_q. Integer loads, the half2 unpack,
// dp4a, a float store, the shuffle/barrier pair and shared staging; nothing else.
static __device__ unsigned long long fg186q_paths,fg186q_guards;
static __device__ __forceinline__ int32_t fg186q_ld_i32(uint64_t p,uint64_t o){return *(const int32_t*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ uint32_t fg186q_ld_i16(uint64_t p,uint64_t o){return *(const uint16_t*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ int32_t fg186q_ld_i8(uint64_t p,uint64_t o){return *(const int8_t*)((const char*)(uintptr_t)p+o);}
static __device__ __forceinline__ float fg186q_ld_half(uint64_t p,uint64_t o){return __half2float(*(const half*)((const char*)(uintptr_t)p+o));}
static __device__ __forceinline__ int32_t fg186q_dp4a(int32_t a,int32_t b,int32_t c){return __dp4a(a,b,c);}
static __device__ __forceinline__ void fg186q_st_f32(uint64_t p,uint64_t o,float v){*(float*)((char*)(uintptr_t)p+o)=v;}
static __device__ __forceinline__ float fg186q_shfl_xor(float v,int32_t o,int32_t w){return __shfl_xor_sync(0xFFFFFFFFu,v,o,w);}
static __device__ __forceinline__ void fg186q_barrier(){__syncthreads();}
static __device__ float * fg186q_buf;
static __device__ __forceinline__ void fg186q_shm_put(int32_t w,int32_t l,float v){fg186q_buf[w*32+l]=v;}
static __device__ __forceinline__ float fg186q_shm_get(int32_t w,int32_t l){return fg186q_buf[w*32+l];}
static __device__ __forceinline__ void fg186q_fail(int64_t){atomicAdd(&fg186q_guards,1ULL);asm("trap;");}
#define FG186Q_BODY fg186qd_gg_function_1
#include "device-functions-generated.cuh"

// Same grid as native: (ceil(nrows/rpb), nchannels_dst, nsamples) with block (32,nwarps).
static __global__ __launch_bounds__(128) void fg186q_kernel(
 const char * vx,const char * vy,const char * ids,float * dst,
 int64_t ncols_x,int64_t blocks_per_row,int64_t blocks_per_iter,
 int64_t stride_row_x,int64_t stride_col_y,int64_t stride_col_dst,
 int64_t scx,int64_t scy,int64_t scd,int64_t ssx,int64_t ssy,int64_t ssd,
 int64_t nchannels_y,int64_t channel_ratio,int64_t sample_ratio,int32_t use_ids,int64_t ids_stride,
 int64_t nwarps){
 if(blockIdx.x==0&&blockIdx.y==0&&blockIdx.z==0&&threadIdx.x==0&&threadIdx.y==0)atomicOr(&fg186q_paths,1);
 extern __shared__ char fg186q_data[];
 fg186q_buf=(float*)fg186q_data;
 const uint64_t X=(uint64_t)(uintptr_t)vx,Y=(uint64_t)(uintptr_t)vy,D=(uint64_t)(uintptr_t)dst,I=(uint64_t)(uintptr_t)ids;
 const bool u=use_ids!=0;
 FG186Q_BODY(X,Y,I,D,ncols_x,blocks_per_row,blocks_per_iter,
  stride_row_x,stride_col_y,stride_col_dst,scx,scy,scd,ssx,ssy,ssd,
  nchannels_y,channel_ratio,sample_ratio,u,ids_stride,nwarps,
  (int64_t)blockIdx.x,(int64_t)threadIdx.x,(int64_t)threadIdx.y,
  (int64_t)blockIdx.y,(int64_t)blockIdx.z,0);
}

struct fg186q_packet{cudaStream_t stream{};uint64_t v[8][32]{};bool launched=false;};
static std::atomic<uint64_t>fg186q_host_seen{0};
static fg186q_packet&fg186q_p(int64_t p){GGML_ASSERT(p);return*(fg186q_packet*)(uintptr_t)p;}

extern "C" int64_t fg186h_arg(int64_t p,int64_t k){GGML_UNUSED(k);return p;}
extern "C" int64_t fg186h_plan(int64_t p){return p;}
extern "C" void fg186h_put(int64_t p,int64_t k,int64_t i,uint64_t x){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);fg186q_p(p).v[k][i]=x;}
extern "C" uint64_t fg186h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<32);return fg186q_p(p).v[k][i];}
extern "C" void fg186h_fail(int64_t c){GGML_ABORT("Goose mul_mat_vec_q Q6_K validation code %lld",(long long)c);}
extern "C" int64_t fg186h_launch(int64_t pp){
 auto&q=fg186q_p(pp);q.launched=true;auto&v=q.v;
 const auto stream=q.stream;
 GGML_ASSERT(stream!=nullptr);
 const dim3 grid((unsigned)v[7][0],(unsigned)v[7][1],(unsigned)v[7][2]);
 const dim3 block((unsigned)v[7][5],(unsigned)v[7][6],1u);
 // native shared size: (nwarps-1)*ncols_dst*rows_per_block*warp_size*4
 const int nbytes=(int)((v[7][6]-1)*v[7][5]*sizeof(float));
 fprintf(stderr,"FG186T launch grid=(%u,%u,%u) block=(%u,%u) shm=%d v7=[%llu,%llu,%llu,%llu,%llu,%llu,%llu]\n",
   grid.x,grid.y,grid.z,block.x,block.y,nbytes,
   (unsigned long long)v[7][0],(unsigned long long)v[7][1],(unsigned long long)v[7][2],
   (unsigned long long)v[7][3],(unsigned long long)v[7][4],(unsigned long long)v[7][5],(unsigned long long)v[7][6]);
 const auto lp=ggml_cuda_kernel_launch_params(grid,block,(size_t)nbytes,stream);
 ggml_cuda_kernel_launch(fg186q_kernel,lp,
   (const char*)(uintptr_t)v[0][0],(const char*)(uintptr_t)v[0][1],
   v[0][3]?(const char*)(uintptr_t)v[0][3]:nullptr,(float*)(uintptr_t)v[0][2],
   (int64_t)v[1][0],(int64_t)v[7][4],(int64_t)v[7][3],
   (int64_t)v[1][1],(int64_t)v[1][2],(int64_t)v[1][3],
   (int64_t)v[2][0],(int64_t)v[2][1],(int64_t)v[2][2],
   (int64_t)v[3][0],(int64_t)v[3][1],(int64_t)v[3][2],
   (int64_t)v[4][3],(int64_t)v[5][4],(int64_t)v[5][5],(int32_t)v[5][2],(int64_t)v[5][3],
   (int64_t)v[7][6]);
 return cudaGetLastError();
}
extern "C" uint64_t fg186q_cuda_host_seen(){return fg186q_host_seen.load(std::memory_order_relaxed);}
extern "C" int fg186q_cuda_device_read(uint64_t*out){
 if(!out)return cudaErrorInvalidValue;
 auto e=cudaMemcpyFromSymbol(out,fg186q_paths,sizeof(uint64_t));if(e!=cudaSuccess)return e;
 return cudaMemcpyFromSymbol(out+1,fg186q_guards,sizeof(uint64_t));
}
static void fg186q_report(){
 uint64_t x[2]={};int e=fg186q_cuda_device_read(x);
 fprintf(stderr,"FG186Q_CUDA_MMVQ_Q6K host=0x%llx,cuda_status=%d,paths=0x%llx,guards=%llu\n",
  (unsigned long long)fg186q_cuda_host_seen(),e,(unsigned long long)x[0],(unsigned long long)x[1]);
}

void fg185q_native_ggml_cuda_mul_mat_vec_q(ggml_backend_cuda_context&,const ggml_tensor*,const ggml_tensor*,
 const ggml_tensor*,ggml_tensor*,const ggml_cuda_mm_fusion_args_host*);
void ggml_cuda_mul_mat_vec_q(ggml_backend_cuda_context &ctx,const ggml_tensor*src0,const ggml_tensor*src1,
 const ggml_tensor*ids,ggml_tensor*dst,const ggml_cuda_mm_fusion_args_host*fusion){
 // EXP186 measured -3.66% decode; the owner accepted that as a recorded limitation
 // rather than a veto, and accepted improvements must be on by default. The flag now
 // only disables: GGML_GOOSE_CUDA_MMVQ_Q6K=0 returns to the native kernel.
 const char*flag=getenv("GGML_GOOSE_CUDA_MMVQ_Q6K");
 if(flag&&strcmp(flag,"0")==0){fg185q_native_ggml_cuda_mul_mat_vec_q(ctx,src0,src1,ids,dst,fusion);return;}
 static std::once_flag once;std::call_once(once,[]{std::atexit(fg186q_report);});
 fprintf(stderr,"FG186T step=1 enter ctx=%p dev=%d\n",(void*)&ctx,ctx.device);
 { ggml_cuda_pool & pr = ctx.pool(); fprintf(stderr,"FG186T step=1a pool=%p\n",(void*)&pr); }
 const int dev=ggml_cuda_get_device();
 fprintf(stderr,"FG186T step=2 dev=%d\n",dev);
 const int warp=ggml_cuda_info().devices[dev].warp_size;
 GGML_TENSOR_BINARY_OP_LOCALS;
 fprintf(stderr,"FG186T step=3 locals ne00=%lld\n",(long long)ne00);
 fg186q_packet p{};
 fprintf(stderr,"FG186T step=4 packet\n");
 p.stream=ctx.stream();
 fprintf(stderr,"FG186T step=4a stream=%p\n",(void*)p.stream);
 p.v[0][0]=(uint64_t)(uintptr_t)src0->data;
 // src1 arrives as F32 and must be quantized to q8_1 first; native does this into a
 // padded staging buffer. The quantizer is already a qualified Goose module (EXP178),
 // so reusing it here keeps the quantization itself in Goose.
 const int64_t ne10_padded=GGML_PAD(ne00,MATRIX_ROW_PADDING);
 fprintf(stderr,"FG186T step=4b pad=%lld bytes=%lld\n",(long long)ne10_padded,(long long)(ne13*ne12*ne11*ne10_padded*(int64_t)sizeof(block_q8_1)/QK8_1));
 fprintf(stderr,"FG186T step=4c pool enter dev=%d stream_no=%d\n",ctx.device,ctx.curr_stream_no);
 ggml_cuda_pool & fg186pool = ctx.pool();
 const size_t fg186sz = (size_t)(ne13*ne12*ne11*ne10_padded*sizeof(block_q8_1)/QK8_1);
 fprintf(stderr,"FG186T step=4c1 pool=%p size=%zu\n",(void*)&fg186pool,fg186sz);
 ggml_cuda_pool_alloc<char> src1_q8(fg186pool,fg186sz);
 fprintf(stderr,"FG186T step=4d alloc ptr=%p\n",(void*)src1_q8.get());
 {
  const int64_t t11=src1->nb[1]/4,t12=src1->nb[2]/4,t13=src1->nb[3]/4;
  fprintf(stderr,"FG186T step=5 quantize ne10=%lld type=%d\n",(long long)ne10,(int)src0->type);
  quantize_row_q8_1_cuda((const float*)src1->data,nullptr,src1_q8.get(),src0->type,
   ne10,t11,t12,t13,ne10_padded,ne11,ne12,ne13,ctx.stream());
 }
 p.v[0][1]=(uint64_t)(uintptr_t)src1_q8.get();
 p.v[0][2]=(uint64_t)(uintptr_t)dst->data;
 p.v[0][3]=ids?(uint64_t)(uintptr_t)ids->data:0ULL;
 p.v[1][0]=(uint64_t)ne00;
 p.v[1][1]=(uint64_t)(src0->nb[1]/ggml_type_size(src0->type));
 // native quantizes src1 into a padded q8_1 staging buffer:
 // stride_col_y = PAD(ncols, MATRIX_ROW_PADDING)/QK8_1.
 p.v[1][2]=(uint64_t)(ne10_padded/QK8_1);
 p.v[1][3]=(uint64_t)(ids?dst->nb[2]/4:dst->nb[1]/4);
 p.v[2][0]=(uint64_t)(src0->nb[2]/ggml_type_size(src0->type));
 // native derives the y channel stride from the padded q8_1 staging layout:
 // s11 = ne10_padded/QK8_1, s12 = ne11*s11 -- in BLOCKS, not elements.
 const int64_t s11q=ne10_padded/QK8_1;
 p.v[2][1]=(uint64_t)(ids?s11q:ne11*s11q);
 p.v[2][2]=(uint64_t)(ids?dst->nb[1]/4:dst->nb[2]/4);
 p.v[3][0]=(uint64_t)(src0->nb[3]/ggml_type_size(src0->type));
 p.v[3][1]=(uint64_t)(ne12*ne11*s11q);
 p.v[3][2]=(uint64_t)(dst->nb[3]/4);
 p.v[4][0]=(uint64_t)ne01;
 p.v[4][1]=(uint64_t)(ids?ne1:ne2);
 p.v[4][2]=(uint64_t)(ids?ne2:ne3);
 p.v[4][3]=(uint64_t)(ids?ne11:ne12);
 p.v[5][0]=(uint64_t)warp;
 p.v[5][1]=(uint64_t)4;   // nwarps for Q6_K, ncols_dst=1 on the SM86+ table
 p.v[5][2]=ids?1ULL:0ULL;
 p.v[5][3]=(uint64_t)(ids?ids->nb[1]/4:0);
 p.v[5][4]=(uint64_t)(ids?0:(ne2/src0->ne[2]));
 p.v[5][5]=(uint64_t)(src0->ne[3]?(dst->ne[3]/src0->ne[3]):1);
 p.v[6][1]=(uint64_t)(ids?ne2:ne1);
 p.v[6][2]=(src0->type==GGML_TYPE_Q6_K)?1ULL:0ULL;
 const bool eligible = src0->type==GGML_TYPE_Q6_K && src1->type==GGML_TYPE_F32 &&
  dst->type==GGML_TYPE_F32 && (!ids||ids->type==GGML_TYPE_I32) &&
  (!ids||dst->ne[2]==1) && (ids||dst->ne[1]==1) &&
  (fusion==nullptr||(fusion->gate==nullptr&&fusion->x_bias==nullptr&&fusion->gate_bias==nullptr&&
                     fusion->x_scale==nullptr&&fusion->gate_scale==nullptr)) &&
  ne13==ne3;
 p.v[6][0]=eligible?1ULL:0ULL;
 fprintf(stderr,"FG186T step=6 run eligible=%d\n",(int)eligible);
 if(fg186qh_run((int64_t)(uintptr_t)&p)&&eligible){
  fg186q_host_seen.fetch_or(p.launched?1ULL:16ULL,std::memory_order_relaxed);
  return;
 }
 fg186q_host_seen.fetch_or(32ULL,std::memory_order_relaxed);
 fprintf(stderr,"FG186T step=7 fallback native\n");
 fg185q_native_ggml_cuda_mul_mat_vec_q(ctx,src0,src1,ids,dst,fusion);
 fprintf(stderr,"FG186T step=8 done\n");
}
