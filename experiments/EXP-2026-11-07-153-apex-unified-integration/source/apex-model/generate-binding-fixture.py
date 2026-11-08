from pathlib import Path
import json,re
B=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');S=B/'source/apex-model';a=json.loads((S/'abi.json').read_text());U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
q=(S/'native-qwen35moe.cpp').read_text();model=(U/'src/llama-model.cpp').read_text()
def body(txt,start):
 a=txt.index('{',start);lev=1;p=a+1
 while lev:
  if txt[p]=='{':lev+=1
  if txt[p]=='}':lev-=1
  p+=1
 return txt[a+1:p-1]
mp={}
# Match exactly the production name mapping from binding services, preserving output tied weights.
prod=(S/'binding-services.cpp').read_text()
for m in re.finditer(r'case (\d+):slot=.*?;kind=(\w+);suffix=(nullptr|"[^"]+");break;',prod):mp[int(m[1])]=(m[2],m[3])
head='''#include "models.h"
#include "bridge.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <array>
struct Name {llm_tensor kind;const char *suffix;int il;};
struct BindingTrace {
 llama_hparams hparams={};std::array<llama_layer,41> layers={};ggml_tensor *tok_embd=nullptr,*output=nullptr,*output_norm=nullptr;int configuration=0,trunk=0;std::vector<std::string> events;
 static constexpr int TENSOR_NOT_REQUIRED=1,TENSOR_DUPLICATED=2,TENSOR_SKIP=4,TENSOR_SKIP_IF_VIRTUAL=8;
 BindingTrace(int c,int f):configuration(c),trunk(f){auto &h=hparams;h.n_embd=2048;h.n_layer_all=41;h.n_layer_nextn=1;h.n_expert=256;h.n_expert_used_arr.fill(8);h.n_ff_arr.fill(4096);h.n_ff_exp_arr.fill(512);h.n_ff_shexp=512;h.n_head_arr.fill(16);h.n_head_kv_arr.fill(2);h.n_embd_head_k_full=256;h.n_embd_head_v_full=256;h.ssm_d_conv=4;h.ssm_d_state=128;h.ssm_n_group=16;h.ssm_dt_rank=32;for(int i=0;i<41;++i)h.is_recr_impl[i]=i<40&&(i+1)%4!=0;}
 Name tn(llm_tensor k,const char *s=nullptr,int il=-1){return {k,s,il};}Name tn(llm_tensor k,int il){return {k,nullptr,il};}
 ggml_tensor *create_tensor(Name n,std::initializer_list<int64_t> d,int flags){std::ostringstream s;s<<n.kind<<":"<<(n.suffix?n.suffix:"")<<":"<<n.il<<":"<<flags;for(auto x:d)s<<":"<<x;events.push_back(s.str());if(flags&TENSOR_SKIP)return nullptr;if(n.kind==LLM_TENSOR_OUTPUT&&(configuration&4))return nullptr;if(n.kind==LLM_TENSOR_FFN_GATE_UP_EXPS&&!(configuration&2))return nullptr;if(n.kind==LLM_TENSOR_ATTN_QKV&&n.il>=0&&(n.il+1)%4==0&&!(configuration&1))return nullptr;if(n.suffix&&std::string(n.suffix)=="bias"&&!(configuration&8))return nullptr;return (ggml_tensor *)(uintptr_t)(4096+64*events.size());}
 void create_tensor_gate_up_exps(llama_layer &,int,int64_t,int64_t,int64_t,int);
 void create_tensor_qkv(llama_layer &,int,int64_t,int64_t,int64_t,int64_t,int);
 void native();
};
'''
f=[head]
for name in ['create_tensor_gate_up_exps','create_tensor_qkv']:
 start=model.index('void llama_model_base::'+name+'(');end=model.index('{',start);f.append(model[start:end].replace('llama_model_base::','BindingTrace::')+'{'+body(model,start)+'}')
start=q.index('    tok_embd = create_tensor(');end=q.index('    auto load_block_mtp',start);native=q[start:end]
f.append('''void BindingTrace::native(){const int64_t n_embd=2048,n_vocab=248064,n_ff=4096,n_expert=256,n_expert_used=8,n_embd_head_k=256,n_head=16,n_embd_k_gqa=512,n_embd_v_gqa=512;const int trunk_flags=trunk;'''+native+'for(int i=0;i<40;++i)load_block_trunk(i,trunk_flags);}')
f+=['extern "C" int64_t fg153_am_lget(int64_t e,int64_t k){auto &v=*(BindingTrace *)(intptr_t)e;auto &h=v.hparams;if(k>=100)return h.is_recr(k-100);switch(k){']
vals=['h.n_embd','248064','h.n_layer()','h.n_expert','h.n_expert_used()','h.n_ff()','h.n_ff_exp()','h.n_ff_shexp','h.n_embd_head_k()','h.n_head()','h.n_embd_k_gqa()','h.n_embd_v_gqa()','h.ssm_d_state','h.ssm_n_group','h.ssm_dt_rank','h.ssm_d_conv','v.trunk']
for i,x in enumerate(vals):f.append(f'case {i}:return {x};')
for i,x in {20:'1',21:'4',22:'8',23:'2',30:'41',31:'4'}.items():f.append(f'case {i}:return {x};')
f+=['default:abort();}}','extern "C" void fg153_am_recurrent(int64_t e,int64_t il,bool v){((BindingTrace *)(intptr_t)e)->hparams.is_recr_impl[il]=v;}','extern "C" int64_t fg153_am_bind(int64_t e,int64_t k,int64_t il,int64_t nd,int64_t d0,int64_t d1,int64_t d2,int64_t flags){auto &v=*(BindingTrace *)(intptr_t)e;Name name={LLM_TENSOR_TOKEN_EMBD,"weight",(int)il};switch(k){']
for i,(kind,suf) in mp.items():f.append(f'case {i}:name.kind={kind};name.suffix={suf};break;')
f+=['default:abort();}','if(k=='+str(a['weights'].index('output'))+'&&(flags&2))name.kind=LLM_TENSOR_TOKEN_EMBD;ggml_tensor *out=nullptr;if(nd==1)out=v.create_tensor(name,{d0},flags);if(nd==2)out=v.create_tensor(name,{d0,d1},flags);if(nd==3)out=v.create_tensor(name,{d0,d1,d2},flags);return (int64_t)(intptr_t)out;}']
f+=['''int main(){int passed=0;for(int configuration=0;configuration<16;++configuration){for(int flags:{0,1,4}){BindingTrace n(configuration,flags),g(configuration,flags);n.native();fg153_apex_bind((int64_t)(intptr_t)&g,0);if(n.events!=g.events){std::cerr<<"binding mismatch config="<<configuration<<" flags="<<flags<<" native="<<n.events.size()<<" goose="<<g.events.size()<<"\\n";for(size_t i=0;i<std::min(n.events.size(),g.events.size());++i)if(n.events[i]!=g.events[i]){std::cerr<<i<<" "<<n.events[i]<<" / "<<g.events[i]<<"\\n";break;}return 1;}++passed;}}BindingTrace p(0,0);p.hparams.is_recr_impl.fill(false);fg153_apex_bind((int64_t)(intptr_t)&p,1);int nr=0;for(int i=0;i<41;++i){bool expected=i<40&&(i+1)%4!=0;if(p.hparams.is_recr_impl[i]!=expected)return 2;nr+=expected;}if(nr!=30)return 3;std::cout<<"PASS "<<passed<<" native/Goose binding traces and 41-layer recurrent policy (30 recurrent,10 attention,1 excluded MTP)\\n";}
''']
(S/'binding-fixture.cpp').write_text('\n'.join(f))
