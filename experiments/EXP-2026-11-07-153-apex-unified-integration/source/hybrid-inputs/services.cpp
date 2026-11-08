#include "frame.hpp"
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
