#include "host-ffi.h"
#include <atomic>
#include <cinttypes>
#include <cstdlib>
extern "C" void fg170s_host_entry(int64_t);
struct fg170s_packet{int64_t args[5];fg170s_plan plan;};
static std::atomic<uint64_t>fg170s_seen{0};static thread_local uint64_t fg170s_thread_seen;
static bool fg170s_enabled(){static const bool on=[](){const char*v=getenv("GGML_GOOSE_CUDA_SSM");return !(v&&v[0]=='0'&&v[1]==0);}();return on;}
static void fg170s_report();
int64_t fg170s_arg(int64_t p,int64_t k){GGML_ASSERT(k>=0&&k<5);return((const fg170s_packet*)(uintptr_t)p)->args[k];}
int64_t fg170s_plan_ptr(int64_t p){return(int64_t)(uintptr_t)&((fg170s_packet*)(uintptr_t)p)->plan;}
uint64_t fg170s_hget(int64_t p,int64_t k,int64_t i){return fg170s_project((const fg170s_plan*)(uintptr_t)p,k,i);}
void fg170s_hput(int64_t p,int64_t k,int64_t i,uint64_t v){auto*q=(fg170s_plan*)(uintptr_t)p;switch(k){case 0:GGML_ASSERT(i>=0&&i<4);q->ptrs[i]=v;return;case 1:GGML_ASSERT(i>=0&&i<16);q->dims[i/4][i%4]=v;return;case 2:GGML_ASSERT(i>=0&&i<16);q->strides[i/4][i%4]=v;return;case 3:GGML_ASSERT(i>=0&&i<16);q->iv[i]=v;return;default:GGML_ABORT("Invalid SSM plan write");}}
int64_t fg170s_ti(int64_t p,int64_t k,int64_t i){GGML_ASSERT(p);const auto*t=(const ggml_tensor*)(uintptr_t)p;switch(k){case 0:GGML_ASSERT(i>=0&&i<GGML_MAX_SRC);return(int64_t)(uintptr_t)t->src[i];case 1:return(int64_t)(uintptr_t)t->data;case 2:return t->type;case 3:GGML_ASSERT(i>=0&&i<4);return t->ne[i];case 4:GGML_ASSERT(i>=0&&i<4);return(int64_t)t->nb[i];case 9:return(int64_t)ggml_nbytes(t);default:GGML_ABORT("Invalid SSM tensor read");}}
uint8_t fg170s_contiguous(int64_t t){return ggml_is_contiguous((const ggml_tensor*)(uintptr_t)t);}
int64_t fg170s_f32_type(){return GGML_TYPE_F32;}
void fg170s_fail(int64_t c){GGML_ABORT("Goose SSM validation failed:code=%" PRId64,c);}
static __device__ unsigned long long fg170s_modes,fg170s_sizes;
template<int NC,bool SiLU,bool Long>static __global__ void fg170s_kernel(fg170s_plan p){
 extern __shared__ float smem[];
 if(blockIdx.x==0&&blockIdx.y==0&&blockIdx.z==0&&threadIdx.x==0){atomicOr(&fg170s_modes,(Long?2ULL:1ULL)|(SiLU?4ULL:0ULL)|(p.iv[1]?8ULL:0ULL));constexpr int bit=NC==3?0:NC==4?1:NC==5?2:NC==7?3:NC==9?4:5;atomicOr(&fg170s_sizes,1ULL<<bit);}
 if constexpr(Long){FG170S_LONG_DISPATCH;}
 else{FG170S_SERIAL_DISPATCH;}
}
template<int NC,bool SiLU>static void fg170s_dispatch(fg170s_packet*p){auto&ctx=*(ggml_backend_cuda_context*)(uintptr_t)p->args[1];const auto&q=p->plan;dim3 grid(q.iv[8],q.iv[9],q.iv[6]?q.iv[10]:1);if(q.iv[6]){fg170s_kernel<NC,SiLU,true><<<grid,128,q.iv[11],ctx.stream()>>>(q);}else{ggml_cuda_kernel_launch_params lp(grid,128,0,ctx.stream());ggml_cuda_kernel_launch(fg170s_kernel<NC,SiLU,false>,lp,q);}}
template<bool S>static void fg170s_size(fg170s_packet*p){switch(p->plan.iv[2]){case 3:fg170s_dispatch<3,S>(p);return;case 4:fg170s_dispatch<4,S>(p);return;case 5:fg170s_dispatch<5,S>(p);return;case 7:fg170s_dispatch<7,S>(p);return;case 9:fg170s_dispatch<9,S>(p);return;case 15:fg170s_dispatch<15,S>(p);return;default:GGML_ABORT("Invalid SSM convolution size");}}
void fg170s_launch(int64_t p){auto*q=(fg170s_packet*)(uintptr_t)p;if(q->plan.iv[0])fg170s_size<true>(q);else fg170s_size<false>(q);}
extern "C" __attribute__((visibility("default"))) uint64_t fg170s_cuda_host_seen(){return fg170s_seen.load();}
extern "C" __attribute__((visibility("default"))) int fg170s_cuda_device_read(uint64_t*out){auto st=cudaMemcpyFromSymbol(out,fg170s_modes,sizeof(fg170s_modes));if(st==cudaSuccess)st=cudaMemcpyFromSymbol(out+1,fg170s_sizes,sizeof(fg170s_sizes));return(int)st;}
static void fg170s_report(){uint64_t host=fg170s_seen.load();if(!host)return;uint64_t out[2]={};int st=fg170s_cuda_device_read(out);fprintf(stderr,"FG170S_CUDA_SSM host=0x%" PRIx64 ",cuda_status=%d,modes=0x%" PRIx64 ",sizes=0x%" PRIx64 "\n",host,st,out[0],out[1]);}
void ggml_cuda_op_ssm_conv(ggml_backend_cuda_context&ctx,ggml_tensor*dst,ggml_tensor*bias,ggml_tensor*silu){if(!fg170s_enabled()){fg170s_native_ggml_cuda_op_ssm_conv(ctx,dst,bias,silu);return;}static const bool registered=[](){std::atexit(fg170s_report);return true;}();(void)registered;const uint64_t bit=silu?2ULL:1ULL;if(!(fg170s_thread_seen&bit)){fg170s_thread_seen|=bit;fg170s_seen.fetch_or(bit,std::memory_order_relaxed);}fg170s_packet p{};p.args[0]=silu?1:0;p.args[1]=(int64_t)(uintptr_t)&ctx;p.args[2]=(int64_t)(uintptr_t)dst;p.args[3]=(int64_t)(uintptr_t)bias;p.args[4]=(int64_t)(uintptr_t)silu;fg170s_host_entry((int64_t)(uintptr_t)&p);}
