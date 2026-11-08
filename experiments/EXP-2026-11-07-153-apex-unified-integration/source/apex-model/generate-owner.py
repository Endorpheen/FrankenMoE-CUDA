from pathlib import Path
import json
B=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');S=B/'source/apex-model';a=json.loads((S/'abi.json').read_text())
(S/'owner.hpp').write_text('''#pragma once
#include "models.h"
#include "bridge.h"
struct ApexOwner final : llm_graph_context {
 const llama_model & model;
 std::array<int,4> rope_sections;
 std::array<ggml_tensor *,8> probe={};
 std::unique_ptr<llm_graph_input_embd> pending_embedding;
 std::vector<llm_graph_input_ptr> pending_inputs;
 explicit ApexOwner(const llama_model & m,const llm_graph_params & p,bool execute=true):llm_graph_context(p),model(m),rope_sections(hparams.rope_sections) {if(execute)fg153_apex_build((int64_t)(intptr_t)this);}
};
''')
f=['#include "owner.hpp"','#include "llama-memory-recurrent.h"','#include "llama-kv-cache.h"','#include "llama-adapter.h"','#include "llama-moe-cache.h"','#include "llama-impl.h"','#include <cmath>','#include <limits>','#include <stdexcept>','#include <iterator>','static int64_t h(const void *p){return (int64_t)(intptr_t)p;}','static ggml_tensor *t(int64_t p){return (ggml_tensor *)(intptr_t)p;}','static ApexOwner &o(int64_t p){GGML_ASSERT(p);return *(ApexOwner *)(intptr_t)p;}','static llm_graph_input_attn_kv *ai(int64_t p){return (llm_graph_input_attn_kv *)(intptr_t)p;}','static llm_graph_input_rs *ri(int64_t p){return (llm_graph_input_rs *)(intptr_t)p;}','static llm_graph_input_embd *ei(int64_t p){return (llm_graph_input_embd *)(intptr_t)p;}']
f+=['extern "C" int64_t fg153_am_get(int64_t e,int64_t k,int64_t i){auto &g=o(e);switch(k){']
for i,x in enumerate(a['fields']):f.append(f'case {i}:return g.{x};')
f+=['default:GGML_ABORT("invalid Apex metadata field");}}','extern "C" float fg153_am_fget(int64_t e,int64_t k,int64_t){auto &g=o(e);switch(k){']
for i,x in enumerate(a['floats']):f.append(f'case {i}:return g.{x};')
f+=['default:GGML_ABORT("invalid Apex float field");}}','extern "C" int64_t fg153_am_wt(int64_t e,int64_t il,int64_t k){const auto &m=o(e).model;GGML_ASSERT(il>=-1&&il<(int64_t)m.layers.size());switch(k){']
for i,x in enumerate(a['weights']):f.append(f'case {i}:return h(m.'+(x if x in ['tok_embd','output','output_norm','output_s'] else f'layers.at(il).{x}')+');')
f+=['default:GGML_ABORT("invalid Apex tensor binding");}}','extern "C" void fg153_am_put(int64_t e,int64_t k,int64_t i,int64_t v){auto &g=o(e);switch(k){case 0:GGML_ASSERT(i>=0&&i<g.n_layer);g.res->t_layer_inp[i]=t(v);break;case 1:g.res->t_h_nextn=t(v);break;case 2:g.res->t_embd=t(v);break;case 3:g.res->t_logits=t(v);break;case 4:g.res->t_inp_tokens=t(v);break;case 5:g.res->t_inp_embd=t(v);break;case 6:GGML_ASSERT(i>=0&&i<8);g.probe[i]=t(v);break;default:GGML_ABORT("invalid Apex result slot");}}']
f.append('static const char *labels[]={'+','.join(json.dumps(x) for x in a['labels'])+'};')
f+=['extern "C" void fg153_am_observe(int64_t e,int64_t v,int64_t k,int64_t il){GGML_ASSERT(k>=0&&k<(int64_t)(sizeof(labels)/sizeof(*labels)));o(e).cb(t(v),labels[k],il);}',
'extern "C" void fg153_am_fused(int64_t e,int64_t k,int64_t v,int64_t il){o(e).res->add_fused_node({(llm_fused_op)k,t(v),(int)il});}',
'extern "C" int64_t fg153_am_ne(int64_t p,int64_t i){GGML_ASSERT(p&&i>=0&&i<4);return t(p)->ne[i];}',
'extern "C" int64_t fg153_am_nb(int64_t p,int64_t i){GGML_ASSERT(p&&i>=0&&i<4);return t(p)->nb[i];}',
'extern "C" int64_t fg153_am_src(int64_t p,int64_t i){GGML_ASSERT(p&&i>=0&&i<GGML_MAX_SRC);return h(t(p)->src[i]);}',
'extern "C" int64_t fg153_am_typ(int64_t p){return t(p)->type;}',
'extern "C" int64_t fg153_am_elem(int64_t p){return ggml_element_size(t(p));}',
'extern "C" int64_t fg153_am_row(int64_t p,int64_t n){return ggml_row_size(t(p)->type,n);}',
'extern "C" float fg153_am_fsqrt(float x){return sqrtf(x);}',
'extern "C" float fg153_am_infinity(){return std::numeric_limits<float>::infinity();}',
'extern "C" float fg153_am_layer_float(int64_t e,int64_t k,int64_t il){GGML_ASSERT(il>=0&&il<o(e).n_layer);return k==0?o(e).hparams.swiglu_clamp_shexp[il]:o(e).hparams.swiglu_clamp_exp[il];}',
'extern "C" int64_t fg153_am_layer_int(int64_t e,int64_t k,int64_t il){GGML_ASSERT(k==0&&il>=0&&il<o(e).n_layer);return o(e).hparams.n_expert_used(il);}',
'static auto la(ApexOwner &g,int64_t i){GGML_ASSERT(i>=0&&i<(int64_t)g.loras->size());auto it=g.loras->begin();std::advance(it,i);return it;}',
'extern "C" float fg153_am_adapter_scale(int64_t e,int64_t w,int64_t i,int64_t id){auto &g=o(e);auto it=la(g,i);auto *lw=it->first->get_weight(t(w));GGML_ASSERT(lw);if(id){float alpha=it->first->alpha;return alpha?it->second*alpha/(float)lw->b->ne[0]:it->second;}return lw->get_scale(it->first->alpha,it->second);}',

'extern "C" void fg153_am_putin(int64_t,int64_t inp,int64_t k,int64_t v){auto *p=ei(inp);switch(k){case 0:p->tokens=t(v);break;case 1:p->embd=t(v);break;case 2:p->mixed_tokens=t(v);break;case 3:p->mixed_slots=t(v);break;case 4:p->mixed_embd=t(v);break;case 5:p->scale_rows=t(v);break;default:GGML_ABORT("invalid embedding input slot");}}',
'extern "C" void fg153_am_input_scale(int64_t,int64_t inp,float scale){ei(inp)->scale_tok=scale;}',
'extern "C" int64_t fg153_am_select_input(int64_t e,int64_t a,int64_t b,int64_t c,int64_t n,int64_t idx){ggml_tensor *inputs[]={t(a),t(b),t(c)};GGML_ASSERT(n==2||n==3);GGML_ASSERT(ggml_are_same_shape(inputs[0],inputs[1])&&ggml_are_same_stride(inputs[0],inputs[1]));if(n==3)GGML_ASSERT(ggml_are_same_shape(inputs[0],inputs[2])&&ggml_are_same_stride(inputs[0],inputs[2]));return h(ggml_build_forward_select(o(e).gf,inputs,n,idx));}',
'''extern "C" int64_t fg153_am_service(int64_t e,int64_t k,int64_t a,int64_t b,int64_t c){auto &g=o(e);switch(k){
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
case 44:return g.ubatch.token?g.ubatch.n_tokens:0;
case 45:return g.ubatch.is_mixed()?2:g.ubatch.token?0:1;
case 46:GGML_ASSERT(g.pending_embedding.get()==ei(a));g.res->add_input(std::move(g.pending_embedding));return 0;

default:GGML_ABORT("invalid Apex owner service");}}''']
# The embedding mixed-path token row count is exactly the upstream helper.
f[-1]=f[-1].replace('case 44:return g.ubatch.token?g.ubatch.n_tokens:0;','case 44:{if(!g.ubatch.is_mixed())return g.ubatch.n_tokens;int64_t n=0;for(uint32_t i=0;i<g.ubatch.n_tokens;++i)n+=!g.ubatch.type[i];return n;}')
(S/'services.cpp').write_text('\n'.join(f)+'\n')
g=(B/'generated/apex-model/kernel.c').read_text()
# The entry is discovered from main's first internal call, rather than guessing IDs.
main=g[g.index('static void gg_function_0(int64_t base) {'):];entry=__import__('re').search(r'(gg_function_\d+)\(.*?\)',main)[1]
# The first match is the main declaration itself; its body calls the exported entry.
entry=__import__('re').search(r'\b(gg_function_\d+)\(t\d+,',main)[1]
pair=__import__('re').search(r'typedef struct \{\s*int64_t f0;\s*int64_t f1;\s*\} (gg_type_\d+)',g)
print('entry',entry,'pair',pair[1] if pair else 'see layout')
unit='#include "bridge.h"\n#include <stdatomic.h>\nstatic _Atomic uint64_t build_calls,bind_calls,policy_calls;\n#define main fg153_apex_unused_main\n#include "../../generated/apex-model/kernel.c"\n#undef main\n'
unit+='int64_t fg153_apex_build(int64_t owner){atomic_fetch_add_explicit(&build_calls,1,memory_order_relaxed);return '+entry+'(owner,0);}\n'
unit+='_Static_assert(sizeof(gg_type_6)==16,"Pair layout");\n_Static_assert(_Alignof(gg_type_6)==1,"Pair packing");\n_Static_assert(offsetof(gg_type_6,f1)==8,"Pair second field");\n_Static_assert(sizeof(gg_type_7)==24,"Triple layout");\n_Static_assert(sizeof(gg_type_9)==2048,"Expert array layout");\n'
calls=__import__('re').findall(r'\b(gg_function_\d+)\(t\d+,',main)
unit+='void fg153_apex_bind(int64_t env,int64_t mode){atomic_fetch_add_explicit(mode?&policy_calls:&bind_calls,1,memory_order_relaxed);'+calls[2]+'(env,mode,0); }\n'
unit+='int64_t fg153_apex_probe(int64_t owner,int64_t mode){return '+calls[1]+'(owner,mode,0); }\n'
unit+='uint64_t fg153_apex_counts(int64_t mode){return atomic_load_explicit(mode==0?&build_calls:mode==1?&bind_calls:&policy_calls,memory_order_relaxed); }\n__attribute__((destructor))static void fg153_apex_report(void){uint64_t b=fg153_apex_counts(0),t=fg153_apex_counts(1),p=fg153_apex_counts(2);if(b||t||p)fprintf(stderr,\"FG153_APEX_MODEL build=%llu bind=%llu recurrent_policy=%llu\\n\",(unsigned long long)b,(unsigned long long)t,(unsigned long long)p); }\n'
(S/'unit.c').write_text(unit)
