from pathlib import Path
import json,re
B=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');S=B/'source/apex-model';a=json.loads((S/'abi.json').read_text())
(S/'binding-owner.hpp').write_text('''#pragma once
#include "models.h"
#include "bridge.h"
struct ApexLoadOwner {llama_model_base & model;llama_model_loader & loader;int trunk;uint32_t interval=4;};
inline bool fg153_apex_enabled(const llama_model &m){const char *v=std::getenv("FG153_APEX_MODEL");return (!v||std::strcmp(v,"0")!=0)&&m.arch==LLM_ARCH_QWEN35MOE&&m.name=="Apex-35B-V1.0"&&m.hparams.n_layer()==40&&m.hparams.n_embd==2048;}
''')
mp={
 'tok_embd':('LLM_TENSOR_TOKEN_EMBD','weight'),'output':('LLM_TENSOR_OUTPUT','weight'),'output_norm':('LLM_TENSOR_OUTPUT_NORM','weight'),
 'attn_norm':('LLM_TENSOR_ATTN_NORM','weight'),'attn_post_norm':('LLM_TENSOR_ATTN_POST_NORM','weight'),'wqkv':('LLM_TENSOR_ATTN_QKV','weight'),'wqkv_b':('LLM_TENSOR_ATTN_QKV','bias'),'wqkv_gate':('LLM_TENSOR_ATTN_GATE','weight'),'wo':('LLM_TENSOR_ATTN_OUT','weight'),'attn_q_norm':('LLM_TENSOR_ATTN_Q_NORM','weight'),'attn_k_norm':('LLM_TENSOR_ATTN_K_NORM','weight'),
 'ssm_conv1d':('LLM_TENSOR_SSM_CONV1D','weight'),'ssm_dt':('LLM_TENSOR_SSM_DT','bias'),'ssm_a':('LLM_TENSOR_SSM_A_NOSCAN',None),'ssm_beta':('LLM_TENSOR_SSM_BETA','weight'),'ssm_alpha':('LLM_TENSOR_SSM_ALPHA','weight'),'ssm_norm':('LLM_TENSOR_SSM_NORM','weight'),'ssm_out':('LLM_TENSOR_SSM_OUT','weight'),
}
for x in ['ffn_gate_inp','ffn_down_exps','ffn_gate_up_exps','ffn_gate_exps','ffn_up_exps','ffn_gate_inp_shexp','ffn_gate_shexp','ffn_up_shexp','ffn_down_shexp']:mp[x]=('LLM_TENSOR_'+x.upper(),'weight')
for x in ['q','k','v']:
 mp['w'+x]=('LLM_TENSOR_ATTN_'+x.upper(),'weight');mp['w'+x+'_b']=('LLM_TENSOR_ATTN_'+x.upper(),'bias')
f=['#include <cstdlib>','#include <cstring>','#include "binding-owner.hpp"','static ApexLoadOwner &b(int64_t p){GGML_ASSERT(p);return *(ApexLoadOwner *)(intptr_t)p;}','extern "C" int64_t fg153_am_lget(int64_t e,int64_t k){auto &v=b(e);auto &m=v.model;auto &h=m.hparams;if(k>=100){GGML_ASSERT(k-100<(int64_t)h.n_layer());return h.is_recr(k-100);}switch(k){']
values=['h.n_embd','m.vocab.n_tokens()','h.n_layer()','h.n_expert','h.n_expert_used()','h.n_ff()','h.n_ff_exp()','h.n_ff_shexp','h.n_embd_head_k()','h.n_head()','h.n_embd_k_gqa()','h.n_embd_v_gqa()','h.ssm_d_state','h.ssm_n_group','h.ssm_dt_rank','h.ssm_d_conv','v.trunk']
for i,x in enumerate(values):f.append(f'case {i}:return {x};')
for i,x in {20:'m.TENSOR_NOT_REQUIRED',21:'m.TENSOR_SKIP',22:'m.TENSOR_SKIP_IF_VIRTUAL',23:'m.TENSOR_DUPLICATED',30:'h.n_layer_all',31:'v.interval'}.items():f.append(f'case {i}:return {x};')
f+=['default:GGML_ABORT("invalid Apex loader projection");}}','extern "C" void fg153_am_recurrent(int64_t e,int64_t il,bool value){auto &h=b(e).model.hparams;GGML_ASSERT(il>=0&&il<h.n_layer_all);h.is_recr_impl[il]=value;}','extern "C" int64_t fg153_am_bind(int64_t e,int64_t k,int64_t il,int64_t nd,int64_t d0,int64_t d1,int64_t d2,int64_t flags){auto &v=b(e);auto &m=v.model;GGML_ASSERT(il>=-1&&il<(int64_t)m.layers.size());GGML_ASSERT(nd>=1&&nd<=3&&d0>0&&d1>0&&d2>0);ggml_tensor **slot=nullptr;llm_tensor kind=LLM_TENSOR_TOKEN_EMBD;const char *suffix="weight";switch(k){']
for name,(kind,suffix) in mp.items():
 field='m.'+name if name in ['tok_embd','output','output_norm'] else f'm.layers.at(il).{name}'
 f.append(f'case {a["weights"].index(name)}:slot=&{field};kind={kind};suffix='+('nullptr' if suffix is None else json.dumps(suffix))+';break;')
f+=['default:GGML_ABORT("unsupported Apex binding slot");}','if(k=='+str(a['weights'].index('output'))+'&&(flags&m.TENSOR_DUPLICATED))kind=LLM_TENSOR_TOKEN_EMBD;','auto tn=il<0?m.tn(kind,suffix):m.tn(kind,suffix,il);ggml_tensor *out=nullptr;if(nd==1)out=m.create_tensor(tn,{d0},flags);if(nd==2)out=m.create_tensor(tn,{d0,d1},flags);if(nd==3)out=m.create_tensor(tn,{d0,d1,d2},flags);*slot=out;return (int64_t)(intptr_t)out;}']
(S/'binding-services.cpp').write_text('\n'.join(f)+'\n')
# Hash-bound copied source integration, original branches preserved for rollback.
q=(S/'native-qwen35moe.cpp').read_text();orig=q
q=q.replace('#include "models.h"','#include "models.h"\n#include <cstdlib>\n#include <cstring>\n#include "owner.hpp"\n#include "binding-owner.hpp"',1)
loop='''        for (uint32_t i = 0; i < hparams.n_layer_all; ++i) {
            hparams.is_recr_impl[i] = (i < hparams.n_layer()) && ((i + 1) % full_attn_interval != 0);
        }'''
q=q.replace(loop,'''        if (fg153_apex_enabled(*this)) {
            ApexLoadOwner owner{*this, ml, 0, full_attn_interval};
            fg153_apex_bind((int64_t)(intptr_t)&owner, 1);
        } else {
'''+loop+'''\n        }''')
start=q.index('    tok_embd = create_tensor(');end=q.index('    auto load_block_mtp =',start);native=q[start:end]
q=q[:start]+q[end:]
trunk='''    for (int i = 0; i < n_layer; ++i) {
        load_block_trunk(i, trunk_flags);
    }'''
q=q.replace(trunk,'''    if (fg153_apex_enabled(*this)) {
        ApexLoadOwner owner{*this, ml, trunk_flags};
        fg153_apex_bind((int64_t)(intptr_t)&owner, 0);
    } else {
'''+native+trunk+'''\n    }''',1)
needle='''    return std::make_unique<graph>(*this, params);'''
q=q.replace(needle,'''    if (fg153_apex_enabled(*this)) {
        return std::make_unique<ApexOwner>(*this, params);
    }
'''+needle,1)
(S/'qwen35moe.cpp').write_text(q)
import difflib
(S/'integration.patch').write_text(''.join(difflib.unified_diff(orig.splitlines(True),q.splitlines(True),fromfile='a/src/models/qwen35moe.cpp',tofile='b/src/models/qwen35moe.cpp')))
