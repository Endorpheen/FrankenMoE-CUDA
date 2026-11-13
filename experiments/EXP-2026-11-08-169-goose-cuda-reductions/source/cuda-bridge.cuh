#include "host-ffi.h"
#include <atomic>
#include <cinttypes>
#include <cstdlib>
#include <type_traits>
extern "C" void fg169_host_entry(int64_t);
struct fg169_packet{int64_t args[9];fg169_plan plan;size_t cub_bytes=0,actual_bytes=0;};
static_assert(sizeof(float)==4&&sizeof(int32_t)==4&&std::is_trivially_copyable<fg169_plan>::value,"Reduction packet ABI");
static std::atomic<uint64_t>fg169_seen{0},fg169_cub_seen{0},fg169_live_temp{0};static thread_local uint64_t fg169_thread_seen;
static bool fg169_enabled(){static const bool value=[](){const char*v=getenv("GGML_GOOSE_CUDA_REDUCTIONS");return!(v&&v[0]=='0'&&v[1]==0);}();return value;}
static void fg169_report();
static void fg169_run(int mode,ggml_backend_cuda_context*ctx,ggml_tensor*d,const float*x=nullptr,void*out=nullptr,int64_t cols=0,int64_t rows=0,cudaStream_t stream=nullptr,ggml_cuda_pool*pool=nullptr){static const bool registered=[](){std::atexit(fg169_report);return true;}();(void)registered;const uint64_t bit=UINT64_C(1)<<mode;if(!(fg169_thread_seen&bit)){fg169_thread_seen|=bit;fg169_seen.fetch_or(bit,std::memory_order_relaxed);}fg169_packet p{};p.args[0]=mode;p.args[1]=(int64_t)(uintptr_t)ctx;p.args[2]=(int64_t)(uintptr_t)d;p.args[3]=(int64_t)(uintptr_t)x;p.args[4]=(int64_t)(uintptr_t)out;p.args[5]=cols;p.args[6]=rows;p.args[7]=(int64_t)(uintptr_t)stream;p.args[8]=(int64_t)(uintptr_t)pool;fg169_host_entry((int64_t)(uintptr_t)&p);}
int64_t fg169_arg(int64_t p,int64_t k){GGML_ASSERT(k>=0&&k<9);return((const fg169_packet*)(uintptr_t)p)->args[k];}
int64_t fg169_plan_ptr(int64_t p){return(int64_t)(uintptr_t)&((fg169_packet*)(uintptr_t)p)->plan;}
uint64_t fg169_hget(int64_t p,int64_t k,int64_t i){return fg169_project((const fg169_plan*)(uintptr_t)p,k,i);}
void fg169_hput(int64_t pp,int64_t k,int64_t i,uint64_t v){auto*p=(fg169_plan*)(uintptr_t)pp;switch(k){case 0:GGML_ASSERT(i>=0&&i<3);p->ptrs[i]=v;return;case 1:GGML_ASSERT(i>=0&&i<8);p->dims[i/4][i%4]=v;return;case 2:GGML_ASSERT(i>=0&&i<8);p->strides[i/4][i%4]=v;return;case 3:GGML_ASSERT(i>=0&&i<16);p->iv[i]=v;return;default:GGML_ABORT("Invalid reduction field write");}}
int64_t fg169_ti(int64_t p,int64_t k,int64_t i){GGML_ASSERT(p);const auto*t=(const ggml_tensor*)(uintptr_t)p;switch(k){case 0:GGML_ASSERT(i>=0&&i<GGML_MAX_SRC);return(int64_t)(uintptr_t)t->src[i];case 1:return(int64_t)(uintptr_t)t->data;case 2:return t->type;case 3:GGML_ASSERT(i>=0&&i<4);return t->ne[i];case 4:GGML_ASSERT(i>=0&&i<4);return(int64_t)t->nb[i];case 9:return(int64_t)ggml_nbytes(t);default:GGML_ABORT("Invalid reduction tensor field");}}
uint8_t fg169_pred(int64_t k,int64_t t){const auto*x=(const ggml_tensor*)(uintptr_t)t;switch(k){case 0:return ggml_is_contiguous(x);case 1:return ggml_is_contiguous_rows(x);case 2:return ggml_is_contiguously_allocated(x);default:GGML_ABORT("Invalid reduction tensor predicate");}}
int64_t fg169_constant(int64_t pp,int64_t k){auto*p=(fg169_packet*)(uintptr_t)pp;switch(k){case 0:return GGML_TYPE_F32;case 1:return GGML_TYPE_I32;case 2:return ggml_cuda_info().devices[ggml_cuda_get_device()].nsm;case 3:
#ifdef GGML_CUDA_USE_CUB
 return 1;
#else
 return 0;
#endif
case 4:
#ifdef USE_CUDA_GRAPH
 return 1;
#else
 return 0;
#endif
#ifdef USE_CUDA_GRAPH
case 5:return((ggml_backend_cuda_context*)(uintptr_t)p->args[1])->any_cuda_graph_has_instance();
case 6:{cudaStreamCaptureStatus status;auto&ctx=*(ggml_backend_cuda_context*)(uintptr_t)p->args[1];CUDA_CHECK(cudaStreamIsCapturing(ctx.stream(),&status));return status!=cudaStreamCaptureStatusNone;}
case 7:return((ggml_backend_cuda_context*)(uintptr_t)p->args[1])->any_cuda_graph_enabled();
#else
case 5:case 6:case 7:return 0;
#endif
default:GGML_ABORT("Invalid reduction constant");}}
static cudaStream_t fg169_stream(fg169_packet*p){if(p->args[1])return((ggml_backend_cuda_context*)(uintptr_t)p->args[1])->stream();return(cudaStream_t)(uintptr_t)p->args[7];}
static ggml_cuda_pool&fg169_pool(fg169_packet*p){if(p->args[1])return((ggml_backend_cuda_context*)(uintptr_t)p->args[1])->pool();GGML_ASSERT(p->args[8]);return*(ggml_cuda_pool*)(uintptr_t)p->args[8];}
uint64_t fg169_cub_query(int64_t pp){auto*p=(fg169_packet*)(uintptr_t)pp;
#ifdef GGML_CUDA_USE_CUB
 p->cub_bytes=0;CUDA_CHECK(cub::DeviceReduce::Sum(nullptr,p->cub_bytes,(const float*)(uintptr_t)p->plan.ptrs[0],(float*)(uintptr_t)p->plan.ptrs[1],(int64_t)p->plan.iv[1],fg169_stream(p)));return p->cub_bytes;
#else
 GGML_ABORT("CUB primitive unavailable");
#endif
}
uint64_t fg169_pool_alloc(int64_t pp,uint64_t bytes){auto*p=(fg169_packet*)(uintptr_t)pp;void*temp=fg169_pool(p).alloc(bytes,&p->actual_bytes);p->plan.iv[8]=p->actual_bytes;if(temp)fg169_live_temp.fetch_add(1,std::memory_order_relaxed);return(uint64_t)(uintptr_t)temp;}
void fg169_cub_run(int64_t pp,uint64_t tmp){auto*p=(fg169_packet*)(uintptr_t)pp;
#ifdef GGML_CUDA_USE_CUB
 CUDA_CHECK(cub::DeviceReduce::Sum((void*)(uintptr_t)tmp,p->cub_bytes,(const float*)(uintptr_t)p->plan.ptrs[0],(float*)(uintptr_t)p->plan.ptrs[1],(int64_t)p->plan.iv[1],fg169_stream(p)));fg169_cub_seen.fetch_or(UINT64_C(1)<<p->plan.iv[0],std::memory_order_relaxed);
#else
 GGML_ABORT("CUB primitive unavailable");
#endif
}
void fg169_pool_free(int64_t pp,uint64_t tmp){auto*p=(fg169_packet*)(uintptr_t)pp;fg169_pool(p).free((void*)(uintptr_t)tmp,p->actual_bytes);fg169_live_temp.fetch_sub(1,std::memory_order_relaxed);p->actual_bytes=0;}
void fg169_fail(int64_t k){GGML_ABORT("Goose CUDA reduction validation failed:code=%" PRId64,k);}
static __device__ unsigned long long fg169_modes;
template<int Mode,bool Strided>static __global__ void fg169_rows(fg169_plan p){extern __shared__ float fg169_rows_shared[];if(threadIdx.x==0&&blockIdx.x==0)atomicOr(&fg169_modes,1ULL<<Mode);const int64_t row=blockIdx.x;const int32_t tid=threadIdx.x,bs=blockDim.x;uint64_t shared=(uint64_t)(uintptr_t)fg169_rows_shared;if constexpr(Strided){if constexpr(Mode==1)FG169_ROW_MEAN_STRIDED(p.ptrs[0],p.ptrs[1],p.iv[1],p.dims[0][1],p.dims[0][2],p.strides[0][1],p.strides[0][2],p.strides[0][3],row,tid,bs,shared,0);else FG169_ROW_SUM_STRIDED(p.ptrs[0],p.ptrs[1],p.iv[1],p.dims[0][1],p.dims[0][2],p.strides[0][1],p.strides[0][2],p.strides[0][3],row,tid,bs,shared,0);}else{if constexpr(Mode==1)FG169_ROW_MEAN(p.ptrs[0],p.ptrs[1],p.iv[1],row,tid,bs,shared,0);else FG169_ROW_SUM(p.ptrs[0],p.ptrs[1],p.iv[1],row,tid,bs,shared,0);}}
static __global__ void fg169_argmax(fg169_plan p){if(threadIdx.x==0&&blockIdx.x==0)atomicOr(&fg169_modes,1ULL<<2);FG169_ARGMAX(p.ptrs[0],p.ptrs[1],p.iv[1],(int64_t)blockIdx.x,(int32_t)threadIdx.x,(int32_t)blockDim.x,0);}
static __global__ void fg169_finish_mean(fg169_plan p){if(threadIdx.x==0)atomicOr(&fg169_modes,1ULL<<8);FG169_FINISH_MEAN(p.ptrs[1],p.iv[1],(int32_t)threadIdx.x,0);}
template<int Mode>static void fg169_launch_rows(fg169_packet*p){const auto&q=p->plan;ggml_cuda_kernel_launch_params cfg((dim3)q.iv[4],(dim3)q.iv[3],q.iv[5],fg169_stream(p));if(q.iv[6])ggml_cuda_kernel_launch(fg169_rows<Mode,true>,cfg,q);else ggml_cuda_kernel_launch(fg169_rows<Mode,false>,cfg,q);}
void fg169_launch(int64_t pp){auto*p=(fg169_packet*)(uintptr_t)pp;const auto&q=p->plan;if(q.iv[7]==2){fg169_finish_mean<<<1,32,0,fg169_stream(p)>>>(q);CUDA_CHECK(cudaGetLastError());return;}switch(q.iv[0]){case 0:fg169_launch_rows<0>(p);return;case 1:fg169_launch_rows<1>(p);return;case 2:fg169_argmax<<<q.iv[4],q.iv[3],q.iv[5],fg169_stream(p)>>>(q);CUDA_CHECK(cudaGetLastError());return;case 3:fg169_launch_rows<3>(p);return;case 4:fg169_launch_rows<4>(p);return;case 5:fg169_launch_rows<5>(p);return;default:GGML_ABORT("Invalid reduction launch mode");}}
extern "C" __attribute__((visibility("default"))) uint64_t fg169_cuda_host_seen(){return fg169_seen.load();}
extern "C" __attribute__((visibility("default"))) uint64_t fg169_cuda_cub_seen(){return fg169_cub_seen.load();}
extern "C" __attribute__((visibility("default"))) uint64_t fg169_cuda_live_temp(){return fg169_live_temp.load();}
extern "C" __attribute__((visibility("default"))) int fg169_cuda_device_read(uint64_t*out){return(int)cudaMemcpyFromSymbol(out,fg169_modes,sizeof(fg169_modes));}
static void fg169_report(){uint64_t host=fg169_seen.load();if(!host)return;uint64_t out=0;int status=fg169_cuda_device_read(&out);fprintf(stderr,"FG169_CUDA_REDUCTIONS host=0x%" PRIx64 ",cuda_status=%d,modes=0x%" PRIx64 ",cub=0x%" PRIx64 ",live_temp=%" PRIu64 "\n",host,status,out,fg169_cub_seen.load(),fg169_live_temp.load());}
void ggml_cuda_op_sum_rows(ggml_backend_cuda_context&ctx,ggml_tensor*d){if(!fg169_enabled()){fg169_native_ggml_cuda_op_sum_rows(ctx,d);return;}fg169_run(0,&ctx,d);}
void ggml_cuda_op_mean(ggml_backend_cuda_context&ctx,ggml_tensor*d){if(!fg169_enabled()){fg169_native_ggml_cuda_op_mean(ctx,d);return;}fg169_run(1,&ctx,d);}
void ggml_cuda_argmax(ggml_backend_cuda_context&ctx,ggml_tensor*d){if(!fg169_enabled()){fg169_native_ggml_cuda_argmax(ctx,d);return;}fg169_run(2,&ctx,d);}
void sum_rows_f32_cuda(const float*x,float*d,int cols,int rows,cudaStream_t stream){if(!fg169_enabled()){fg169_native_sum_rows_f32_cuda(x,d,cols,rows,stream);return;}fg169_run(3,nullptr,nullptr,x,d,cols,rows,stream);}
void ggml_cuda_op_sum(ggml_backend_cuda_context&ctx,ggml_tensor*d){if(!fg169_enabled()){fg169_native_ggml_cuda_op_sum(ctx,d);return;}fg169_run(4,&ctx,d);}
void sum_f32_cuda(ggml_cuda_pool&pool,const float*x,float*d,int64_t ne,cudaStream_t stream){if(!fg169_enabled()){fg169_native_sum_f32_cuda(pool,x,d,ne,stream);return;}fg169_run(5,nullptr,nullptr,x,d,ne,1,stream,&pool);}
