from pathlib import Path
import json,re,shlex,subprocess,hashlib
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';S=E/'source';B=F/'runtime/exp173/build';R=F/'runtime/exp173/run';B.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
assert __import__('hashlib').sha256((F/'runtime/compiler/stage3-f849233b999c').read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a','Pinned compiler changed'
assert all(__import__('hashlib').sha256(Path(p).read_bytes()).hexdigest()==h for p,h in json.loads((E/'external-header-pins.json').read_text()).items()),'Pinned external header changed; review first'

def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=900);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-9000:]);raise SystemExit(q.returncode)
 return q
def body(g,m):
 at=m.end();depth=1
 while depth:
  depth+=(g[at]=='{')-(g[at]=='}');at+=1
 return g[m.start():at]
def qualify(g,p):return re.sub(r'\b(gg_|ggf_|gs_)',lambda m:p+m[1],g)
prefix='fg175vd_';run('emit-vector',[C,'-o',S/'vector-device-generated.c',S/'vector-device.goose']);g=(S/'vector-device-generated.c').read_text();main=g[g.rfind('static void gg_function_0('):];calls=[n for n in re.findall(r'\b(gg_function_\d+)\(',main)if n!='gg_function_0'];assert len(calls)==2;entries=[prefix+n for n in calls]
tm=re.search(r'(?m)^typedef struct[^\n]*gg_type_\d+',g);assert tm;start=g.rfind('\n\n',0,tm.start());types=g[start:g.index('static void gg_quote(')];named=[]
for m in re.finditer(r'struct (gg_type_\d+) \{\n([^}]+)\};',g):
 fields=re.findall(r'\bfloat (f\d+);',m[2])
 if len(fields)==4:named.append((m[1],fields))
assert len(named)==1
(S/'vector-types-generated.cuh').write_text('#pragma once\n#include <stdint.h>\n'+qualify('typedef struct {void*data;int64_t len;} gg_slice;\n'+types,prefix)+'using fg175v_V4 = '+prefix+named[0][0]+';\n')
funcs=[body(g,m)for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+)\([^;{}]*\)\s*\{',g)if m[1]!='gg_function_0'];helper='static void gg_fail(const char*){asm("trap;");}\n'
for hn in ['gg_view','gg_double_bits','gg_index','gg_div','gg_mod','gg_udiv','gg_umod']:
 m=re.search(r'(?m)^static [^\n{]+? '+hn+r'\([^;{}]*\)\s*\{',g);assert m;helper+=body(g,m)+'\n'
declarations='\n'.join(f[:f.index('{')].rstrip()+';'for f in funcs)+'\n';text=qualify(helper+declarations+'\n'.join(funcs),prefix);text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
for forbidden in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']:assert forbidden not in text
(S/'vector-functions-generated.cuh').write_text('#pragma once\n'+text+'\n#define FG175V_VEC '+entries[0]+'\n#define FG175V_COMBINE '+entries[1]+'\n')
s=(S/'native-fattn-common.cuh').read_text().replace('flash_attn_combine_results','fg175v_native_flash_attn_combine_results');(S/'vector-native-common.cuh').write_text(s);s=(S/'native-fattn-vec.cuh').read_text().replace('#include "fattn-common.cuh"','#include "vector-native-common.cuh"').replace('flash_attn_ext_vec','fg175v_native_flash_attn_ext_vec');(S/'vector-native.cuh').write_text(s);(S/'cuda-vector.cu').write_text('#include "vector-native.cuh"\n#include "vector-primitives.cuh"\n#include "vector-functions-generated.cuh"\n#include "vector-bridge.cuh"\n')
D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');db=json.loads((D/'compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/fattn-vec-instance-q4_0-q4_0.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(Path(r['file']).parent.parent));cmd.insert(1,'-I'+str(S));cmd[cmd.index('-c')+1]=str(S/'cuda-vector.cu');cmd[cmd.index('-o')+1]=str(B/'cuda-vector.o');run('cuda-vector-component-build',cmd,Path(r['directory']));(E/'vector-object-receipt.json').write_text(json.dumps(dict(status='COMPILED_COMPONENT_ONLY_NOT_RUNTIME_ACCEPTED',compiler_sha256=H(C),entries=entries,object=str(B/'cuda-vector.o'),object_sha256=H(B/'cuda-vector.o'),vm_allocation_in_compute=False,production_changed=False),indent=2)+'\n');print('VECTOR_COMPONENT_OBJECT_READY')
