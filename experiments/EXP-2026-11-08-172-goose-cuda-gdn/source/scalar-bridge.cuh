#include "host-ffi.h"
#include <atomic>
#include <cstdlib>
#include <cinttypes>
extern "C" void fg174s_host_entry(int64_t);
static std::atomic<uint64_t>fg174s_seen{0};static thread_local uint64_t fg174s_thread_seen;
static bool fg174s_enabled(){static const bool on=[](){const char*v=getenv("GGML_GOOSE_CUDA_GDN");return !(v&&v[0]=='0'&&v[1]==0);}();return on;}
static void fg174s_report();
int64_t fg174s_arg(int64_t p,int64_t i){GGML_ASSERT(i>=0&&i<5);return((const fg174s_packet*)(uintptr_t)p)->args[i];}
int64_t fg174s_plan_ptr(int64_t p){return(int64_t)(uintptr_t)&((fg174s_packet*)(uintptr_t)p)->plan;}
uint64_t fg174s_hget(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<=4&&i>=0&&i<(k==0?8:k==3?24:k==4?6:32));return fg174s_project((const fg174s_plan*)(uintptr_t)p,k,i);}
void fg174s_hput(int64_t p,int64_t k,int64_t i,uint64_t v){auto*q=(fg174s_plan*)(uintptr_t)p;switch(k){case 0:GGML_ASSERT(i>=0&&i<8);q->ptrs[i]=v;return;case 1:GGML_ASSERT(i>=0&&i<32);q->dims[i/4][i%4]=v;return;case 2:GGML_ASSERT(i>=0&&i<32);q->strides[i/4][i%4]=v;return;case 3:GGML_ASSERT(i>=0&&i<24);q->iv[i]=v;return;case 4:GGML_ASSERT(i>=0&&i<6&&v<=UINT32_MAX);q->magic[i/3][i%3]=v;return;default:GGML_ABORT("Invalid GDN plan write");}}
void fg174s_hfloat(int64_t p,int64_t i,float f){GGML_ASSERT(i>=0&&i<8);((fg174s_plan*)(uintptr_t)p)->fv[i]=f;}
int64_t fg174s_ti(int64_t p,int64_t k,int64_t i){GGML_ASSERT(p);const auto*t=(const ggml_tensor*)(uintptr_t)p;switch(k){case 0:GGML_ASSERT(i>=0&&i<GGML_MAX_SRC);return(int64_t)(uintptr_t)t->src[i];case 1:return(int64_t)(uintptr_t)t->data;case 2:return t->type;case 3:GGML_ASSERT(i>=0&&i<4);return t->ne[i];case 4:GGML_ASSERT(i>=0&&i<4);return(int64_t)t->nb[i];case 8:{GGML_ASSERT(i>=0&&i<GGML_MAX_OP_PARAMS/4);int32_t v;memcpy(&v,(const char*)t->op_params+4*i,4);return v;}case 9:return(int64_t)ggml_nbytes(t);default:GGML_ABORT("Invalid GDN tensor projection");}}
uint8_t fg174s_contiguous(int64_t t){return ggml_is_contiguous((const ggml_tensor*)(uintptr_t)t);}
int64_t fg174s_f32_type(){return GGML_TYPE_F32;}
int64_t fg174s_warp_size(){return ggml_cuda_info().devices[ggml_cuda_get_device()].warp_size;}
int64_t fg174s_nsm(){return ggml_cuda_info().devices[ggml_cuda_get_device()].nsm;}
float fg174s_sqrt(float f){return sqrtf(f);}
void fg174s_fail(int64_t c){GGML_ABORT("Goose GDN validation failed:code=%" PRId64,c);}
static __device__ unsigned long long fg174s_modes,fg174s_sizes;
template<int SV,bool KDA,bool Keep>static __global__ __launch_bounds__((ggml_cuda_get_physical_warp_size()<SV?ggml_cuda_get_physical_warp_size():SV)*4,2) void fg174s_kernel(fg174s_plan p){p.iv[0]=SV;p.iv[13]=ggml_cuda_get_physical_warp_size();p.iv[16]=KDA;p.iv[7]=Keep;
 if(blockIdx.x==0&&blockIdx.y==0&&blockIdx.z==0&&threadIdx.x==0&&threadIdx.y==0){atomicOr(&fg174s_modes,1ULL|(KDA?2ULL:0ULL)|(Keep?4ULL:0ULL));atomicOr(&fg174s_sizes,SV==16?1ULL:SV==32?2ULL:SV==64?4ULL:8ULL);}
 FG174S_GDN((int64_t)(uintptr_t)&p,blockIdx.x,blockIdx.y,blockIdx.z,threadIdx.x,threadIdx.y,1);
}
template<int SV,bool KDA>static void fg174s_keep(fg174s_packet*p){auto&ctx=*(ggml_backend_cuda_context*)(uintptr_t)p->args[1];const auto&q=p->plan;ggml_cuda_kernel_launch_params lp(dim3(q.iv[8],q.iv[9],q.iv[10]),dim3(q.iv[11],q.iv[12],1),0,ctx.stream());if(q.iv[7])ggml_cuda_kernel_launch(fg174s_kernel<SV,KDA,true>,lp,q);else ggml_cuda_kernel_launch(fg174s_kernel<SV,KDA,false>,lp,q);}
template<int SV>static void fg174s_gate(fg174s_packet*p){if(p->plan.iv[16])fg174s_keep<SV,true>(p);else fg174s_keep<SV,false>(p);}
void fg174s_launch(int64_t pp){auto*p=(fg174s_packet*)(uintptr_t)pp;switch(p->plan.iv[0]){case 16:fg174s_gate<16>(p);return;case 32:fg174s_gate<32>(p);return;case 64:fg174s_gate<64>(p);return;case 128:fg174s_gate<128>(p);return;default:GGML_ABORT("Invalid GDN state size");}}
extern "C" __attribute__((visibility("default"))) uint64_t fg174s_cuda_host_seen(){return fg174s_seen.load();}
extern "C" __attribute__((visibility("default"))) int fg174s_cuda_device_read(uint64_t*out){auto st=cudaMemcpyFromSymbol(out,fg174s_modes,sizeof(fg174s_modes));if(st==cudaSuccess)st=cudaMemcpyFromSymbol(out+1,fg174s_sizes,sizeof(fg174s_sizes));return(int)st;}
static void fg174s_report(){uint64_t host=fg174s_seen.load();if(!host)return;uint64_t out[2]={};int st=fg174s_cuda_device_read(out);fprintf(stderr,"FG174S_CUDA_GDN host=0x%" PRIx64 ",cuda_status=%d,modes=0x%" PRIx64 ",sizes=0x%" PRIx64 "\n",host,st,out[0],out[1]);}
static void fg174s_enter(ggml_backend_cuda_context&ctx,ggml_tensor*dst,const ggml_cuda_gated_delta_net_fused_cache*cache){if(!fg174s_enabled()){if(cache)fg174s_native_ggml_cuda_op_gated_delta_net_fused_cache(ctx,dst,*cache);else fg174s_native_ggml_cuda_op_gated_delta_net(ctx,dst);return;}static const bool registered=[](){std::atexit(fg174s_report);return true;}();(void)registered;uint64_t bit=cache?2ULL:1ULL;if(!(fg174s_thread_seen&bit)){fg174s_thread_seen|=bit;fg174s_seen.fetch_or(bit,std::memory_order_relaxed);}fg174s_packet p{};p.args[0]=cache?1:0;p.args[1]=(int64_t)(uintptr_t)&ctx;p.args[2]=(int64_t)(uintptr_t)dst;p.args[3]=cache?(int64_t)(uintptr_t)cache->data:0;p.args[4]=cache?cache->slot_stride:0;fg174s_host_entry((int64_t)(uintptr_t)&p);}
void ggml_cuda_op_gated_delta_net(ggml_backend_cuda_context&ctx,ggml_tensor*dst){fg174s_enter(ctx,dst,nullptr);}
void ggml_cuda_op_gated_delta_net_fused_cache(ggml_backend_cuda_context&ctx,ggml_tensor*dst,ggml_cuda_gated_delta_net_fused_cache cache){fg174s_enter(ctx,dst,&cache);}
