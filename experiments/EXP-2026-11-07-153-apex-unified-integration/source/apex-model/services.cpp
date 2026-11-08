#include "owner.hpp"
#include "llama-memory-recurrent.h"
#include "llama-kv-cache.h"
#include "llama-adapter.h"
#include "llama-moe-cache.h"
#include "llama-impl.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <iterator>
static int64_t h(const void *p){return (int64_t)(intptr_t)p;}
static ggml_tensor *t(int64_t p){return (ggml_tensor *)(intptr_t)p;}
static ApexOwner &o(int64_t p){GGML_ASSERT(p);return *(ApexOwner *)(intptr_t)p;}
static llm_graph_input_attn_kv *ai(int64_t p){return (llm_graph_input_attn_kv *)(intptr_t)p;}
static llm_graph_input_rs *ri(int64_t p){return (llm_graph_input_rs *)(intptr_t)p;}
static llm_graph_input_embd *ei(int64_t p){return (llm_graph_input_embd *)(intptr_t)p;}
extern "C" int64_t fg153_am_get(int64_t e,int64_t k,int64_t i){auto &g=o(e);switch(k){
case 0:return g.ubatch.equal_seqs();
case 1:return g.hparams.n_embd_head_v();
case 2:return g.hparams.n_embd_head_k();
case 3:return g.hparams.n_embd_s();
case 4:return g.hparams.n_embd_r();
case 5:return g.hparams.is_recr(i);
case 6:return g.n_layer;
case 7:return g.ubatch.n_seqs;
case 8:return g.ubatch.n_seq_tokens;
case 9:return g.n_head;
case 10:return g.n_head_kv;
case 11:return g.n_tokens;
case 12:return g.n_rot;
case 13:return g.rope_type;
case 14:return g.n_ctx_orig;
case 15:return g.hparams.ssm_d_inner;
case 16:return g.hparams.ssm_d_state;
case 17:return g.hparams.ssm_n_group;
case 18:return g.hparams.ssm_dt_rank;
case 19:return g.ubatch.n_tokens;
case 20:return g.cparams.fused_gdn_ar;
case 21:return g.cparams.fused_gdn_ch;
case 22:return g.n_expert;
case 23:return g.n_expert_used;
case 24:return g.cparams.n_rs_seq;
case 25:return g.cparams.embeddings_nextn;
case 26:return g.cparams.embeddings_nextn_masked;
case 27:return g.hparams.n_expert_groups;
case 28:return g.cparams.training;
case 29:return g.cparams.flash_attn;
case 30:return g.hparams.attn_soft_cap;
case 31:return g.cparams.offload_kqv;
case 32:return g.hparams.n_embd_inp();
case 33:return g.hparams.n_embd;
case 34:return g.cparams.ctx_type;
case 35:return g.hparams.n_deepstack_layers;
case 36:return g.hparams.n_pos_per_embd();
case 37:return g.n_outputs;
default:GGML_ABORT("invalid Apex metadata field");}}
extern "C" float fg153_am_fget(int64_t e,int64_t k,int64_t){auto &g=o(e);switch(k){
case 0:return g.freq_base;
case 1:return g.freq_scale;
case 2:return g.ext_factor;
case 3:return g.attn_factor;
case 4:return g.beta_fast;
case 5:return g.beta_slow;
case 6:return g.hparams.f_attention_scale;
case 7:return g.hparams.f_norm_rms_eps;
case 8:return g.hparams.expert_weights_scale;
case 9:return g.hparams.f_attn_logit_softcapping;
case 10:return g.hparams.f_max_alibi_bias;
case 11:return g.hparams.f_embedding_scale;
default:GGML_ABORT("invalid Apex float field");}}
extern "C" int64_t fg153_am_wt(int64_t e,int64_t il,int64_t k){const auto &m=o(e).model;GGML_ASSERT(il>=-1&&il<(int64_t)m.layers.size());switch(k){
case 0:return h(m.tok_embd);
case 1:return h(m.layers.at(il).attn_norm);
case 2:return h(m.layers.at(il).attn_post_norm);
case 3:return h(m.output_norm);
case 4:return h(m.output_s);
case 5:return h(m.output);
case 6:return h(m.layers.at(il).ffn_gate_inp_shexp);
case 7:return h(m.layers.at(il).wqkv);
case 8:return h(m.layers.at(il).wqkv_s);
case 9:return h(m.layers.at(il).wqkv_gate);
case 10:return h(m.layers.at(il).wqkv_gate_s);
case 11:return h(m.layers.at(il).attn_q_norm);
case 12:return h(m.layers.at(il).attn_k_norm);
case 13:return h(m.layers.at(il).wo);
case 14:return h(m.layers.at(il).wo_s);
case 15:return h(m.layers.at(il).ssm_beta);
case 16:return h(m.layers.at(il).ssm_beta_s);
case 17:return h(m.layers.at(il).ssm_alpha);
case 18:return h(m.layers.at(il).ssm_alpha_s);
case 19:return h(m.layers.at(il).ssm_dt);
case 20:return h(m.layers.at(il).ssm_a);
case 21:return h(m.layers.at(il).ssm_conv1d);
case 22:return h(m.layers.at(il).ssm_norm);
case 23:return h(m.layers.at(il).ssm_out);
case 24:return h(m.layers.at(il).ssm_out_s);
case 25:return h(m.layers.at(il).ffn_gate_inp);
case 26:return h(m.layers.at(il).ffn_up_exps);
case 27:return h(m.layers.at(il).ffn_gate_exps);
case 28:return h(m.layers.at(il).ffn_down_exps);
case 29:return h(m.layers.at(il).ffn_gate_up_exps);
case 30:return h(m.layers.at(il).ffn_up_exps_s);
case 31:return h(m.layers.at(il).ffn_gate_exps_s);
case 32:return h(m.layers.at(il).ffn_down_exps_s);
case 33:return h(m.layers.at(il).ffn_up_shexp);
case 34:return h(m.layers.at(il).ffn_up_shexp_s);
case 35:return h(m.layers.at(il).ffn_gate_shexp);
case 36:return h(m.layers.at(il).ffn_gate_shexp_s);
case 37:return h(m.layers.at(il).ffn_down_shexp);
case 38:return h(m.layers.at(il).ffn_down_shexp_s);
case 39:return h(m.layers.at(il).wqkv_b);
case 40:return h(m.layers.at(il).wq_b);
case 41:return h(m.layers.at(il).wk_b);
case 42:return h(m.layers.at(il).wv_b);
case 43:return h(m.layers.at(il).wq);
case 44:return h(m.layers.at(il).wq_s);
case 45:return h(m.layers.at(il).wk);
case 46:return h(m.layers.at(il).wk_s);
case 47:return h(m.layers.at(il).wv);
case 48:return h(m.layers.at(il).wv_s);
default:GGML_ABORT("invalid Apex tensor binding");}}
extern "C" void fg153_am_put(int64_t e,int64_t k,int64_t i,int64_t v){auto &g=o(e);switch(k){case 0:GGML_ASSERT(i>=0&&i<g.n_layer);g.res->t_layer_inp[i]=t(v);break;case 1:g.res->t_h_nextn=t(v);break;case 2:g.res->t_embd=t(v);break;case 3:g.res->t_logits=t(v);break;case 4:g.res->t_inp_tokens=t(v);break;case 5:g.res->t_inp_embd=t(v);break;case 6:GGML_ASSERT(i>=0&&i<8);g.probe[i]=t(v);break;default:GGML_ABORT("invalid Apex result slot");}}
static const char *labels[]={"model.input_embed","attn_norm","attn_residual","attn_post_norm","ffn_out","post_moe","l_out","h_nextn","result_norm","result_output","linear_attn_qkv_mixed","z","Qcur_full","Kcur","Vcur","Qcur_reshaped","Qcur_normed","Kcur_normed","gate_reshaped","Qcur","attn_pregate","gate_sigmoid","attn_gated","attn_output","beta","beta_sigmoid","alpha","a_softplus","gate","state_predelta","conv_output_raw","conv_output_silu","q_conv","k_conv","v_conv","q_conv_predelta","k_conv_predelta","v_conv_predelta","linear_attn_out","ffn_moe_out","ffn_shexp","shared_expert_gate","shared_expert_gate_sigmoid","ffn_shexp_gated","q_in","k_in","v_in","b_in","g_in","v_b","k_b","g_cs","decay_mask","kq","attn","dnet_add_ch_lhs","attn_pre_solve","dnet_add_ch_attn_solved","k_beta_g_exp","k_cumdecay","g_last","g_last_exp_t","g_diff","key_gdiff","key_gdiff_t","dnet_add_ch_state","v_prime","v_t_new","v_attn","attn_inter","dnet_add_ch_attn_out","output_state","dnet_add_ar_state","conv_states","conv_states_reshaped","qkv_mixed_transposed","conv_input","conv_state_last","conv_state_update","new_state","norm","norm_w","wqkv","wqkv_b","ffn_up","ffn_up_s","ffn_gate","ffn_gate_s","ffn_up_clamped","ffn_silu","ffn_silu_clamped","ffn_swiglu_limited","ffn_swiglu","ffn_down_s","ffn_moe_logits","ffn_moe_probs","ffn_moe_argsort","ffn_moe_topk","ffn_moe_weights","ffn_moe_weights_sum","ffn_moe_weights_sum_clamped","ffn_moe_weights_norm","ffn_moe_weights_scaled","ffn_moe_gate_up","ffn_moe_gate_up_scaled","ffn_moe_gate","ffn_moe_up","ffn_moe_up_scaled","ffn_moe_gate_scaled","ffn_moe_up_clamped","ffn_moe_silu","ffn_moe_silu_clamped","ffn_moe_swiglu_limited","ffn_moe_swiglu","ffn_moe_down","ffn_moe_down_scaled","ffn_moe_weighted","kqv_out","kq_scaled_1","kq_tanh","kq_scaled_2","kq_soft_max","v_cont","kqv","inp_tokens","inp_embd","inp_mixed_tokens","inp_mixed_slots","inp_mixed_embd","inp_scale_rows","embd","ffn_moe_slots","inp_pos"};
extern "C" void fg153_am_observe(int64_t e,int64_t v,int64_t k,int64_t il){GGML_ASSERT(k>=0&&k<(int64_t)(sizeof(labels)/sizeof(*labels)));o(e).cb(t(v),labels[k],il);}
extern "C" void fg153_am_fused(int64_t e,int64_t k,int64_t v,int64_t il){o(e).res->add_fused_node({(llm_fused_op)k,t(v),(int)il});}
extern "C" int64_t fg153_am_ne(int64_t p,int64_t i){GGML_ASSERT(p&&i>=0&&i<4);return t(p)->ne[i];}
extern "C" int64_t fg153_am_nb(int64_t p,int64_t i){GGML_ASSERT(p&&i>=0&&i<4);return t(p)->nb[i];}
extern "C" int64_t fg153_am_src(int64_t p,int64_t i){GGML_ASSERT(p&&i>=0&&i<GGML_MAX_SRC);return h(t(p)->src[i]);}
extern "C" int64_t fg153_am_typ(int64_t p){return t(p)->type;}
extern "C" int64_t fg153_am_elem(int64_t p){return ggml_element_size(t(p));}
extern "C" int64_t fg153_am_row(int64_t p,int64_t n){return ggml_row_size(t(p)->type,n);}
extern "C" float fg153_am_fsqrt(float x){return sqrtf(x);}
extern "C" float fg153_am_infinity(){return std::numeric_limits<float>::infinity();}
extern "C" float fg153_am_layer_float(int64_t e,int64_t k,int64_t il){GGML_ASSERT(il>=0&&il<o(e).n_layer);return k==0?o(e).hparams.swiglu_clamp_shexp[il]:o(e).hparams.swiglu_clamp_exp[il];}
extern "C" int64_t fg153_am_layer_int(int64_t e,int64_t k,int64_t il){GGML_ASSERT(k==0&&il>=0&&il<o(e).n_layer);return o(e).hparams.n_expert_used(il);}
static auto la(ApexOwner &g,int64_t i){GGML_ASSERT(i>=0&&i<(int64_t)g.loras->size());auto it=g.loras->begin();std::advance(it,i);return it;}
extern "C" float fg153_am_adapter_scale(int64_t e,int64_t w,int64_t i,int64_t id){auto &g=o(e);auto it=la(g,i);auto *lw=it->first->get_weight(t(w));GGML_ASSERT(lw);if(id){float alpha=it->first->alpha;return alpha?it->second*alpha/(float)lw->b->ne[0]:it->second;}return lw->get_scale(it->first->alpha,it->second);}
extern "C" void fg153_am_putin(int64_t,int64_t inp,int64_t k,int64_t v){auto *p=ei(inp);switch(k){case 0:p->tokens=t(v);break;case 1:p->embd=t(v);break;case 2:p->mixed_tokens=t(v);break;case 3:p->mixed_slots=t(v);break;case 4:p->mixed_embd=t(v);break;case 5:p->scale_rows=t(v);break;default:GGML_ABORT("invalid embedding input slot");}}
extern "C" void fg153_am_input_scale(int64_t,int64_t inp,float scale){ei(inp)->scale_tok=scale;}
extern "C" int64_t fg153_am_select_input(int64_t e,int64_t a,int64_t b,int64_t c,int64_t n,int64_t idx){ggml_tensor *inputs[]={t(a),t(b),t(c)};GGML_ASSERT(n==2||n==3);GGML_ASSERT(ggml_are_same_shape(inputs[0],inputs[1])&&ggml_are_same_stride(inputs[0],inputs[1]));if(n==3)GGML_ASSERT(ggml_are_same_shape(inputs[0],inputs[2])&&ggml_are_same_stride(inputs[0],inputs[2]));return h(ggml_build_forward_select(o(e).gf,inputs,n,idx));}
extern "C" int64_t fg153_am_service(int64_t e,int64_t k,int64_t a,int64_t b,int64_t c){auto &g=o(e);switch(k){
#ifdef FG153_APEX_TEST
case 71:{auto *out=ggml_scale(g.ctx0,t(a),1.0f);g.cb(out,c?"fixture_recurrent":"fixture_attention",b);return h(out);}
#endif
case 70:GGML_ASSERT(a>=0&&a<8);return h(g.probe[a]);
case 0:return h(g.build_inp_mem_hybrid());


case 3:return h(((llm_graph_input_mem_hybrid *)(intptr_t)a)->get_recr());
case 4:return h(((llm_graph_input_mem_hybrid *)(intptr_t)a)->get_attn());
case 5:return h(ri(a)->mctx);
case 6:return ((const llama_memory_recurrent_context *)(intptr_t)a)->get_head();
case 7:return ((const llama_memory_recurrent_context *)(intptr_t)a)->get_size();
case 8:return h(((const llama_memory_recurrent_context *)(intptr_t)a)->get_r_l(b));
case 9:return h(((const llama_memory_recurrent_context *)(intptr_t)a)->get_s_l(b));
case 10:return h(g.cvec->apply_to(g.ctx0,t(a),b));

case 12:if(g.prec_policy)g.prec_policy->apply(t(a));if(t(b)->type==GGML_TYPE_NVFP4)ggml_prec_set_acc(t(a),GGML_PREC_BF16);return 0;
case 50:return g.moe_cache!=nullptr;
case 51:return h(g.moe_cache->get_slot_map(a,b,c));
case 53:return ggml_backend_supports_op(g.moe_cache->backend(a),t(b));
case 54:ggml_backend_sched_set_tensor_backend(g.sched,t(b),g.moe_cache->backend(a));return 0;
case 60:if(g.hdmd){auto it=g.hdmd->rot.find(t(a));if(it!=g.hdmd->rot.end()){const auto &x=it->second;switch(b){case 0:return h(x.rot);case 1:return h(x.signs);case 2:return x.perm_rep;case 3:return x.perm_hd;case 4:return x.perm_nk;}}}return 0;
case 61:return h(g.res->get_hdmd_input(t(a),t(b)));
case 62:g.res->set_hdmd_input(t(a),t(b),t(c));return 0;
case 63:if(g.hdmd){auto it=g.hdmd->inv.find(t(a));if(it!=g.hdmd->inv.end())return b?h(it->second.signs):h(it->second.rot);}return 0;
case 80:{llm_graph_input_ptr input;if(a==0)input=std::make_unique<llm_graph_input_pos>(g.hparams.n_pos_per_embd(),g.hparams.rope_section_order);else input=std::make_unique<llm_graph_input_out_ids>(g.hparams,g.cparams,g.n_outputs);auto *p=input.get();g.pending_inputs.push_back(std::move(input));return h(p);}
case 81:if(c==0)((llm_graph_input_pos *)(intptr_t)a)->pos=t(b);else ((llm_graph_input_out_ids *)(intptr_t)a)->out_ids=t(b);return 0;
case 83:ggml_set_name(t(a),"out_ids");return 0;
case 84:for(auto it=g.pending_inputs.begin();it!=g.pending_inputs.end();++it){if(h(it->get())==a){g.res->add_input(std::move(*it));g.pending_inputs.erase(it);return 0;}}GGML_ABORT("Apex pending input not owned");
case 13:return g.loras->size();
case 14:{auto *lw=la(g,b)->first->get_weight(t(a));return lw?h(lw->a):0;}
case 15:{auto *lw=la(g,b)->first->get_weight(t(a));return lw?h(lw->b):0;}
case 16:GGML_ASSERT(g.moe_cache);return h(g.moe_cache->get_experts(t(a)));
case 20:GGML_ASSERT(!g.pending_embedding);g.pending_embedding=std::make_unique<llm_graph_input_embd>(g.hparams.n_embd_inp());return h(g.pending_embedding.get());
case 30:return h(ai(a)->self_k_rot);
case 31:return h(ai(a)->self_v_rot);
case 32:return h(llama_mul_mat_hadamard(g.ctx0,t(a),t(b)));
case 33:return ai(a)->mctx->get_n_kv();
case 34:return h(ai(a)->mctx->cpy_k(g.ctx0,t(b),ai(a)->get_k_idxs(),c));
case 35:return h(ai(a)->mctx->cpy_v(g.ctx0,t(b),ai(a)->get_v_idxs(),c));
case 36:return h(ai(a)->mctx->get_k(g.ctx0,b));
case 37:return h(ai(a)->mctx->get_v(g.ctx0,b));
case 38:return h(ai(a)->get_kq_mask());
case 39:ggml_backend_sched_set_tensor_backend(g.sched,t(a),g.backend_cpu);return 0;
case 40:return h(ri(a)->s_copy);
case 41:return ri(a)->mctx->get_head();
case 42:return ri(a)->mctx->get_n_rs();
case 43:return ri(a)->mctx->get_rs_z();
case 44:{if(!g.ubatch.is_mixed())return g.ubatch.n_tokens;int64_t n=0;for(uint32_t i=0;i<g.ubatch.n_tokens;++i)n+=!g.ubatch.type[i];return n;}
case 45:return g.ubatch.is_mixed()?2:g.ubatch.token?0:1;
case 46:GGML_ASSERT(g.pending_embedding.get()==ei(a));g.res->add_input(std::move(g.pending_embedding));return 0;

default:GGML_ABORT("invalid Apex owner service");}}
