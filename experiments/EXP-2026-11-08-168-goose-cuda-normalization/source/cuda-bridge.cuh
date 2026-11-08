#include "host-ffi.h"
#include <atomic>
#include <cinttypes>
#include <cstdlib>
#include <type_traits>
extern "C" void fg168_host_entry(int64_t);
struct fg168_packet{int64_t args[5];fg168_plan plan;};
static_assert(sizeof(float)==4&&sizeof(uint64_t)==8&&std::is_trivially_copyable<fg168_plan>::value,"Normalization POD ABI");
static std::atomic<uint64_t>fg168_seen{0};static thread_local uint64_t fg168_thread_seen;
static bool fg168_enabled(int mode){static const bool enabled=[](){const char*v=getenv("GGML_GOOSE_CUDA_NORM");return!(v&&v[0]=='0'&&v[1]==0);}();static const bool rms=[](){const char*v=getenv("GGML_GOOSE_RMS");return!(v&&v[0]=='0'&&v[1]==0);}();return enabled&&(mode<2||mode>5||rms);}
static void fg168_report();
static void fg168_run(int mode,ggml_backend_cuda_context&ctx,ggml_tensor*dst,ggml_tensor*second=nullptr,ggml_tensor*third=nullptr){static const bool registered=[](){std::atexit(fg168_report);return true;}();(void)registered;uint64_t bit=UINT64_C(1)<<mode;if(!(fg168_thread_seen&bit)){fg168_thread_seen|=bit;fg168_seen.fetch_or(bit,std::memory_order_relaxed);}fg168_packet p{};p.args[0]=mode;p.args[1]=(int64_t)(uintptr_t)&ctx;p.args[2]=(int64_t)(uintptr_t)dst;p.args[3]=(int64_t)(uintptr_t)second;p.args[4]=(int64_t)(uintptr_t)third;fg168_host_entry((int64_t)(uintptr_t)&p);}
int64_t fg168_arg(int64_t p,int64_t k){GGML_ASSERT(k>=0&&k<5);return((const fg168_packet*)(uintptr_t)p)->args[k];}
int64_t fg168_plan_ptr(int64_t p){return(int64_t)(uintptr_t)&((fg168_packet*)(uintptr_t)p)->plan;}
uint64_t fg168_hget(int64_t p,int64_t k,int64_t i){return fg168_project((const fg168_plan*)(uintptr_t)p,k,i);}
void fg168_hput(int64_t pp,int64_t k,int64_t i,uint64_t v){auto*p=(fg168_plan*)(uintptr_t)pp;switch(k){case 0:GGML_ASSERT(i>=0&&i<5);p->ptrs[i]=v;return;case 1:GGML_ASSERT(i>=0&&i<20);p->dims[i/4][i%4]=v;return;case 2:GGML_ASSERT(i>=0&&i<20);p->strides[i/4][i%4]=v;return;case 3:GGML_ASSERT(i>=0&&i<24);p->iv[i]=v;return;case 4:GGML_ASSERT(i>=0&&i<24&&v<=UINT32_MAX);p->magic[i/3][i%3]=(uint32_t)v;return;default:GGML_ABORT("Invalid normalization field write");}}
void fg168_hfloat(int64_t p,int64_t i,float v){GGML_ASSERT(i>=0&&i<8);((fg168_plan*)(uintptr_t)p)->fv[i]=v;}
int64_t fg168_ti(int64_t p,int64_t k,int64_t i){GGML_ASSERT(p);const auto*t=(const ggml_tensor*)(uintptr_t)p;switch(k){case 0:GGML_ASSERT(i>=0&&i<GGML_MAX_SRC);return(int64_t)(uintptr_t)t->src[i];case 1:return(int64_t)(uintptr_t)t->data;case 2:return t->type;case 3:GGML_ASSERT(i>=0&&i<4);return t->ne[i];case 4:GGML_ASSERT(i>=0&&i<4);return(int64_t)t->nb[i];case 8:{GGML_ASSERT(i>=0&&i<GGML_MAX_OP_PARAMS/4);int32_t v;memcpy(&v,(const char*)t->op_params+i*4,4);return v;}case 9:return(int64_t)ggml_nbytes(t);default:GGML_ABORT("Invalid normalization tensor field");}}
float fg168_tf(int64_t p,int64_t i){GGML_ASSERT(p&&i>=0&&i<GGML_MAX_OP_PARAMS/4);float v;memcpy(&v,(const char*)((const ggml_tensor*)(uintptr_t)p)->op_params+i*4,4);return v;}
uint8_t fg168_contiguous(int64_t p){return ggml_is_contiguous((const ggml_tensor*)(uintptr_t)p);}
int64_t fg168_f32_type(){return GGML_TYPE_F32;}
void fg168_fail(int64_t k){GGML_ABORT("Goose CUDA normalization validation failed:code=%" PRId64,k);}
static __device__ unsigned long long fg168_modes,fg168_vectors,fg168_fusions;
template<int Mode,int BS,int VPT,bool Vec>static __global__ void fg168_kernel(fg168_plan p){extern __shared__ float shared[];if(threadIdx.x==0&&blockIdx.x==0&&blockIdx.y==0&&blockIdx.z==0){atomicOr(&fg168_modes,1ULL<<Mode);atomicOr(&fg168_vectors,Vec?2ULL:1ULL);atomicOr(&fg168_fusions,(Mode==3||Mode==5?1ULL:0ULL)|(Mode==5?2ULL:0ULL)|(Mode==4?4ULL:0ULL));}FG168_DEVICE_LANE((int64_t)(uintptr_t)&p,Mode,BS,VPT,Vec,Mode==3||Mode==5,Mode==5,Mode==4,(int32_t)threadIdx.x,(uint64_t)blockIdx.x,(uint64_t)blockIdx.y,(uint64_t)blockIdx.z,(uint64_t)gridDim.y,(uint64_t)gridDim.z,(uint64_t)(uintptr_t)shared,0);}
template<int Mode,int BS,int VPT,bool Vec>static void fg168_typed(fg168_packet*p){const auto&q=p->plan;auto&ctx=*(ggml_backend_cuda_context*)(uintptr_t)p->args[1];dim3 grid(q.iv[11],q.iv[12],q.iv[13]),threads(BS);cudaStream_t stream=ctx.stream();if constexpr(Mode>=2&&Mode<=5){ggml_cuda_kernel_launch_params cfg(grid,threads,q.iv[14],stream);ggml_cuda_kernel_launch(fg168_kernel<Mode,BS,VPT,Vec>,cfg,q);}else{fg168_kernel<Mode,BS,VPT,Vec><<<grid,threads,q.iv[14],stream>>>(q);CUDA_CHECK(cudaGetLastError());}}
template<int Mode>static void fg168_select(fg168_packet*p){const auto&q=p->plan;if constexpr(Mode>=2&&Mode<=5){if(q.iv[7]){switch(q.iv[5]){case 32:fg168_typed<Mode,32,1,true>(p);return;case 128:fg168_typed<Mode,128,1,true>(p);return;case 256:fg168_typed<Mode,256,1,true>(p);return;case 512:fg168_typed<Mode,512,1,true>(p);return;case 1024:if(q.iv[6]==4)fg168_typed<Mode,1024,4,true>(p);else fg168_typed<Mode,1024,1,true>(p);return;default:GGML_ABORT("Invalid normalization vector block");}}if(q.iv[5]==256){fg168_typed<Mode,256,1,false>(p);return;}}else if(q.iv[5]==32){fg168_typed<Mode,32,1,false>(p);return;}GGML_ASSERT(q.iv[5]==1024);fg168_typed<Mode,1024,1,false>(p);}
void fg168_launch(int64_t pp){auto*p=(fg168_packet*)(uintptr_t)pp;switch(p->plan.iv[0]){
case 0:fg168_select<0>(p);return;
case 1:fg168_select<1>(p);return;
case 2:fg168_select<2>(p);return;
case 3:fg168_select<3>(p);return;
case 4:fg168_select<4>(p);return;
case 5:fg168_select<5>(p);return;
case 6:fg168_select<6>(p);return;
case 7:fg168_select<7>(p);return;
default:GGML_ABORT("Invalid normalization mode");}}
extern "C" __attribute__((visibility("default"))) uint64_t fg168_cuda_host_seen(){return fg168_seen.load();}
extern "C" __attribute__((visibility("default"))) int fg168_cuda_device_read(uint64_t*out){auto st=cudaMemcpyFromSymbol(out,fg168_modes,sizeof(fg168_modes));if(st==cudaSuccess)st=cudaMemcpyFromSymbol(out+1,fg168_vectors,sizeof(fg168_vectors));if(st==cudaSuccess)st=cudaMemcpyFromSymbol(out+2,fg168_fusions,sizeof(fg168_fusions));return(int)st;}
static void fg168_report(){uint64_t host=fg168_seen.load();if(!host)return;uint64_t out[3]={};int status=fg168_cuda_device_read(out);fprintf(stderr,"FG168_CUDA_NORM host=0x%" PRIx64 ",cuda_status=%d,modes=0x%" PRIx64 ",vectors=0x%" PRIx64 ",fusions=0x%" PRIx64 "\n",host,status,out[0],out[1],out[2]);}
void ggml_cuda_op_norm(ggml_backend_cuda_context&ctx,ggml_tensor*dst){if(!fg168_enabled(0)){fg168_native_ggml_cuda_op_norm(ctx,dst);return;}fg168_run(0,ctx,dst);}
void ggml_cuda_op_group_norm(ggml_backend_cuda_context&ctx,ggml_tensor*dst){if(!fg168_enabled(1)){fg168_native_ggml_cuda_op_group_norm(ctx,dst);return;}fg168_run(1,ctx,dst);}
void ggml_cuda_op_rms_norm(ggml_backend_cuda_context&ctx,ggml_tensor*dst){if(!fg168_enabled(2)){fg168_native_ggml_cuda_op_rms_norm(ctx,dst);return;}fg168_run(2,ctx,dst);}
void ggml_cuda_op_rms_norm_fused(ggml_backend_cuda_context&ctx,ggml_tensor*dst,ggml_tensor*second){if(!fg168_enabled(3)){fg168_native_ggml_cuda_op_rms_norm_fused(ctx,dst,second);return;}fg168_run(3,ctx,dst,second);}
void ggml_cuda_op_rms_norm_scale_fused(ggml_backend_cuda_context&ctx,ggml_tensor*dst,ggml_tensor*second){if(!fg168_enabled(4)){fg168_native_ggml_cuda_op_rms_norm_scale_fused(ctx,dst,second);return;}fg168_run(4,ctx,dst,second);}
void ggml_cuda_op_rms_norm_fused_add(ggml_backend_cuda_context&ctx,ggml_tensor*dst,ggml_tensor*second,ggml_tensor*third){if(!fg168_enabled(5)){fg168_native_ggml_cuda_op_rms_norm_fused_add(ctx,dst,second,third);return;}fg168_run(5,ctx,dst,second,third);}
void ggml_cuda_op_rms_norm_back(ggml_backend_cuda_context&ctx,ggml_tensor*dst){if(!fg168_enabled(6)){fg168_native_ggml_cuda_op_rms_norm_back(ctx,dst);return;}fg168_run(6,ctx,dst);}
void ggml_cuda_op_l2_norm(ggml_backend_cuda_context&ctx,ggml_tensor*dst){if(!fg168_enabled(7)){fg168_native_ggml_cuda_op_l2_norm(ctx,dst);return;}fg168_run(7,ctx,dst);}
