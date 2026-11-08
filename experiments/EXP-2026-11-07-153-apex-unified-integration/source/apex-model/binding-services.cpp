#include <cstdlib>
#include <cstring>
#include "binding-owner.hpp"
static ApexLoadOwner &b(int64_t p){GGML_ASSERT(p);return *(ApexLoadOwner *)(intptr_t)p;}
extern "C" int64_t fg153_am_lget(int64_t e,int64_t k){auto &v=b(e);auto &m=v.model;auto &h=m.hparams;if(k>=100){GGML_ASSERT(k-100<(int64_t)h.n_layer());return h.is_recr(k-100);}switch(k){
case 0:return h.n_embd;
case 1:return m.vocab.n_tokens();
case 2:return h.n_layer();
case 3:return h.n_expert;
case 4:return h.n_expert_used();
case 5:return h.n_ff();
case 6:return h.n_ff_exp();
case 7:return h.n_ff_shexp;
case 8:return h.n_embd_head_k();
case 9:return h.n_head();
case 10:return h.n_embd_k_gqa();
case 11:return h.n_embd_v_gqa();
case 12:return h.ssm_d_state;
case 13:return h.ssm_n_group;
case 14:return h.ssm_dt_rank;
case 15:return h.ssm_d_conv;
case 16:return v.trunk;
case 20:return m.TENSOR_NOT_REQUIRED;
case 21:return m.TENSOR_SKIP;
case 22:return m.TENSOR_SKIP_IF_VIRTUAL;
case 23:return m.TENSOR_DUPLICATED;
case 30:return h.n_layer_all;
case 31:return v.interval;
default:GGML_ABORT("invalid Apex loader projection");}}
extern "C" void fg153_am_recurrent(int64_t e,int64_t il,bool value){auto &h=b(e).model.hparams;GGML_ASSERT(il>=0&&il<h.n_layer_all);h.is_recr_impl[il]=value;}
extern "C" int64_t fg153_am_bind(int64_t e,int64_t k,int64_t il,int64_t nd,int64_t d0,int64_t d1,int64_t d2,int64_t flags){auto &v=b(e);auto &m=v.model;GGML_ASSERT(il>=-1&&il<(int64_t)m.layers.size());GGML_ASSERT(nd>=1&&nd<=3&&d0>0&&d1>0&&d2>0);ggml_tensor **slot=nullptr;llm_tensor kind=LLM_TENSOR_TOKEN_EMBD;const char *suffix="weight";switch(k){
case 0:slot=&m.tok_embd;kind=LLM_TENSOR_TOKEN_EMBD;suffix="weight";break;
case 5:slot=&m.output;kind=LLM_TENSOR_OUTPUT;suffix="weight";break;
case 3:slot=&m.output_norm;kind=LLM_TENSOR_OUTPUT_NORM;suffix="weight";break;
case 1:slot=&m.layers.at(il).attn_norm;kind=LLM_TENSOR_ATTN_NORM;suffix="weight";break;
case 2:slot=&m.layers.at(il).attn_post_norm;kind=LLM_TENSOR_ATTN_POST_NORM;suffix="weight";break;
case 7:slot=&m.layers.at(il).wqkv;kind=LLM_TENSOR_ATTN_QKV;suffix="weight";break;
case 39:slot=&m.layers.at(il).wqkv_b;kind=LLM_TENSOR_ATTN_QKV;suffix="bias";break;
case 9:slot=&m.layers.at(il).wqkv_gate;kind=LLM_TENSOR_ATTN_GATE;suffix="weight";break;
case 13:slot=&m.layers.at(il).wo;kind=LLM_TENSOR_ATTN_OUT;suffix="weight";break;
case 11:slot=&m.layers.at(il).attn_q_norm;kind=LLM_TENSOR_ATTN_Q_NORM;suffix="weight";break;
case 12:slot=&m.layers.at(il).attn_k_norm;kind=LLM_TENSOR_ATTN_K_NORM;suffix="weight";break;
case 21:slot=&m.layers.at(il).ssm_conv1d;kind=LLM_TENSOR_SSM_CONV1D;suffix="weight";break;
case 19:slot=&m.layers.at(il).ssm_dt;kind=LLM_TENSOR_SSM_DT;suffix="bias";break;
case 20:slot=&m.layers.at(il).ssm_a;kind=LLM_TENSOR_SSM_A_NOSCAN;suffix=nullptr;break;
case 15:slot=&m.layers.at(il).ssm_beta;kind=LLM_TENSOR_SSM_BETA;suffix="weight";break;
case 17:slot=&m.layers.at(il).ssm_alpha;kind=LLM_TENSOR_SSM_ALPHA;suffix="weight";break;
case 22:slot=&m.layers.at(il).ssm_norm;kind=LLM_TENSOR_SSM_NORM;suffix="weight";break;
case 23:slot=&m.layers.at(il).ssm_out;kind=LLM_TENSOR_SSM_OUT;suffix="weight";break;
case 25:slot=&m.layers.at(il).ffn_gate_inp;kind=LLM_TENSOR_FFN_GATE_INP;suffix="weight";break;
case 28:slot=&m.layers.at(il).ffn_down_exps;kind=LLM_TENSOR_FFN_DOWN_EXPS;suffix="weight";break;
case 29:slot=&m.layers.at(il).ffn_gate_up_exps;kind=LLM_TENSOR_FFN_GATE_UP_EXPS;suffix="weight";break;
case 27:slot=&m.layers.at(il).ffn_gate_exps;kind=LLM_TENSOR_FFN_GATE_EXPS;suffix="weight";break;
case 26:slot=&m.layers.at(il).ffn_up_exps;kind=LLM_TENSOR_FFN_UP_EXPS;suffix="weight";break;
case 6:slot=&m.layers.at(il).ffn_gate_inp_shexp;kind=LLM_TENSOR_FFN_GATE_INP_SHEXP;suffix="weight";break;
case 35:slot=&m.layers.at(il).ffn_gate_shexp;kind=LLM_TENSOR_FFN_GATE_SHEXP;suffix="weight";break;
case 33:slot=&m.layers.at(il).ffn_up_shexp;kind=LLM_TENSOR_FFN_UP_SHEXP;suffix="weight";break;
case 37:slot=&m.layers.at(il).ffn_down_shexp;kind=LLM_TENSOR_FFN_DOWN_SHEXP;suffix="weight";break;
case 43:slot=&m.layers.at(il).wq;kind=LLM_TENSOR_ATTN_Q;suffix="weight";break;
case 40:slot=&m.layers.at(il).wq_b;kind=LLM_TENSOR_ATTN_Q;suffix="bias";break;
case 45:slot=&m.layers.at(il).wk;kind=LLM_TENSOR_ATTN_K;suffix="weight";break;
case 41:slot=&m.layers.at(il).wk_b;kind=LLM_TENSOR_ATTN_K;suffix="bias";break;
case 47:slot=&m.layers.at(il).wv;kind=LLM_TENSOR_ATTN_V;suffix="weight";break;
case 42:slot=&m.layers.at(il).wv_b;kind=LLM_TENSOR_ATTN_V;suffix="bias";break;
default:GGML_ABORT("unsupported Apex binding slot");}
if(k==5&&(flags&m.TENSOR_DUPLICATED))kind=LLM_TENSOR_TOKEN_EMBD;
auto tn=il<0?m.tn(kind,suffix):m.tn(kind,suffix,il);ggml_tensor *out=nullptr;if(nd==1)out=m.create_tensor(tn,{d0},flags);if(nd==2)out=m.create_tensor(tn,{d0,d1},flags);if(nd==3)out=m.create_tensor(tn,{d0,d1,d2},flags);*slot=out;return (int64_t)(intptr_t)out;}
