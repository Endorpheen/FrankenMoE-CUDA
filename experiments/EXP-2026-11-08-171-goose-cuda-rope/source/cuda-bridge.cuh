#include "host-ffi.h"
#include <atomic>
#include <cstdlib>
#include <cinttypes>
extern "C" void fg171r_host_entry(int64_t);
struct fg171r_packet{int64_t args[3];fg171r_plan plan;};
static std::atomic<uint64_t>fg171r_seen{0};static thread_local uint64_t fg171r_thread_seen;
static bool fg171r_enabled(){static const bool on=[](){const char*v=getenv("GGML_GOOSE_CUDA_ROPE");return !(v&&v[0]=='0'&&v[1]==0);}();return on;}
static void fg171r_report();
int64_t fg171r_arg(int64_t p,int64_t i){GGML_ASSERT(i>=0&&i<3);return((const fg171r_packet*)(uintptr_t)p)->args[i];}
int64_t fg171r_plan_ptr(int64_t p){return(int64_t)(uintptr_t)&((fg171r_packet*)(uintptr_t)p)->plan;}
uint64_t fg171r_hget(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<4&&i>=0&&i<(k==0?5:k==3?32:16));return fg171r_project((const fg171r_plan*)(uintptr_t)p,k,i);}
void fg171r_hput(int64_t p,int64_t k,int64_t i,uint64_t v){auto*q=(fg171r_plan*)(uintptr_t)p;switch(k){case 0:GGML_ASSERT(i>=0&&i<5);q->ptrs[i]=v;return;case 1:GGML_ASSERT(i>=0&&i<16);q->dims[i/4][i%4]=v;return;case 2:GGML_ASSERT(i>=0&&i<16);q->strides[i/4][i%4]=v;return;case 3:GGML_ASSERT(i>=0&&i<32);q->iv[i]=v;return;default:GGML_ABORT("Invalid RoPE plan write");}}
void fg171r_hfloat(int64_t p,int64_t i,float f){GGML_ASSERT(i>=0&&i<8);((fg171r_plan*)(uintptr_t)p)->fv[i]=f;}
int64_t fg171r_ti(int64_t p,int64_t k,int64_t i){GGML_ASSERT(p);const auto*t=(const ggml_tensor*)(uintptr_t)p;switch(k){case 0:GGML_ASSERT(i>=0&&i<GGML_MAX_SRC);return(int64_t)(uintptr_t)t->src[i];case 1:return(int64_t)(uintptr_t)t->data;case 2:return t->type;case 3:GGML_ASSERT(i>=0&&i<4);return t->ne[i];case 4:GGML_ASSERT(i>=0&&i<4);return(int64_t)t->nb[i];case 8:{GGML_ASSERT(i>=0&&i<GGML_MAX_OP_PARAMS/4);int32_t v;memcpy(&v,(const char*)t->op_params+4*i,4);return v;}case 9:return(int64_t)ggml_nbytes(t);default:GGML_ABORT("Invalid RoPE tensor projection");}}
float fg171r_tf(int64_t p,int64_t i){GGML_ASSERT(p&&i>=0&&i<GGML_MAX_OP_PARAMS/4);float f;memcpy(&f,(const char*)((const ggml_tensor*)(uintptr_t)p)->op_params+4*i,4);return f;}
uint8_t fg171r_contiguous(int64_t t){return ggml_is_contiguous((const ggml_tensor*)(uintptr_t)t);}
int64_t fg171r_f32_type(){return GGML_TYPE_F32;}int64_t fg171r_f16_type(){return GGML_TYPE_F16;}int64_t fg171r_i32_type(){return GGML_TYPE_I32;}
float fg171r_hfloor(float f){return floorf(f);}float fg171r_hceil(float f){return ceilf(f);}
float fg171r_hlog(float f){return logf(f);}float fg171r_hpow(float a,float b){return powf(a,b);}
void fg171r_fail(int64_t c){GGML_ABORT("Goose RoPE validation failed:code=%" PRId64,c);}
static __device__ unsigned long long fg171r_modes,fg171r_variants;
template<int Mode,bool Xhalf,bool Dhalf,bool Forward,bool FF>static __global__ void fg171r_kernel(fg171r_plan p){p.iv[0]=Mode;p.iv[11]=Xhalf;p.iv[12]=Dhalf;p.iv[7]=Forward;p.iv[9]=FF;
 if(blockIdx.x==0&&blockIdx.y==0&&threadIdx.y==0){atomicOr(&fg171r_modes,Mode==0?1ULL:Mode==2?2ULL:Mode==8?4ULL:8ULL);atomicOr(&fg171r_variants,(Xhalf?1ULL:0ULL)|(Dhalf?2ULL:0ULL)|(FF?4ULL:0ULL)|(!Forward?8ULL:0ULL)|(p.iv[8]?16ULL:0ULL)|(p.iv[2]?32ULL:0ULL)|(p.iv[1]<p.dims[0][0]?64ULL:0ULL));}
 FG171R_ROPE((int64_t)(uintptr_t)&p,(int64_t)blockDim.y*blockIdx.y+threadIdx.y,(int64_t)blockDim.x*blockIdx.x+threadIdx.x,1);
}
template<int M,bool X,bool D,bool F>static void fg171r_ff(fg171r_packet*p){auto&ctx=*(ggml_backend_cuda_context*)(uintptr_t)p->args[1];const auto&q=p->plan;ggml_cuda_kernel_launch_params lp(dim3(q.iv[13],q.iv[14],1),dim3(1,256,1),0,ctx.stream());if(q.iv[9])ggml_cuda_kernel_launch(fg171r_kernel<M,X,D,F,true>,lp,q);else ggml_cuda_kernel_launch(fg171r_kernel<M,X,D,F,false>,lp,q);}
template<int M,bool X,bool D>static void fg171r_direction(fg171r_packet*p){if(p->plan.iv[7])fg171r_ff<M,X,D,true>(p);else fg171r_ff<M,X,D,false>(p);}
template<int M>static void fg171r_types(fg171r_packet*p){if(p->plan.iv[11])fg171r_direction<M,true,true>(p);else if(p->plan.iv[12])fg171r_direction<M,false,true>(p);else fg171r_direction<M,false,false>(p);}
void fg171r_launch(int64_t pp){auto*p=(fg171r_packet*)(uintptr_t)pp;switch(p->plan.iv[0]){case 0:fg171r_types<0>(p);return;case 2:fg171r_types<2>(p);return;case 8:fg171r_types<8>(p);return;case 40:fg171r_types<40>(p);return;default:GGML_ABORT("Invalid RoPE mode");}}
extern "C" __attribute__((visibility("default"))) uint64_t fg171r_cuda_host_seen(){return fg171r_seen.load();}
extern "C" __attribute__((visibility("default"))) int fg171r_cuda_device_read(uint64_t*out){auto st=cudaMemcpyFromSymbol(out,fg171r_modes,sizeof(fg171r_modes));if(st==cudaSuccess)st=cudaMemcpyFromSymbol(out+1,fg171r_variants,sizeof(fg171r_variants));return(int)st;}
static void fg171r_report(){uint64_t host=fg171r_seen.load();if(!host)return;uint64_t out[2]={};int st=fg171r_cuda_device_read(out);fprintf(stderr,"FG171R_CUDA_ROPE host=0x%" PRIx64 ",cuda_status=%d,modes=0x%" PRIx64 ",variants=0x%" PRIx64 "\n",host,st,out[0],out[1]);}
static void fg171r_enter(ggml_backend_cuda_context&ctx,ggml_tensor*dst,int mode){GGML_ASSERT(dst);if(!fg171r_enabled()||((const int32_t*)dst->op_params)[2]==GGML_ROPE_TYPE_VISION){if(mode)fg171r_native_ggml_cuda_op_rope_back(ctx,dst);else fg171r_native_ggml_cuda_op_rope(ctx,dst);return;}static const bool registered=[](){std::atexit(fg171r_report);return true;}();(void)registered;uint64_t bit=1ULL<<mode;if(!(fg171r_thread_seen&bit)){fg171r_thread_seen|=bit;fg171r_seen.fetch_or(bit,std::memory_order_relaxed);}fg171r_packet p{};p.args[0]=mode;p.args[1]=(int64_t)(uintptr_t)&ctx;p.args[2]=(int64_t)(uintptr_t)dst;fg171r_host_entry((int64_t)(uintptr_t)&p);}
void ggml_cuda_op_rope(ggml_backend_cuda_context&ctx,ggml_tensor*dst){fg171r_enter(ctx,dst,0);}
void ggml_cuda_op_rope_back(ggml_backend_cuda_context&ctx,ggml_tensor*dst){fg171r_enter(ctx,dst,1);}
void ggml_cuda_op_rope_fused(ggml_backend_cuda_context&ctx,ggml_tensor*dst,ggml_tensor*setrows){fg171r_native_ggml_cuda_op_rope_fused(ctx,dst,setrows);}
void ggml_cuda_op_rms_norm_mul_rope_fused(ggml_backend_cuda_context&ctx,ggml_tensor*rms,ggml_tensor*mul,ggml_tensor*rope,ggml_tensor*setrows){fg171r_native_ggml_cuda_op_rms_norm_mul_rope_fused(ctx,rms,mul,rope,setrows);}
