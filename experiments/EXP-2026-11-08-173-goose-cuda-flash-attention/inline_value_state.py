from pathlib import Path
import importlib.util,re,json,hashlib
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';S=E/'source';sp=importlib.util.spec_from_file_location('lexical',E/'specialize_iter.py');m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m);src=(S/'value-prefill-device.goose').read_text();original=src;join=lambda t:''.join(v for _,v in t)
assert not (S/'value-prefill-call-device.goose').exists()or(S/'value-prefill-call-device.goose').read_text()==src;(S/'value-prefill-call-device.goose').write_text(src);
if not (S/'value-prefill-call-device-generated.c').exists():(S/'value-prefill-call-device-generated.c').write_bytes((S/'value-prefill-device-generated.c').read_bytes())
bs={}
def find(ts,name):
 for fi in range(len(ts)):
  if ts[fi]!=('ident','fn'):continue
  ni=m.ns(ts,fi+1)
  if ts[ni]!=('ident',name):continue
  j=m.ns(ts,ni+1);assert ts[j]==('punct','(');pc=m.match(ts,j);k=m.ns(ts,pc+1)
  if ts[k]==('punct','->'):k=m.ns(ts,m.ns(ts,k+1)+1)
  assert ts[k]==('punct','{');return fi,ni,j,pc,k,m.match(ts,k)
 raise RuntimeError('function missing '+name)
for nc in [4,8]:
 ts=m.lex(src);fi,ni,j,pc,k,bc=find(ts,'iterate_value'+str(nc));body=join(ts[k+1:bc]);assert body.startswith('var acc:Accumulator=initial;')and body.rstrip().endswith('return acc;');body=body[len('var acc:Accumulator=initial;'):];body=body[:body.rfind('return acc;')];bs[nc]=body;src=join(ts[:fi])+join(ts[bc+1:])
for nc,npv in [(4,2),(8,1)]:
 ts=m.lex(src);fi,ni,j,pc,k,bc=m.find(ts,'process_tile_apex_nc'+str(nc)+'_np'+str(npv));bt=ts[k+1:bc];out=[];i=0;calls=[]
 while i<len(bt):
  if bt[i]==('ident','acc'):
   eq=m.ns(bt,i+1);call=m.ns(bt,eq+1)
   if eq<len(bt)and bt[eq]==('punct','=')and call<len(bt)and bt[call]==('ident','iterate_value'+str(nc)):
    op=m.ns(bt,call+1);assert bt[op]==('punct','(');cl=m.match(bt,op);end=m.ns(bt,cl+1);assert bt[end]==('punct',';');aa=[re.sub(r'\s+','',x)for x in m.split_params(bt[op+1:cl])]
    expected=['k_h2','v_h2','mask_h','stride_k','stride_v','stride_mask','slope','jt','kb0','ne01z','qr','acc','smem','tx','ty'];assert aa[:-1]==expected and aa[-1]in ['false','true'],aa
    out.append(('raw','if true {let n_queries:i64=ne01z;let last:bool='+aa[-1]+';\n'+bs[nc]+'\n}\n'));calls.append(aa[-1]);i=end+1;continue
  out.append(bt[i]);i+=1
 assert calls==['false','true'],calls;src=join(ts[:k+1])+join(out)+join(ts[bc:])
assert 'iterate_value'not in src
assert not re.search(r'fn [^{\n]*(?:QueryRegs|Accumulator)',src),'State should not cross a Goose function boundary'
(S/'value-prefill-device.goose').write_text(src)
p=E/'value-prefill-pending.json';d=json.loads(p.read_text());d.update(status='INLINED_GOOSE_STATE_PENDING_NEW_ABI_GPU_APEX',by_value_call_variant=dict(source_sha256=hashlib.sha256(original.encode()).hexdigest(),library='runtime/exp173/run/libggml-cuda-fa-6545dc984351.so.0.27.0',gpu_correctness='PASS whole API and long-random corpus/default+rollback',apex='NOT_RUN:actual ptxas exposes significant spill traffic;withheld before promotion',ptxas='value-prefill-ptxas.json'),hypothesis='Keep scalar-field state inside one Goose process block;inline actual Goose iteration source without copying/returning large state PODs across a function ABI. Same math/order/barriers/partition.',state_crosses_function_boundary=False);p.write_text(json.dumps(d,indent=2)+'\n');print('INLINED_VALUE_STATE_SOURCE_READY',len(src),'bytes')
