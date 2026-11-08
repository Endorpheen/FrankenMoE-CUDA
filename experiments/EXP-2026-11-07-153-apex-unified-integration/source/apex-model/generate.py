from pathlib import Path
import re,json,hashlib
B=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107')
S=B/'source/apex-model';G=B/'generated/apex-model';R=B/'results/apex-model';D=B/'build/apex-model'
for d in [S,G,R,D]: d.mkdir(parents=True,exist_ok=True)
U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
q=(U/'src/models/qwen35moe.cpp').read_text();delta=(U/'src/models/delta-net-base.cpp').read_text();graph=(U/'src/llama-graph.cpp').read_text()
for name,txt in [('qwen35moe.cpp',q),('delta-net-base.cpp',delta),('llama-graph.cpp',graph)]: (S/('native-'+name)).write_text(txt)
def body(txt,needle):
 st=txt.index(needle);a=txt.index('{',st);lev=1;p=a+1
 while lev:
  if txt[p]=='{':lev+=1
  elif txt[p]=='}':lev-=1
  p+=1
 return txt[a+1:p-1]
def strip(t):return re.sub(r'/\*.*?\*/|//[^\n]*','',t,flags=re.S)
# Stable ABI field numbers are generated from the exact captured access set.
fields=[];floats=[];weights=[];labels=[]
def field(expr,fl=False):
 expr=expr.replace('LOCAL_','')
 arr=floats if fl else fields
 if expr not in arr:arr.append(expr)
 return ('fget' if fl else 'get')+f'(e,{arr.index(expr)},0)'
def weight(x):
 if x not in weights:weights.append(x)
 return str(weights.index(x))
def label(x):
 if x not in labels:labels.append(x)
 return str(labels.index(x))
helpernames={'build_norm','build_lora_mm','build_lora_mm_id','build_qkv','build_layer_attn','build_layer_attn_linear','build_layer_ffn','build_qkvz','build_norm_gated','build_conv_state','build_recurrent_attn','build_delta_net_chunking','build_delta_net_autoregressive','build_delta_net_fused','build_delta_net','get_slice_2d','build_gdn_l2_norm','build_moe_ffn','build_ffn','build_attn','build_attn_mha','build_rs'}
prim=set()
def convert(t):
 t=strip(t)
 t=t.replace('res->t_layer_inp[il] = inpL;','put(e,0,il,inpL);')
 for i,x in enumerate(['t_h_nextn','t_embd','t_logits'],1):t=t.replace('res->'+x+' = cur;',f'put(e,{i},0,cur);')
 t=re.sub(r'res->add_fused_node\(\{(\w+),\s*(\w+),\s*il\}\);',r'fused(e,\1,\2,il);',t)
 t=t.replace('std::copy(std::begin(hparams.rope_sections), std::begin(hparams.rope_sections) + 4, sections);','')
 t=t.replace('int sections[4];','let sections:i64=0;')
 t=t.replace('auto [Qcur_full, Kcur, Vcur] =','let qkv_tuple =')
 t=t.replace('il, false);','il, false);\nvar Qcur_full:i64=qkv_tuple.a;var Kcur:i64=qkv_tuple.b;var Vcur:i64=qkv_tuple.c;')
 t=t.replace('auto * inp = build_inp_mem_hybrid();','var inp:i64=service(e,0,0,0,0);')
 t=t.replace('build_inp_embd(model.tok_embd)','input_embed(e,wt(e,-1,'+weight('tok_embd')+'))')
 t=t.replace('build_inp_pos()','service(e,1,0,0,0)').replace('build_inp_out_ids()','service(e,2,0,0,0)')
 t=t.replace('inp->get_recr()','service(e,3,inp,0,0)').replace('inp->get_attn()','service(e,4,inp,0,0)')
 t=t.replace('inp->mctx','service(e,5,inp,0,0)')
 for name,id in [('get_head',6),('get_size',7)]:t=t.replace('mctx_cur->'+name+'()',f'service(e,{id},mctx_cur,0,0)')
 for name,id in [('get_r_l',8),('get_s_l',9)]:t=t.replace('mctx_cur->'+name+'(il)',f'service(e,{id},mctx_cur,il,0)')
 t=t.replace('build_cvec(cur, il)','service(e,10,cur,il,0)')
 t=t.replace('crop_before_nextn(inp_out_ids)','crop_before(e,inp_out_ids)').replace('crop_after_nextn(inp_out_ids)','crop_after(e,inp_out_ids)')
 t=t.replace('ubatch.equal_seqs()',field('ubatch.equal_seqs()')+'!=0')
 for x in ['n_embd_head_v','n_embd_head_k','n_embd_s','n_embd_r']:t=t.replace('hparams.'+x+'()',field('hparams.'+x+'()'))
 t=t.replace('hparams.is_recr(il)','get(e,'+str(len(fields))+',il)!=0') if False else t
 t=re.sub(r'hparams\.is_recr\(il\)',lambda m:field('hparams.is_recr(i)').replace(',0)',',il)')+'!=0',t)
 t=re.sub(r'model\.layers\[il\]\.(\w+)',lambda m:'wt(e,il,'+weight(m[1])+')',t)
 for x in ['output_norm','output_s','output']:t=re.sub(r'model\.'+x+r'\b','wt(e,-1,'+weight(x)+')',t)
 t=re.sub(r'hparams\.(f_attention_scale|f_norm_rms_eps|expert_weights_scale)',lambda m:field(m[0],True),t)
 t=re.sub(r'\b(hparams|ubatch|cparams)\.(\w+)\b',lambda m:field(m[0]),t)
 t=re.sub(r'\b(n_embd_head_k|n_embd_head_v|n_head_kv|n_head|n_layer|n_tokens|n_expert_used|n_expert|n_rot|n_ctx_orig|rope_type)\b',lambda m:field(m[0]),t)
 for x in ['freq_base','freq_scale','ext_factor','attn_factor','beta_fast','beta_slow']:t=re.sub(r'\b'+x+r'\b',field(x,True),t)
 # Undo substitutions inside local declarations and generated metadata expressions is handled by explicit body locals below.
 t=t.replace('ctx0,','e,').replace('gf,','e,').replace('ctx0)','e)')
 t=re.sub(r'\b(\w+)->(ne|nb|src)\[([^]]+)\]',lambda m:f'{m[2]}({m[1]},{m[3]})',t)
 t=re.sub(r'\b(\w+)->type\b',r'typ(\1)',t)
 t=re.sub(r'ggml_row_size\(typ\((\w+)\),',r'row(\1,',t)
 t=re.sub(r'ggml_element_size\((\w+)\)',r'elem(\1)',t)
 t=re.sub(r'\bggml_(\w+)\s*\(',lambda m:(prim.add('ggml_'+m[1]) or 'p_'+m[1]+'('),t)
 t=re.sub(r'\bcb\(([^,]+),\s*"([^"]+)",',lambda m:'observe(e,'+m[1]+','+label(m[2])+',',t)
 t=re.sub(r'GGML_ASSERT\((.*?)\);',r'assert(\1);',t,flags=re.S)
 t=t.replace('nullptr','0').replace('NULL','0')
 t=t.replace('qkvz.first','qkvz.a').replace('qkvz.second','qkvz.b').replace('attn_out.first','attn_out.a').replace('attn_out.second','attn_out.b')
 t=re.sub(r'return \{\s*(\w+)\s*,\s*(\w+)\s*\};',r'return Pair { a:\1,b:\2 };',t)
 t=t.replace('const auto *','var').replace('const auto   ','var ').replace('const auto ','var ').replace('auto *','var ').replace('auto ','var ')
 t=re.sub(r'\b(?:const |constexpr )?float\s+(\w+)\s*=',r'var \1:f32=',t)
 t=re.sub(r'\b(?:const )?bool\s+(\w+)\s*=',r'var \1:bool=',t)
 t=re.sub(r'\b(?:const )?(?:int64_t|uint32_t|size_t|int)\s+(\w+)\s*=',r'var \1:i64=',t)
 t=re.sub(r'ggml_tensor\s*\*\s*(\w+)\s*=',r'var \1:i64=',t)
 t=re.sub(r'ggml_tensor\s*\*\s*(\w+)\s*;',r'var \1:i64=0;',t)
 t=re.sub(r'var\s+(\w+)\s*=',r'var \1=',t)
 t=t.replace('(int64_t) ','').replace('(size_t) ','')
 t=re.sub(r'float\((\w+)\)',r'(\1 as f32)',t)
 t=t.replace('sqrtf(','fsqrt(')
 t=re.sub(r'(\d+(?:\.\d+)?(?:e[+-]?\d+)?)f\b',r'\1',t)
 t=t.replace('std::max<int64_t>','maxi').replace('std::min<int64_t>','mini')
 t=t.replace('kda ? 16 : 64','if kda {16} else {64}')
 t=t.replace('fget(e,0,0) == 0.0 ? 1.0 / fsqrt((n_embd_head as f32)) : fget(e,0,0)','if fget(e,0,0) == 0.0 {1.0 / fsqrt(n_embd_head as f32)} else {fget(e,0,0)}')
 t=t.replace('model.layers[il], cur','il, cur')
 t=re.sub(r'for \(var (\w+):i64=\s*([^;]+);\s*\1\s*<\s*([^;]+);\s*\1\+\+\)\s*\{',r'for \1 in \3 {',t)
 # Loops requiring one-based indices get explicit offset.
 t=re.sub(r'for \(var t:i64=\s*1;\s*t <= K;\s*\+\+t\)\s*\{',r'for t0 in K { let t=t0+1;',t)
 t=re.sub(r'for \(var il:i64=\s*0;\s*il < get\(e,([0-9]+),0\);\s*\+\+il\)\s*\{',r'for il in get(e,\1,0) {',t)
 # general native tensor-pointer args become handles; helpers receive owner handle.
 for name in sorted(helpernames,key=len,reverse=True):t=re.sub(r'\b'+name+r'\(',name+'(e,',t)
 # ctx is already the owner on the two standalone helpers.
 t=t.replace('get_slice_2d(e,e,','get_slice_2d(e,').replace('build_gdn_l2_norm(e,e,','build_gdn_l2_norm(e,')
 t=t.replace('build_lora_mm(e,wt(e,il,'+weight('ffn_gate_inp_shexp')+'), cur)','build_lora_mm(e,wt(e,il,'+weight('ffn_gate_inp_shexp')+'), cur,0)')
 t=re.sub(r'for \(var il:i64=\s*0; il < (get\(e,\d+,0\)); \+\+il\) \{',r'for il in \1 {',t)
 return t
# Do not replace locals sharing context names (n_tokens in delta is tensor-derived).
def conv(t):
 # Protect declarations and all local references before converting metadata identifiers.
 locals_=set(re.findall(r'(?:const\s+)?(?:int64_t|int|float|bool)\s+(\w+)\s*=',strip(t))) & {'n_tokens','n_layer','n_head','n_head_kv','n_embd_head_k','n_embd_head_v','n_expert','n_expert_used','n_rot','n_ctx_orig','rope_type'}
 for x in locals_:t=re.sub(r'(?<![.\w])'+x+r'\b','LOCAL_'+x,t)
 t=convert(t)
 return re.sub(r'\bLOCAL_(\w+)\b',r'\1',t)
parts=[]
def add(name,params,ret,t):parts.append(f'fn {name}(e:i64{"," if params else ""}{params})->{ret} {{\n'+conv(t)+'\n}\n')
add('build_main','','i64',body(q,'llama_model_qwen35moe::graph::graph(')+'\nreturn cur;')
for name,params,ret in [
 ('build_qkvz','input:i64,il:i64','Pair'),('build_norm_gated','input:i64,weights:i64,gate:i64,layer:i64','i64'),('build_layer_attn','inp:i64,cur:i64,inp_pos:i64,sections:i64,il:i64','i64'),('build_layer_attn_linear','inp:i64,cur:i64,il:i64','i64'),('build_layer_ffn','cur:i64,il:i64','i64')]:
 add(name,params,ret,body(q,'llama_model_qwen35moe::graph::'+name+'('))
for name in ['chunking','autoregressive','fused','']:
 n='build_delta_net'+('_'+name if name else '')
 add(n,'q:i64,k:i64,v:i64,g:i64,b:i64,s:i64,il:i64','Pair',body(delta,'llm_build_delta_net_base::'+n+'('))
add('get_slice_2d','t:i64,c:i64','i64',body(delta,'static ggml_tensor * get_slice_2d('))
add('build_conv_state','inp:i64,conv_states_all:i64,qkv_mixed:i64,conv_kernel_size:i64,conv_channels:i64,il:i64','i64',body(delta,'llm_build_delta_net_base::build_conv_state('))
add('build_recurrent_attn','inp:i64,ssm_states_all:i64,q:i64,k:i64,v:i64,g:i64,b:i64,s:i64,il:i64','i64',body(delta,'llm_build_delta_net_base::build_recurrent_attn('))
# The pure input storage object is projected through the C++ adapter; the graph policy is Goose.
flow=parts[0].replace('fn build_main(','fn flow_main(').replace('var inp:i64=service(e,0,0,0,0);','var inp:i64=0;')
flow=flow.replace('build_layer_attn_linear(e,service(e,3,inp,0,0), cur, il)','service(e,71,cur,il,1)').replace('build_layer_attn(e,service(e,4,inp,0,0), cur, inp_pos, sections, il)','service(e,71,cur,il,0)')
text='\n'.join(parts)+'\n'+flow
(S/'translated.goose').write_text(text)
(S/'abi.json').write_text(json.dumps(dict(fields=fields,floats=floats,weights=weights,labels=labels,primitives=sorted(prim)),indent=2))
print('wrote',len(text.splitlines()),'translated lines')
ht=Path(__file__).with_name('helpers.tpl').read_text()
ht=re.sub(r'@([^@]+)@',lambda m:field(m[1]),ht)
ht=re.sub(r'#([^#]+)#',lambda m:field(m[1],True),ht)
ht=re.sub(r'%([^%]+)%',lambda m:weight(m[1]),ht)
ht=re.sub(r'obs\(e,([^\n]*?),"([^"]+)",',lambda m:'observe(e,'+m[1]+','+label(m[2])+',',ht)
bt=Path(__file__).with_name('bind.tpl').read_text()
bt=re.sub(r'%([^%]+)%',lambda m:weight(m[1]),bt)
text=text+'\n'+ht+'\n'+bt
text=text.replace('service(e,1,0,0,0)','position_input(e)').replace('service(e,2,0,0,0)','outid_input(e)')
for n,v in {'LLM_NORM_RMS':1,'LLM_FFN_SILU':1,'LLM_FFN_PAR':1,'LLAMA_EXPERT_GATING_FUNC_TYPE_SOFTMAX':1,'LLM_FUSED_OP_GDN_AR':1,'LLM_FUSED_OP_GDN_CH':2,'GGML_TRI_TYPE_LOWER_DIAG':2,'GGML_TRI_TYPE_LOWER':3}.items():text=re.sub(r'\b'+n+r'\b',str(v),text)
text=re.sub(r'(fget\(e,\d+,0\)) == 0.0 \? 1.0 / fsqrt\(\(n_embd_head as f32\)\) : fget\(e,\d+,0\)',r'if \1==0.0 {1.0/fsqrt(n_embd_head as f32)} else {\1}',text)
text=re.sub(r'if \(get\(([^)]+)\)\)',r'if (get(\1)!=0)',text)
text=re.sub(r'!get\(([^)]+)\)',r'get(\1)==0',text)
text=text.replace('fsqrt(S_k)','fsqrt(S_k as f32)')
text=re.sub(r'\bpad\b','padding',text)
def mutable_params(m):
 name,params,ret=m.groups();ps=[];vs=[]
 for p in params.split(','):
  if not p:continue
  n,t=p.split(':');ps.append(n+'_arg:'+t);vs.append('var '+n+':'+t+'='+n+'_arg;')
 return 'fn '+name+'('+','.join(ps)+')->'+ret+' {'+''.join(vs)
text=re.sub(r'fn (\w+)\(([^)]*)\)->(\w+) \{',mutable_params,text)
# Native APIs are imported with exact C prototypes. Handles are explicit intptr-sized integers.
header=strip((U/'ggml/include/ggml.h').read_text())
used=sorted(set(re.findall(r'\bp_(\w+)\(',text)))
proto={}
for n in used:
 full='ggml_'+n
 m=re.search(r'GGML_API\s+([\w\s*]+?)\s*\b'+full+r'\s*\(([^;]*?)\)\s*;',header,re.S)
 if not m:raise RuntimeError('missing prototype '+full)
 ret=m[1].strip();args=[]
 for a in m[2].split(','):
  a=a.strip()
  if a=='void' or not a:continue
  z=re.match(r'(.*?)\b(\w+)\s*(\[.*?\])?$',a,re.S)
  if not z:raise RuntimeError(a)
  ty=z[1].strip();nm=z[2];ar=z[3]
  goose='f32' if ty=='float' else 'bool' if ty=='bool' else 'i64'
  args.append((ty,nm,ar,goose))
 proto[n]=dict(ret=ret,args=args)
extra=[('get','i64','e:i64,k:i64,i:i64'),('fget','f32','e:i64,k:i64,i:i64'),('wt','i64','e:i64,il:i64,k:i64'),('put','','e:i64,k:i64,i:i64,v:i64'),('service','i64','e:i64,k:i64,a:i64,b:i64,c:i64'),('observe','','e:i64,t:i64,k:i64,il:i64'),('fused','','e:i64,k:i64,t:i64,il:i64'),('ne','i64','t:i64,i:i64'),('nb','i64','t:i64,i:i64'),('src','i64','t:i64,i:i64'),('typ','i64','t:i64'),('elem','i64','t:i64'),('row','i64','t:i64,n:i64'),('adapter_scale','f32','e:i64,w:i64,i:i64,id:i64'),('layer_float','f32','e:i64,k:i64,il:i64'),('layer_int','i64','e:i64,k:i64,il:i64'),('putin','','e:i64,inp:i64,k:i64,t:i64'),('select_input','i64','e:i64,a:i64,b:i64,c:i64,n:i64,idx:i64'),('input_scale','','e:i64,inp:i64,s:f32'),('fsqrt','f32','n:f32'),('infinity','f32','')]
decl=[];cpro=['#pragma once','#include <stdint.h>','#include <stdbool.h>','#ifdef __cplusplus','extern "C" {','#endif']
for name,ret,params in extra:
 decl.append(f'extern "fg153_am_{name}" fn {name}({params})'+(f'->{ret}' if ret else '')+';')
 ca=[]
 for a in params.split(','):
  if not a:continue
  nm,ty=a.split(':');ca.append({'i64':'int64_t','f32':'float'}[ty]+' '+nm)
 cpro.append({'i64':'int64_t','f32':'float','':'void'}[ret]+f' fg153_am_{name}('+(','.join(ca) or 'void')+');')
cpp=['#include "models.h"','#include "llama-memory-recurrent.h"','#include "llama-kv-cache.h"','#include "llama-adapter.h"','#include "llama-moe-cache.h"','#include "bridge.h"','#include "owner.hpp"','#include <cstring>','#include <limits>','#include <stdexcept>','static int64_t handle(const void *p){return (int64_t)(intptr_t)p;}','static ggml_tensor *tensor(int64_t p){return (ggml_tensor *)(intptr_t)p;}','static ApexOwner &owner(int64_t e){return *(ApexOwner *)(intptr_t)e;}']
for n,p in proto.items():
 ret=p['ret'];gr='bool' if ret=='bool' else '' if ret=='void' else 'i64';cr='bool' if ret=='bool' else 'void' if ret=='void' else 'int64_t'
 params=','.join(f'a{i}:{a[3]}' for i,a in enumerate(p['args']));decl.append(f'extern "fg153_am_p_{n}" fn p_{n}({params})'+(f'->{gr}' if gr else '')+';')
 cp=','.join(('float' if a[3]=='f32' else 'bool' if a[3]=='bool' else 'int64_t')+f' a{i}' for i,a in enumerate(p['args']))
 cpro.append(f'{cr} fg153_am_p_{n}({cp});')
 call=[]
 for i,(ty,nm,ar,gt) in enumerate(p['args']):
  if 'ggml_context' in ty:arg=f'owner(a{i}).ctx0'
  elif 'ggml_cgraph' in ty:arg=f'owner(a{i}).gf'
  elif n=='rope_multi' and nm=='sections':arg='owner(a0).rope_sections.data()'
  elif 'ggml_tensor' in ty:arg=f'tensor(a{i})'
  elif ty in ['float','bool','int64_t','size_t']:arg=f'a{i}'
  else:arg=f'({ty})a{i}'
  call.append(arg)
 expr='ggml_'+n+'('+','.join(call)+')'
 stmt=expr+';' if cr=='void' else 'return '+(expr if cr=='bool' else 'handle('+expr+')' if '*' in ret else expr)+';'
 cpp.append(f'extern "C" {cr} fg153_am_p_{n}({cp})'+'{'+stmt+'}')
cpro+=['int64_t fg153_apex_probe(int64_t,int64_t);','uint64_t fg153_apex_counts(int64_t);','int64_t fg153_am_lget(int64_t,int64_t);','int64_t fg153_am_bind(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);','void fg153_am_recurrent(int64_t,int64_t,bool);','void fg153_apex_bind(int64_t,int64_t);','int64_t fg153_apex_build(int64_t owner);','#ifdef __cplusplus','}','#endif']
(S/'bridge.h').write_text('\n'.join(cpro)+'\n')
(S/'primitives.cpp').write_text('\n'.join(cpp)+'\n')
(S/'kernel.goose').write_text('\n'.join(decl)+'\n'+text)
(S/'abi.json').write_text(json.dumps(dict(fields=fields,floats=floats,weights=weights,labels=labels,primitives=proto),indent=2))
# Keep generation inputs available for exact replay.
(S/'generate.py').write_text(Path(__file__).read_text())
(S/'bind.tpl').write_text(Path(__file__).with_name('bind.tpl').read_text())
(S/'helpers.tpl').write_text(Path(__file__).with_name('helpers.tpl').read_text())
print('complete kernel',len(text.splitlines()),'lines',len(proto),'primitive APIs')
