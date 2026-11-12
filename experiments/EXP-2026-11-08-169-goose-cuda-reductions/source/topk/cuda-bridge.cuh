#include "host-ffi.h"
#include <atomic>
#include <cinttypes>
#include <cstdlib>
extern "C" uint8_t fg169t_host_entry(int64_t);
struct fg169t_packet{int64_t args[10];fg169t_plan plan;};
static std::atomic<uint64_t>fg169t_seen{0};static thread_local uint64_t fg169t_thread_seen;
static bool fg169t_enabled(){static const bool enabled=[](){const char*v=getenv("GGML_GOOSE_CUDA_TOPK_MOE");return!(v&&v[0]=='0'&&v[1]==0);}();return enabled;}
static void fg169t_report();
static uint8_t fg169t_enter(fg169t_packet&p,int mode){static const bool registered=[](){std::atexit(fg169t_report);return true;}();(void)registered;const uint64_t bit=UINT64_C(1)<<mode;if(!(fg169t_thread_seen&bit)){fg169t_thread_seen|=bit;fg169t_seen.fetch_or(bit,std::memory_order_relaxed);}p.args[0]=mode;return fg169t_host_entry((int64_t)(uintptr_t)&p);}
int64_t fg169t_arg(int64_t p,int64_t i){GGML_ASSERT(i>=0&&i<10);return((const fg169t_packet*)(uintptr_t)p)->args[i];}
int64_t fg169t_plan_ptr(int64_t p){return(int64_t)(uintptr_t)&((fg169t_packet*)(uintptr_t)p)->plan;}
uint64_t fg169t_hget(int64_t p,int64_t k,int64_t i){return fg169t_project((const fg169t_plan*)(uintptr_t)p,k,i);}
void fg169t_hput(int64_t pp,int64_t k,int64_t i,uint64_t v){auto*p=(fg169t_plan*)(uintptr_t)pp;if(k==0){GGML_ASSERT(i>=0&&i<4);p->ptrs[i]=v;return;}if(k==3){GGML_ASSERT(i>=0&&i<16);p->iv[i]=v;return;}GGML_ABORT("Invalid topk plan write");}
void fg169t_hfloat(int64_t p,int64_t i,float v){GGML_ASSERT(i>=0&&i<2);((fg169t_plan*)(uintptr_t)p)->fv[i]=v;}
int64_t fg169t_ti(int64_t p,int64_t k,int64_t i){GGML_ASSERT(p);const auto*t=(const ggml_tensor*)(uintptr_t)p;switch(k){case 0:GGML_ASSERT(i>=0&&i<GGML_MAX_SRC);return(int64_t)(uintptr_t)t->src[i];case 1:return(int64_t)(uintptr_t)t->data;case 2:return t->type;case 3:GGML_ASSERT(i>=0&&i<4);return t->ne[i];case 4:GGML_ASSERT(i>=0&&i<4);return(int64_t)t->nb[i];case 5:return ggml_get_unary_op(t);case 6:return t->op;case 9:return(int64_t)ggml_nbytes(t);default:GGML_ABORT("Invalid topk tensor field");}}
float fg169t_tf(int64_t p,int64_t i){GGML_ASSERT(p&&i>=0&&i<GGML_MAX_OP_PARAMS/4);float f;memcpy(&f,(const char*)((const ggml_tensor*)(uintptr_t)p)->op_params+4*i,4);return f;}
uint8_t fg169t_contiguous(int64_t p){return ggml_is_contiguous((const ggml_tensor*)(uintptr_t)p);}
int64_t fg169t_cfg(int64_t p,int64_t i){const auto&a=*(const ggml_cuda_topk_moe_args*)(uintptr_t)((const fg169t_packet*)(uintptr_t)p)->args[8];switch(i){case 0:return a.sigmoid;case 1:return a.sqrt_softplus;case 2:return a.delayed_softmax;case 3:return a.n_groups;case 4:return a.n_group_used;default:GGML_ABORT("Invalid topk cfg field");}}
int64_t fg169t_constant(int64_t i){switch(i){case 0:return GGML_TYPE_F32;case 1:return GGML_TYPE_I32;case 2:return GGML_OP_SOFT_MAX;case 3:return GGML_OP_UNARY;case 4:return GGML_UNARY_OP_SIGMOID;case 5:return GGML_UNARY_OP_SOFTPLUS;default:GGML_ABORT("Invalid topk constant");}}
void fg169t_fail(int64_t code){GGML_ABORT("Goose topk validation failed:code=%" PRId64,code);}
static __device__ unsigned long long fg169t_experts,fg169t_flags;
template<int NE,bool Bias>static __global__ __launch_bounds__(256,1) void fg169t_kernel(fg169t_plan p){uint64_t row=(uint64_t)blockIdx.x*blockDim.y+threadIdx.y;int32_t lane=threadIdx.x;if(blockIdx.x==0&&threadIdx.x==0&&threadIdx.y==0){constexpr int expert_bit=NE==1?0:NE==2?1:NE==4?2:NE==8?3:NE==16?4:NE==32?5:NE==64?6:NE==128?7:NE==256?8:NE==288?9:NE==512?10:NE==576?11:0;atomicOr(&fg169t_experts,1ULL<<expert_bit);uint64_t f=p.iv[3];uint64_t modes=(f&8?16ULL:(f&1?2ULL:(f&2?4ULL:1ULL)))|(f&4?8ULL:0ULL)|(Bias?32ULL:0ULL)|(p.iv[4]?64ULL:0ULL);atomicOr(&fg169t_flags,modes);}
if constexpr(NE<=32){FG169T_LANE_1((int64_t)(uintptr_t)&p,row,lane,NE,1,Bias,0);}
else if constexpr(NE/32==2){FG169T_LANE_2((int64_t)(uintptr_t)&p,row,lane,NE,2,Bias,0);}
else if constexpr(NE/32==4){FG169T_LANE_4((int64_t)(uintptr_t)&p,row,lane,NE,4,Bias,0);}
else if constexpr(NE/32==8){FG169T_LANE_8((int64_t)(uintptr_t)&p,row,lane,NE,8,Bias,0);}
else if constexpr(NE/32==9){FG169T_LANE_9((int64_t)(uintptr_t)&p,row,lane,NE,9,Bias,0);}
else if constexpr(NE/32==16){FG169T_LANE_16((int64_t)(uintptr_t)&p,row,lane,NE,16,Bias,0);}
else if constexpr(NE/32==18){FG169T_LANE_18((int64_t)(uintptr_t)&p,row,lane,NE,18,Bias,0);}
}
template<int NE>static void fg169t_bias(fg169t_packet*p){const auto&q=p->plan;auto&ctx=*(ggml_backend_cuda_context*)(uintptr_t)p->args[1];ggml_cuda_kernel_launch_params config((dim3)q.iv[8],dim3(32,8),0,ctx.stream());if(q.iv[3]&16)ggml_cuda_kernel_launch(fg169t_kernel<NE,true>,config,q);else ggml_cuda_kernel_launch(fg169t_kernel<NE,false>,config,q);}
void fg169t_launch(int64_t pp){auto*p=(fg169t_packet*)(uintptr_t)pp;switch(p->plan.iv[0]){
case 1:fg169t_bias<1>(p);return;
case 2:fg169t_bias<2>(p);return;
case 4:fg169t_bias<4>(p);return;
case 8:fg169t_bias<8>(p);return;
case 16:fg169t_bias<16>(p);return;
case 32:fg169t_bias<32>(p);return;
case 64:fg169t_bias<64>(p);return;
case 128:fg169t_bias<128>(p);return;
case 256:fg169t_bias<256>(p);return;
case 288:fg169t_bias<288>(p);return;
case 512:fg169t_bias<512>(p);return;
case 576:fg169t_bias<576>(p);return;
default:GGML_ABORT("Invalid topk expert count");}}
extern "C" __attribute__((visibility("default"))) uint64_t fg169t_cuda_host_seen(){return fg169t_seen.load();}
extern "C" __attribute__((visibility("default"))) int fg169t_cuda_device_read(uint64_t*out){auto st=cudaMemcpyFromSymbol(out,fg169t_experts,sizeof(fg169t_experts));if(st==cudaSuccess)st=cudaMemcpyFromSymbol(out+1,fg169t_flags,sizeof(fg169t_flags));return(int)st;}
static void fg169t_report(){uint64_t host=fg169t_seen.load();if(!host)return;uint64_t out[2]={};int status=fg169t_cuda_device_read(out);fprintf(stderr,"FG169T_CUDA_TOPK host=0x%" PRIx64 ",cuda_status=%d,experts=0x%" PRIx64 ",flags=0x%" PRIx64 "\n",host,status,out[0],out[1]);}
void ggml_cuda_op_topk_moe(ggml_backend_cuda_context&ctx,const ggml_tensor*logits,ggml_tensor*weights,ggml_tensor*ids,const ggml_tensor*clamp,const ggml_tensor*scale,const ggml_tensor*bias,const ggml_cuda_topk_moe_args&args){if(!fg169t_enabled()){fg169t_native_ggml_cuda_op_topk_moe(ctx,logits,weights,ids,clamp,scale,bias,args);return;}fg169t_packet p{};p.args[1]=(int64_t)(uintptr_t)&ctx;p.args[2]=(int64_t)(uintptr_t)logits;p.args[3]=(int64_t)(uintptr_t)weights;p.args[4]=(int64_t)(uintptr_t)ids;p.args[5]=(int64_t)(uintptr_t)clamp;p.args[6]=(int64_t)(uintptr_t)scale;p.args[7]=(int64_t)(uintptr_t)bias;p.args[8]=(int64_t)(uintptr_t)&args;fg169t_enter(p,0);}
bool ggml_cuda_should_use_topk_moe(const ggml_tensor*gate,const ggml_tensor*weights,const ggml_tensor*logits,const ggml_tensor*ids){if(!fg169t_enabled())return fg169t_native_ggml_cuda_should_use_topk_moe(gate,weights,logits,ids);fg169t_packet p{};p.args[2]=(int64_t)(uintptr_t)logits;p.args[3]=(int64_t)(uintptr_t)weights;p.args[4]=(int64_t)(uintptr_t)ids;p.args[9]=(int64_t)(uintptr_t)gate;return fg169t_enter(p,1)!=0;}
