#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-cpu-impl.h"
#include "tiled/tiled.h"
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <cstdio>
extern "C" {
#include "bridge.h"
void fg_selfhost_quant_q8k(const float*,void*,int64_t);
bool fg141_native_dense_tiled(const struct ggml_compute_params*,struct ggml_tensor*);
bool fg141_native_moe_tiled(const struct ggml_compute_params*,struct ggml_tensor*,int64_t,int64_t,const int32_t*,char*);
}
#ifndef FG141_BRIDGE_TRACE
#define FG141_BRIDGE_TRACE 1
#endif
static bool enabled=true,master=true,forced=false;
#if FG141_BRIDGE_TRACE
static std::atomic<uint64_t> dense_calls{0},moe_calls{0},native_dense{0},native_moe{0};
#endif
__attribute__((constructor))static void fg141_config(){const char*v=std::getenv("GGML_GOOSE_TILED_GENERAL");enabled=!v||std::strcmp(v,"0");v=std::getenv("GGML_CPU_TILED_MM");master=!v||std::atoi(v)!=0;v=std::getenv("GGML_CPU_TILED_MM_FORCE");forced=v&&std::atoi(v)==1;}
extern "C" uint8_t fg141_general_enabled(void){return enabled;}
static int kind_of(enum ggml_type type){switch(type){case GGML_TYPE_Q4_K:return 0;case GGML_TYPE_Q5_K:return 1;case GGML_TYPE_Q6_K:return 2;case GGML_TYPE_Q3_K:return 3;case GGML_TYPE_Q2_K:return 4;case GGML_TYPE_IQ4_XS:return 5;case GGML_TYPE_IQ2_XXS:return 6;case GGML_TYPE_IQ2_XS:return 7;case GGML_TYPE_IQ2_S:return 8;case GGML_TYPE_IQ3_XXS:return 9;case GGML_TYPE_IQ3_S:return 10;case GGML_TYPE_IQ1_S:return 11;case GGML_TYPE_IQ1_M:return 12;default:return -1;}}
static bool native_isa_fallback(){
#if defined(__AVX2__) && !(defined(__AVX512VNNI__) && defined(__AVX512VL__) && defined(__AVX512DQ__))
 return false;
#else
 return true;
#endif
}
static bool candidate(const struct ggml_compute_params*params,const struct ggml_tensor*dst){
#if defined(__AVX2__) && !(defined(__AVX512VNNI__) && defined(__AVX512VL__) && defined(__AVX512DQ__))
 const auto*x=dst->src[0];const auto*y=dst->src[1];if(!enabled||!master||params->use_ref||x->extra||kind_of(x->type)<0||x->ne[0]<=0||x->ne[0]%256||x->ne[1]<=0)return false;
 if(y->type!=GGML_TYPE_F32&&y->type!=GGML_TYPE_Q8_K)return false;
 const size_t block=ggml_type_size(x->type);if(x->nb[0]!=block||x->nb[1]%block||x->nb[2]%block||x->nb[3]%block)return false;
 if(y->ne[0]!=x->ne[0]||y->ne[1]<=0||y->ne[2]<=0||y->ne[3]<=0||y->nb[0]!=ggml_type_size(y->type))return false;
 if(y->type==GGML_TYPE_Q8_K&&(y->nb[1]%292||y->nb[2]!=y->nb[1]*y->ne[1]||y->nb[3]!=y->nb[2]*y->ne[2]))return false;
 if(dst->type!=GGML_TYPE_F32||dst->nb[0]!=4||dst->nb[1]%4||dst->nb[2]%4||dst->nb[3]%4)return false;
 return true;
#else
 (void)params;(void)dst;return false;
#endif
}
static fg141_plan metadata(const struct ggml_compute_params*params,const struct ggml_tensor*dst){
 const auto*x=dst->src[0];const auto*y=dst->src[1];fg141_plan p={};p.kind=kind_of(x->type);p.stride=x->nb[1];p.slots=y->ne[1];p.blocks=x->ne[0]/256;p.y_stride=y->type==GGML_TYPE_Q8_K?y->nb[1]/292:p.blocks;p.width=x->ne[0];p.output_rows=x->ne[1];p.ith=params->ith;p.nth=params->nth;p.enabled=1;p.forced=forced;p.out_stride1=dst->nb[1]/4;p.out_stride2=dst->nb[2]/4;p.out_stride3=dst->nb[3]/4;p.n11=y->ne[1];p.n12=y->ne[2];p.n13=y->ne[3];p.n02=x->ne[2];p.n03=x->ne[3];p.nb02=x->nb[2];p.nb03=x->nb[3];p.need_quant=y->type!=GGML_TYPE_Q8_K;p.src_nb0=y->nb[0];p.src_nb1=y->nb[1];p.src_nb2=y->nb[2];p.src_nb3=y->nb[3];p.workspace_mod=(uintptr_t)params->wdata%64;p.wdata_bytes=params->wsize;return p;
}
struct fg141_context {const ggml_compute_params*params;const ggml_tensor*input;const fg141_plan*plan;};
extern "C" void fg141_quantize(int64_t context,int64_t source,int64_t target,int64_t values){
 const auto*c=(const fg141_context*)(uintptr_t)context;const auto*p=c->plan;GGML_ASSERT(source>=0&&target>=0&&values>=0&&values%256==0);
 const size_t input_bytes=ggml_nbytes(c->input);GGML_ASSERT((uint64_t)source<=input_bytes&&(uint64_t)values*4<=input_bytes-(uint64_t)source);
 const size_t capacity=(size_t)(p->width/256)*292*p->n11*p->n12*p->n13;const size_t bytes=(size_t)(values/256)*292;GGML_ASSERT((uint64_t)target<=capacity&&bytes<=capacity-(uint64_t)target);
 fg_selfhost_quant_q8k((const float*)((const char*)c->input->data+source),(char*)c->params->wdata+target,values);
}
extern "C" void fg141_reset(int64_t context,int32_t value){const auto*c=(const fg141_context*)(uintptr_t)context;ggml_threadpool_chunk_set(c->params->threadpool,value);}
extern "C" void fg141_barrier(int64_t context){const auto*c=(const fg141_context*)(uintptr_t)context;ggml_barrier(c->params->threadpool);}
extern "C" int32_t fg141_claim(int64_t context){const auto*c=(const fg141_context*)(uintptr_t)context;return ggml_threadpool_chunk_add(c->params->threadpool,1);}
bool ggml_compute_forward_mul_mat_id_tiled(const struct ggml_compute_params*params,struct ggml_tensor*dst,int64_t expert,int64_t count,const int32_t*routes,char*scratch){
 if(!enabled||native_isa_fallback()){
#if FG141_BRIDGE_TRACE
  native_moe.fetch_add(1,std::memory_order_relaxed);
#endif
  return fg141_native_moe_tiled(params,dst,expert,count,routes,scratch);
 }
 if(!candidate(params,dst))return false;
 const auto*x=dst->src[0];const auto*y=dst->src[1];auto p=metadata(params,dst);p.mode=3;p.count=count;
 const void*activation=y->type==GGML_TYPE_Q8_K?y->data:params->wdata;int64_t ny=y->type==GGML_TYPE_Q8_K?ggml_nbytes(y)/292:ggml_nelements(y)/256;
 GGML_ASSERT(expert>=0&&expert<x->ne[2]&&scratch);const size_t offset=(size_t)((uintptr_t)scratch-(uintptr_t)params->wdata)+(size_t)params->ith*524288;GGML_ASSERT(offset<=params->wsize&&445440<=params->wsize-offset);
 const void*weights=(const char*)x->data+expert*x->nb[2];int64_t nx=(x->ne[1]-1)*x->nb[1]+ggml_row_size(x->type,x->ne[0]);
 auto status=fg141_execute(&p,weights,nx,activation,ny,routes,count,scratch+(size_t)params->ith*524288,(float*)dst->data,ggml_nbytes(dst)/4);
#if FG141_BRIDGE_TRACE
 if(status)moe_calls.fetch_add(1,std::memory_order_relaxed);
#endif
 return status!=0;
}
bool ggml_compute_forward_mul_mat_tiled(const struct ggml_compute_params*params,struct ggml_tensor*dst){
 if(!enabled||native_isa_fallback()){
#if FG141_BRIDGE_TRACE
  native_dense.fetch_add(1,std::memory_order_relaxed);
#endif
  return fg141_native_dense_tiled(params,dst);
 }
 if(!candidate(params,dst))return false;
 const auto*x=dst->src[0];const auto*y=dst->src[1];auto p=metadata(params,dst);p.mode=4;p.dense=1;fg141_context context={params,y,&p};p.context=(int64_t)(uintptr_t)&context;
 const int64_t offset=fg141_workspace_offset(&p);const void*activation=p.need_quant?params->wdata:y->data;int64_t ny=p.need_quant?ggml_nelements(y)/256:ggml_nbytes(y)/292;
 auto status=fg141_execute(&p,x->data,ggml_nbytes(x),activation,ny,nullptr,0,(char*)params->wdata+offset,(float*)dst->data,ggml_nbytes(dst)/4);
#if FG141_BRIDGE_TRACE
 if(status)dense_calls.fetch_add(1,std::memory_order_relaxed);
#endif
 return status!=0;
}
#if FG141_BRIDGE_TRACE
__attribute__((destructor))static void fg141_bridge_report(){std::fprintf(stderr,"FG141_BRIDGE dense=%llu moe=%llu native_dense_fallback=%llu native_moe_fallback=%llu enabled=%d\n",(unsigned long long)dense_calls.load(),(unsigned long long)moe_calls.load(),(unsigned long long)native_dense.load(),(unsigned long long)native_moe.load(),enabled);}
#endif
