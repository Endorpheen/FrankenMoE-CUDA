from pathlib import Path
import hashlib,json
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/hybrid-inputs';D.mkdir(exist_ok=True)
N=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/src/llama-graph.cpp');native=N.read_text();(D/'native-graph.cpp').write_text(native)
(D/'kernel.goose').write_text('''extern "fg153_hi_get" fn hi_get(p:i64,k:i64,i:i64)->i64;
extern "fg153_hi_put" fn hi_put(p:i64,k:i64,v:i64);
extern "fg153_hi_ne" fn hi_ne(t:i64,axis:i64)->i64;
extern "fg153_hi_call" fn hi_call(op:i64,p:i64,a:i64,b:i64,c:i64,d:i64)->i64;
extern "fg153_hi_witness" fn hi_witness(mode:i64);
fn hi_ensure(ok:bool){if !ok {let failed=hi_call(99,0,0,0,0,0);assert(false);}}
fn hi_streams(p:i64)->i64 {if hi_get(p,3,0)!=0{return 1;}let n=hi_get(p,4,0);hi_ensure(n>0);return n;}
fn hi_build(p:i64)->i64 {
 let rs=hi_call(0,p,0,0,0,0);let count=hi_get(p,0,0);hi_ensure(count>0);
 let copy=hi_call(1,p,26,count,0,0);hi_put(p,0,copy);let marked=hi_call(2,p,copy,0,0,0);
 let main=hi_call(3,p,copy,hi_get(p,1,0),0,0);hi_put(p,1,main);
 let tail=hi_call(3,p,copy,count-1,hi_call(4,p,copy,0,0,0),0);hi_put(p,2,tail);
 hi_put(p,3,hi_get(p,5,0));hi_put(p,4,hi_get(p,6,0));
 let attn=hi_call(5,p,0,0,0,0);hi_ensure(hi_get(p,7,0)==0);
 hi_put(p,5,hi_call(6,p,0,0,0,0));hi_put(p,6,hi_call(6,p,1,0,0,0));
 let streams=hi_streams(p);let tokens=hi_get(p,2,0);var kind:i64=0;if hi_get(p,8,0)!=0 {kind=1;}
 let mask=hi_call(7,p,kind,hi_get(p,9,0),tokens/streams,streams);hi_put(p,7,mask);hi_put(p,8,mask);
 hi_put(p,9,hi_call(6,p,2,0,0,0));hi_put(p,10,hi_call(6,p,3,0,0,0));return hi_call(8,p,0,0,0,0);
}
fn hi_set(p:i64)->i64 {
 let k=hi_call(10,p,0,0,0,0);let v=hi_call(10,p,1,0,0,0);let mask=hi_call(10,p,2,0,0,0);
 if hi_get(p,10,0)!=0 {let rot=hi_call(10,p,3,0,0,0);}
 if hi_get(p,11,0)!=0 {let rot=hi_call(10,p,4,0,0,0);}
 let count=hi_get(p,0,0);if hi_get(p,12,0)!=0 {
  let host=hi_call(11,p,0,0,0,0);hi_ensure(host!=0);
  hi_ensure(count>=0&&count<=hi_ne(hi_get(p,12,0),0));
  var i:i64=0;while i<count {let stored=hi_call(12,p,i,hi_call(13,p,i,0,0,0),0,0);i+=1;}
 }
 return 0;
}
fn hi_reuse(p:i64)->i64 {
 let changed=hi_call(14,p,0,0,0,0);var ok:i64=1;
 if hi_ne(hi_get(p,13,0),0)!=hi_get(p,2,0) {ok=0;}
 let mask=hi_get(p,14,0);let streams=hi_streams(p);
 if hi_ne(mask,0)!=hi_get(p,9,0) {ok=0;}
 if hi_ne(mask,1)!=hi_get(p,2,0)/streams {ok=0;}
 if hi_ne(mask,2)!=1 {ok=0;}
 if hi_ne(mask,3)!=streams {ok=0;}
 if hi_ne(hi_get(p,12,0),0)!=hi_get(p,0,0) {ok=0;}
 if hi_ne(hi_get(p,15,0),0)!=hi_get(p,1,0) {ok=0;}
 if hi_get(p,16,0)!=hi_get(p,5,0) {ok=0;}
 if hi_get(p,17,0)!=hi_get(p,6,0) {ok=0;}
 return ok;
}
fn hi_entry(mode:i64,p:i64)->i64 {
 hi_witness(mode);if mode==0{return hi_build(p);}if mode==1{return hi_set(p);}if mode==2{return hi_reuse(p);}if mode==3{return 1;}hi_ensure(false);return 0;
}
fn main(){print(hi_entry(3,0));}
''')
(D/'bridge.h').write_text('''#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int fg153_hybrid_inputs_enabled(void);
int64_t fg153_hybrid_inputs(int64_t,int64_t);
int64_t fg153_hi_get(int64_t,int64_t,int64_t);
void fg153_hi_put(int64_t,int64_t,int64_t);
int64_t fg153_hi_ne(int64_t,int64_t);
int64_t fg153_hi_call(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg153_hi_witness(int64_t);
#ifdef __cplusplus
}
#endif
''')
(D/'frame.hpp').write_text('''#pragma once
#include "llama-graph.h"
#include "llama-memory-hybrid.h"
#include "bridge.h"
struct FG153HybridInputFrame {
 const llm_graph_context *graph=nullptr;
 llm_graph_input_mem_hybrid *input=nullptr;
 const llama_ubatch *batch=nullptr;
 const llm_graph_params *params=nullptr;
 const llama_memory_hybrid_context *memory=nullptr;
 std::unique_ptr<llm_graph_input_rs> rs;
 std::unique_ptr<llm_graph_input_attn_kv> attn;
 explicit FG153HybridInputFrame(const llm_graph_context &g):graph(&g),batch(&g.ubatch),memory(static_cast<const llama_memory_hybrid_context*>(g.mctx)){}
 FG153HybridInputFrame(llm_graph_input_mem_hybrid *i,const llama_ubatch *u,const llm_graph_params *p=nullptr):input(i),batch(u),params(p),memory(i->mctx){}
};
''')
(D/'services.cpp').write_text('''#include "frame.hpp"
#include "llama-impl.h"
#include "ggml-backend.h"
#include <cstdlib>
#include <cstring>
static FG153HybridInputFrame &f(int64_t p){GGML_ASSERT(p);return *(FG153HybridInputFrame*)(intptr_t)p;}
static llm_graph_input_rs *r(FG153HybridInputFrame &p){return p.input?p.input->inp_rs.get():p.rs.get();}
static llm_graph_input_attn_kv *a(FG153HybridInputFrame &p){return p.input?p.input->inp_attn.get():p.attn.get();}
static const llama_cparams &cp(FG153HybridInputFrame &p){return p.params?p.params->cparams:p.input?p.input->cparams:p.graph->cparams;}
static ggml_tensor *t(int64_t h){return(ggml_tensor*)(intptr_t)h;}
extern "C" int fg153_hybrid_inputs_enabled(void){auto*v=getenv("GGML_GOOSE_HYBRID_INPUT");return !v||strcmp(v,"0")!=0;}
extern "C" int64_t fg153_hi_ne(int64_t p,int64_t i){GGML_ASSERT(p&&i>=0&&i<4);return t(p)->ne[i];}
extern "C" int64_t fg153_hi_get(int64_t h,int64_t k,int64_t){auto&p=f(h);switch(k){
case 0:return p.memory->get_recr()->get_n_rs();case 1:return p.batch->n_seqs;case 2:return p.batch->n_tokens;case 3:return cp(p).kv_unified;case 4:return p.batch->n_seqs_unq;case 5:return p.memory->get_recr()->get_head();case 6:return p.memory->get_recr()->get_rs_z();case 7:return p.graph->hparams.swa_type;case 8:return cp(p).flash_attn;case 9:return p.memory->get_attn()->get_n_kv();
case 10:return(intptr_t)a(p)->self_k_rot;case 11:return(intptr_t)a(p)->self_v_rot;case 12:return(intptr_t)r(p)->s_copy;case 13:return(intptr_t)a(p)->self_k_idxs;case 14:return(intptr_t)a(p)->self_kq_mask;case 15:return(intptr_t)r(p)->s_copy_main;case 16:return r(p)->head;case 17:return r(p)->rs_z;default:GGML_ABORT("bad hybrid input field");}}
extern "C" void fg153_hi_put(int64_t h,int64_t k,int64_t v){auto&p=f(h);switch(k){
case 0:r(p)->s_copy=t(v);return;case 1:r(p)->s_copy_main=t(v);return;case 2:r(p)->s_copy_tail=t(v);return;case 3:r(p)->head=v;return;case 4:r(p)->rs_z=v;return;case 5:a(p)->self_k_idxs=t(v);return;case 6:a(p)->self_v_idxs=t(v);return;case 7:a(p)->self_kq_mask=t(v);return;case 8:a(p)->self_kq_mask_cnv=t(v);return;case 9:a(p)->self_k_rot=t(v);return;case 10:a(p)->self_v_rot=t(v);return;default:GGML_ABORT("bad hybrid input field write");}}
extern "C" int64_t fg153_hi_call(int64_t op,int64_t h,int64_t x,int64_t y,int64_t z,int64_t w){if(op==99)GGML_ABORT("Goose hybrid input invariant failed");auto&p=f(h);switch(op){
case 0:p.rs=std::make_unique<llm_graph_input_rs>(p.memory->get_recr());return(intptr_t)p.rs.get();
case 1:return(intptr_t)ggml_new_tensor_1d(p.graph->ctx0,(ggml_type)x,y);
case 2:ggml_set_input(t(x));ggml_set_name(t(x),"rs_s_copy");return 0;
case 3:return(intptr_t)ggml_view_1d(p.graph->ctx0,t(x),y,z);
case 4:GGML_ASSERT(x&&y>=0&&y<4);return t(x)->nb[y];
case 5:p.attn=std::make_unique<llm_graph_input_attn_kv>(p.graph->hparams,p.graph->cparams,p.memory->get_attn());return(intptr_t)p.attn.get();
case 6:switch(x){case 0:return(intptr_t)p.memory->get_attn()->build_input_k_idxs(p.graph->ctx0,*p.batch);case 1:return(intptr_t)p.memory->get_attn()->build_input_v_idxs(p.graph->ctx0,*p.batch);case 2:return(intptr_t)p.memory->get_attn()->build_input_k_rot(p.graph->ctx0);case 3:return(intptr_t)p.memory->get_attn()->build_input_v_rot(p.graph->ctx0);default:GGML_ABORT("bad child cache input");}
case 7:{auto*v=ggml_new_tensor_4d(p.graph->ctx0,(ggml_type)x,y,z,1,w);ggml_set_input(v);ggml_set_name(v,"attn_inp_kq_mask");return(intptr_t)v;}
case 8:{auto v=std::make_unique<llm_graph_input_mem_hybrid>(p.graph->cparams,std::move(p.attn),std::move(p.rs),p.memory);return(intptr_t)p.graph->res->add_input(std::move(v));}
case 10:switch(x){case 0:p.memory->get_attn()->set_input_k_idxs(a(p)->self_k_idxs,p.batch);break;case 1:p.memory->get_attn()->set_input_v_idxs(a(p)->self_v_idxs,p.batch);break;case 2:p.memory->get_attn()->set_input_kq_mask(a(p)->self_kq_mask,p.batch,cp(p).causal_attn);break;case 3:p.memory->get_attn()->set_input_k_rot(a(p)->self_k_rot);break;case 4:p.memory->get_attn()->set_input_v_rot(a(p)->self_v_rot);break;default:GGML_ABORT("bad hybrid input transfer");}return 0;
case 11:return ggml_backend_buffer_is_host(r(p)->s_copy->buffer);
case 12:GGML_ASSERT(x>=0&&x<ggml_nelements(r(p)->s_copy));((int32_t*)r(p)->s_copy->data)[x]=y;return 0;
case 13:return p.memory->get_recr()->s_copy(x);
case 14:p.memory=static_cast<const llama_memory_hybrid_context*>(p.params->mctx);p.input->mctx=p.memory;return 0;
default:GGML_ABORT("bad hybrid input primitive");}}
''')
s=native.replace('#include "llama-graph.h"','#include "llama-graph.h"\n#include "frame.hpp"',1)
hooks=[('llm_graph_input_mem_hybrid * llm_graph_context::build_inp_mem_hybrid() const {','FG153HybridInputFrame frame(*this);return(llm_graph_input_mem_hybrid*)(intptr_t)fg153_hybrid_inputs(0,(intptr_t)&frame);'),('void llm_graph_input_mem_hybrid::set_input(const llama_ubatch * ubatch) {','FG153HybridInputFrame frame(this,ubatch);fg153_hybrid_inputs(1,(intptr_t)&frame);return;'),('bool llm_graph_input_mem_hybrid::can_reuse(const llm_graph_params & params) {','FG153HybridInputFrame frame(this,&params.ubatch,&params);return fg153_hybrid_inputs(2,(intptr_t)&frame)!=0;')]
for sig,body in hooks:
 assert s.count(sig)==1,sig
 s=s.replace(sig,sig+'\n    if(fg153_hybrid_inputs_enabled()){'+body+'}\n',1)
(D/'graph.cpp').write_text(s)
(D/'manifest.json').write_text(json.dumps({'native':str(N),'native_sha256':hashlib.sha256(native.encode()).hexdigest(),'owned_methods':[h[0] for h in hooks],'rollback':'GGML_GOOSE_HYBRID_INPUT=0','explicit_residuals':['child KV tensor-index/mask/rotation construction and filling APIs','native typed graph-result/input object storage and container destruction','non-Apex graph/input methods retained for rollback/backlog'],'model_gate':'PENDING_COMPLETE_BATCH_A'},indent=2)+'\n')
print('Prepared real Goose hybrid graph-input creation/fill/reuse with rollback.')
