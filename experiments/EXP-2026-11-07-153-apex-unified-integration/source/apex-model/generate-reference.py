from pathlib import Path
import re,json,hashlib
B=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');S=B/'source/apex-model';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
graph=(S/'native-llama-graph.cpp').read_text();q=(S/'native-qwen35moe.cpp').read_text()
def function(text,start):
 pos=text.index('{',start);lev=1;i=pos+1
 # Strip only for brace counting while preserving exact original range.
 clean=re.sub(r'//[^\n]*|/\*.*?\*/',lambda m:' '*len(m[0]),text,flags=re.S)
 while lev:
  if clean[i]=='{':lev+=1
  elif clean[i]=='}':lev-=1
  i+=1
 return text[start:i]
ref=['#include "models.h"','#include "llama-impl.h"','#include "llama-adapter.h"','#include "llama-moe-cache.h"','#include "native-probe.hpp"']
manifest=[]
for name in ['build_hadamard_input','build_lora_mm','build_lora_mm_id','build_norm','build_qkv','build_ffn','build_moe_ffn','build_moe_cache_slots','build_inp_embd','build_attn_mha']:
 for match in re.finditer(r'ggml_tensor \* llm_graph_context::'+name+r'\(|llm_graph_qkv llm_graph_context::'+name+r'\(',graph):
  t=function(graph,match.start());ref.append(t);manifest.append(dict(name=name,sha256=hashlib.sha256(t.encode()).hexdigest(),source='llama-graph.cpp'))
# The upstream embedding helper is private to its translation unit.
st=graph.index('static int64_t llm_graph_n_tok_rows(');ref.insert(0,function(graph,st))
# Move includes before this static declaration.
ref=ref[1:6]+[ref[0]]+ref[6:]
st=q.index('ggml_tensor * llama_model_qwen35moe::graph::build_layer_ffn(');t=function(q,st);manifest.append(dict(name='build_layer_ffn',sha256=hashlib.sha256(t.encode()).hexdigest(),source='qwen35moe.cpp'));ref.append(t.replace('llama_model_qwen35moe::graph::','NativeProbe::',1))
mainbody=function(q,q.index('llama_model_qwen35moe::graph::graph('));mainbody=mainbody[mainbody.index('{')+1:-1]
ref.append('void NativeFlow::flow(){'+mainbody+'}')
(S/'native-reference.cpp').write_text('\n\n'.join(ref)+'\n')
(S/'native-probe.hpp').write_text('''#pragma once
#include "models.h"
struct NativeProbe:llm_graph_context {
 const llama_model &model;
 NativeProbe(const llama_model &m,const llm_graph_params &p):llm_graph_context(p),model(m){}
 ggml_tensor *build_layer_ffn(ggml_tensor *,int);
};
''')
(S/'native-reference-manifest.json').write_text(json.dumps(manifest,indent=2))
with (S/'native-probe.hpp').open('a') as f:f.write('''struct NativeFlow:NativeProbe {
 using NativeProbe::NativeProbe;
 std::unique_ptr<llm_graph_input_mem_hybrid> input;
 llm_graph_input_mem_hybrid *build_inp_mem_hybrid(){input=std::make_unique<llm_graph_input_mem_hybrid>(cparams,nullptr,nullptr,nullptr);return input.get();}
 ggml_tensor *build_layer_attn_linear(llm_graph_input_rs *,ggml_tensor *x,int il){auto *out=ggml_scale(ctx0,x,1.0f);cb(out,"fixture_recurrent",il);return out;}
 ggml_tensor *build_layer_attn(llm_graph_input_attn_kv *,ggml_tensor *x,ggml_tensor *,int *,int il){auto *out=ggml_scale(ctx0,x,1.0f);cb(out,"fixture_attention",il);return out;}
 void flow();
};
''')
