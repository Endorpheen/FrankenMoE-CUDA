from pathlib import Path
import subprocess,json,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');D=A/'source/hybrid-inputs';B=A/'build/hybrid-inputs';R=A/'variants/cpu-backend/run';T=B/'run';T.mkdir(exist_ok=True)
lib=Path(json.loads((B/'candidate.json').read_text())['library'])
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise SystemExit('Refusing to replace '+str(q))
  q.symlink_to(lib if f.name in ['libllama.so','libllama.so.0','libllama.so.0.7.0'] else f.resolve())
text=(A/'source/hybrid-memory/fixture.cpp').read_text()
text=text.replace('#include "frame.hpp"','#include "../hybrid-memory/frame.hpp"\n#include "llama-graph.h"\n#include "llama-adapter.h"')
extra=r'''
static uint64_t graph_inputs(llama_model_qwen35moe &model,llama_memory_hybrid_context *memory,const llama_ubatch &batch,uint64_t h){
 llm_graph_result result(512);llama_adapter_cvec cvec;llama_adapter_loras loras;llm_graph_params p={};p.arch=LLM_ARCH_QWEN35MOE;p.hparams=model.hparams;p.cparams={};p.cparams.n_ctx=64;p.cparams.n_ctx_seq=64;p.cparams.n_seq_max=4;p.cparams.kv_unified=true;p.cparams.causal_attn=true;p.cparams.flash_attn=true;p.cparams.ctx_type=LLAMA_CONTEXT_TYPE_DEFAULT;p.ubatch=batch;p.n_outputs=batch.n_tokens;p.cvec=&cvec;p.loras=&loras;p.mctx=memory;p.res=&result;llm_graph_context graph(p);
 auto*input=graph.build_inp_mem_hybrid();CHECK(input&&result.inputs.size()==1);CHECK(input->can_reuse(p));auto changed=p;changed.ubatch.n_tokens+=1;CHECK(!input->can_reuse(changed));CHECK(input->can_reuse(p));
 auto*mask=input->inp_attn->self_kq_mask;auto old=mask->ne[2];mask->ne[2]=2;CHECK(!input->can_reuse(p));mask->ne[2]=old;CHECK(input->can_reuse(p));
 auto*buffer=ggml_backend_alloc_ctx_tensors_from_buft(result.get_ctx(),ggml_backend_cpu_buffer_type());CHECK(buffer);ggml_backend_buffer_clear(buffer,0);input->set_input(&batch);
 std::vector<ggml_tensor*> tensors={input->inp_rs->s_copy,input->inp_rs->s_copy_main,input->inp_rs->s_copy_tail,input->inp_attn->self_k_idxs,input->inp_attn->self_v_idxs,input->inp_attn->self_kq_mask};
 for(auto*t:tensors){CHECK(t);h=mix(h,t->type);h=mix(h,t->op);for(int k=0;k<4;++k){h=mix(h,t->ne[k]);h=mix(h,t->nb[k]);}h=mix(h,t->view_offs);for(const char*name=t->name;*name;++name)h=mix(h,*name);std::vector<unsigned char> bytes(ggml_nbytes(t));if(!bytes.empty())ggml_backend_tensor_get(t,bytes.data(),0,bytes.size());for(auto b:bytes)h=mix(h,b);}
 h=mix(h,input->inp_rs->head);h=mix(h,input->inp_rs->rs_z);CHECK(input->inp_attn->self_k_rot==nullptr&&input->inp_attn->self_v_rot==nullptr);ggml_backend_buffer_free(buffer);return h;
}
'''
anchor='static uint64_t run(llama_model_qwen35moe &model,bool goose,bool embd_all)'
assert text.count(anchor)==1;text=text.replace(anchor,extra+'\n'+anchor)
text=text.replace('setenv("GGML_GOOSE_HYBRID",goose?"1":"0",1);','setenv("GGML_GOOSE_HYBRID",goose?"1":"0",1);setenv("GGML_GOOSE_HYBRID_INPUT",goose?"1":"0",1);')
old='CHECK(context->apply());h=state(m,h);';new='CHECK(context->apply());h=state(m,h);h=graph_inputs(model,dynamic_cast<llama_memory_hybrid_context*>(context.get()),u,h);';assert text.count(old)==1;text=text.replace(old,new)
(D/'fixture.cpp').write_text(text)
inc=['-I'+str(D),'-I'+str(U/'src'),'-I'+str(U/'src/models'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include')]
args=['/usr/bin/g++-13','-std=c++17','-O1',*inc,str(D/'fixture.cpp'),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lllama','-lggml','-lggml-base','-lm','-o',str(B/'fixture')];records=[]
for label,cmd in [('hybrid-input-fixture-build',args),('hybrid-input-fixture',[str(B/'fixture')])]:
 env=os.environ.copy();env['LD_LIBRARY_PATH']=str(T)+':'+str(R);p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=90);records.append({'label':label,'argv':cmd,'exit_code':p.returncode});(B/'fixture-commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-5000:]);raise SystemExit(p.returncode)
summary={'status':'PASS_REAL_CPU_HYBRID_GRAPH_INPUT_DIFFERENTIAL','library':str(lib),'sha256':hashlib.sha256(lib.read_bytes()).hexdigest(),'stdout':p.stdout,'coverage':['real hybrid cache and batch context','input construction metadata/shape/stride/name/view offsets','input ownership transfer to graph result','actual CPU input buffer writes and state copy indices','reuse true and changed-token/mask shape rejection','inherited sequence/state/failure cleanup cohort'],'model_requests':0,'residual_boundaries':['child KV numerical input construction/fill APIs remain pending separate module','C++ ABI object and container ownership remain interim'],'production_changed':False,'rollback':'GGML_GOOSE_HYBRID_INPUT=0'}
(A/'results/hybrid-inputs-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(lib.name)
print('PASS real graph-input native/Goose differential; enabled in private candidate only.')
