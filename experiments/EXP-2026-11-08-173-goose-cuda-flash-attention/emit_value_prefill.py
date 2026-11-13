from pathlib import Path
import re,json,hashlib
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';S=E/'source';g=(S/'value-prefill-device-generated.c').read_text();p='fg175cv_';qual=lambda s:re.sub(r'\b(gg_|ggf_|gs_)',lambda m:p+m[1],s)
def body(g,m):
 at=m.end();depth=1
 while depth:depth+=(g[at]=='{')-(g[at]=='}');at+=1
 return g[m.start():at]
tm=re.search(r'(?m)^typedef struct[^\n]*gg_type_\d+',g);assert tm;types=g[g.rfind('\n\n',0,tm.start()):g.index('static void gg_quote(')];layouts=[]
for m in re.finditer(r'struct (gg_type_\d+) \{\n([^}]+)\};',g):layouts.append((m[1],re.findall(r'\b(float|uint32_t) (f\d+);',m[2])))
alias='';fields={}
for name,shape in [('U4',['uint32_t']*4),('U2',['uint32_t']*2),('F4',['float']*4),('QueryRegs',['uint32_t']*64),('Accumulator',['uint32_t']*64+['float']*4)]:
 t,fs=next((t,fs)for t,fs in layouts if [x[0]for x in fs]==shape);fields[name]=[x[1]for x in fs];alias+='using fg175c_'+name+' = '+p+t+';\n'
(S/'value-prefill-types-generated.cuh').write_text('#pragma once\n#include <stdint.h>\n'+qual('typedef struct{void*data;int64_t len;}gg_slice;\n'+types)+alias+'static_assert(sizeof(fg175c_QueryRegs)==256&&sizeof(fg175c_Accumulator)==272,"Goose value state widths");\n')
funcs=['static void gg_fail(const char*){asm("trap;");}']
for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+|gg_view|gg_double_bits|gg_index|gg_div|gg_mod|gg_udiv|gg_umod)\([^;{}]*\)\s*\{',g):
 if m[1]!='gg_function_0':funcs.append(body(g,m))
text=qual('\n'.join(f[:f.index('{')].rstrip()+';'for f in funcs)+'\n'+'\n'.join(funcs));text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text);assert not any(x in text for x in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']);main=g[g.rfind('static void gg_function_0('):];entry=[x for x in re.findall(r'\b(gg_function_\d+)\(',main)if x!='gg_function_0'];assert len(entry)==1
(S/'value-prefill-functions-generated.cuh').write_text('#pragma once\n'+text+'\n#define FG175C_PROCESS '+p+entry[0]+'\n')
meta=json.loads((E/'value-prefill-pending.json').read_text());out='#pragma once\n#include "value-prefill-types-generated.cuh"\n';ct={'void':'void','u64':'uint64_t','i64':'int64_t','u32':'uint32_t','i32':'int32_t','f32':'float','bool':'uint8_t','U4':'fg175c_U4','U2':'fg175c_U2','F4':'fg175c_F4'};oldfields={'U4':['f0','f1','f2','f3'],'U2':['f8','f9'],'F4':['f4','f5','f6','f7']}
for r in meta['raw_adapters']:
 params=[];argv=[]
 for x in r['params'].split(','):
  if not x.strip():continue
  n,t=[y.strip()for y in x.split(':')];params.append(ct[t]+' '+n)
  argv.append('fg175i_'+t+'{'+','.join(n+'.'+f for f in fields[t])+'}'if t in ['U4','U2','F4']else n)
 call=r['original']+'('+','.join(argv)+')';rt=r['result']
 if rt=='void':b=call+';'
 elif rt in ['U4','U2','F4']:b='auto z='+call+';return{'+','.join('z.'+x for x in oldfields[rt])+'};'
 else:b='return '+call+';'
 out+='static __device__ __forceinline__ '+ct[rt]+' '+r['symbol']+'('+','.join(params)+'){'+b+'}\n'
(S/'value-prefill-primitives.cuh').write_text(out)
pth=S/'cuda-runtime.cu';s=pth.read_text();needle='#include "mma-entry-adapter.cuh"';assert s.count(needle)==1;s=s if '#include "value-prefill-primitives.cuh"'in s else s.replace(needle,'#include "value-prefill-primitives.cuh"\n#include "value-prefill-functions-generated.cuh"\n'+needle);pth.write_text(s)
pth=S/'mma-entry-adapter.cuh';s=pth.read_text();assert s.count('FG175P_PROCESS(')==1 or s.count('FG175C_PROCESS(')==1;pth.write_text(s.replace('FG175P_PROCESS(','FG175C_PROCESS('))
pth=S/'mma-runtime-kernel.cuh';s=pth.read_text();old='uint32_t qregs[64],vregs[64];float kmax[2],rowsums[2];';assert s.count(old)<=1;s=s.replace(old,'')
for name in ['qregs','vregs','kmax','rowsums']:
 old='(uint64_t)(uintptr_t)'+name;assert s.count(old)<=1;s=s.replace(old,'(uint64_t)0')
pth.write_text(s)
pth=E/'verify_generated.py';s=pth.read_text();needle="('host','host-qualified.c','fg175hh_')";assert s.count(needle)==1;s=s if '#include "value-prefill-primitives.cuh"'in s else s.replace(needle,"('value-prefill-device','value-prefill-functions-generated.cuh','fg175cv_'),"+needle);pth.write_text(s)
meta.update(status='EMITTED_PENDING_CUDA_AND_APEX',entry=p+entry[0],pod_fields=fields,legacy_private_state_allocations_removed=True);(E/'value-prefill-pending.json').write_text(json.dumps(meta,indent=2)+'\n');print('VALUE_PREFILL_ACTUAL_GOOSE_AND_RAW_ABI_READY',p+entry[0],len(funcs)-1,'functions')
