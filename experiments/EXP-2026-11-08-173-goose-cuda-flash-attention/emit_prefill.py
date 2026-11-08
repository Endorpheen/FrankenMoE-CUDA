from pathlib import Path
import re,json,hashlib,subprocess
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';S=E/'source';C=F/'runtime/compiler/stage3-f849233b999c';assert hashlib.sha256(C.read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a'
def body(g,m):
 at=m.end();depth=1
 while depth:depth+=(g[at]=='{')-(g[at]=='}');at+=1
 return g[m.start():at]
rows=[]
for part,prefix,macro in [('iter','fg175mi_','FG175I_ITER'),('process','fg175mp_','FG175P_PROCESS')]:
 stem='mma-'+part;src=S/(stem+'-device.goose');out=S/(stem+'-device-generated.c');q=subprocess.run([str(C),'-o',str(out),str(src)],capture_output=True,text=True);assert q.returncode==0,q.stderr;g=out.read_text();main=g[g.rfind('static void gg_function_0('):];calls=[n for n in re.findall(r'\b(gg_function_\d+)\(',main)if n!='gg_function_0'];assert len(calls)==1
 tm=re.search(r'(?m)^typedef struct[^\n]*gg_type_\d+',g);assert tm;types=g[g.rfind('\n\n',0,tm.start()):g.index('static void gg_quote(')];layouts=[]
 for m in re.finditer(r'struct (gg_type_\d+) \{\n([^}]+)\};',g):layouts.append((m[1],re.findall(r'\b(float|uint32_t) (f\d+);',m[2])))
 qual=lambda s:re.sub(r'\b(gg_|ggf_|gs_)',lambda m:prefix+m[1],s)
 wanted=[('fg175i_F4','float',4,['f4','f5','f6','f7']),('fg175i_U4','uint32_t',4,['f0','f1','f2','f3']),('fg175i_U2','uint32_t',2,['f8','f9'])]if part=='iter'else[('fg175p_U4','uint32_t',4,['f0','f1','f2','f3'])]
 alias=''
 for name,typ,n,expected in wanted:
  t,fields=next((t,f)for t,f in layouts if len(f)==n and all(x[0]==typ for x in f));assert [x[1]for x in fields]==expected,(part,name,fields);alias+='using '+name+' = '+prefix+t+';\n'
 (S/(stem+'-types-generated.cuh')).write_text('#pragma once\n#include <stdint.h>\n'+qual('typedef struct{void*data;int64_t len;}gg_slice;\n'+types)+alias)
 funcs=['static void gg_fail(const char*){asm("trap;");}']
 for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+|gg_view|gg_double_bits|gg_index|gg_div|gg_mod|gg_udiv|gg_umod)\([^;{}]*\)\s*\{',g):
  if m[1]!='gg_function_0':funcs.append(body(g,m))
 text=qual('\n'.join(x[:x.index('{')].rstrip()+';'for x in funcs)+'\n'+'\n'.join(funcs));text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
 assert not any(x in text for x in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']);(S/(stem+'-functions-generated.cuh')).write_text('#pragma once\n'+text+'\n#define '+macro+' '+prefix+calls[0]+'\n');rows.append(dict(module=stem,entry=prefix+calls[0],actual_functions=len(funcs)-1,source_sha256=hashlib.sha256(src.read_bytes()).hexdigest()));print('PASS actual selfhosted emit and unchanged rawPOD field ABI',part,flush=True)
(E/'prefill-emission.json').write_text(json.dumps(dict(status='PASS_CURRENT_SELFHOSTED_EMISSION_AND_POD_ABI',modules=rows),indent=2)+'\n')
