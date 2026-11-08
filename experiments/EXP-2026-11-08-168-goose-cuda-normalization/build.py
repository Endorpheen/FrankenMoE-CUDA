from pathlib import Path
import json,re,shlex,subprocess,hashlib,argparse
p=argparse.ArgumentParser();p.add_argument("--host-only",action="store_true");options=p.parse_args()
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-168-goose-cuda-normalization';S=E/'source';B=F/'runtime/exp168/build';R=F/'runtime/exp168/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=900);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-7500:]);raise SystemExit(q.returncode)
 return q
def body(g,m):
 at=m.end();depth=1
 while depth:
  depth+=(g[at]=='{')-(g[at]=='}');at+=1
 return g[m.start():at]
def qualify(g,prefix):return re.sub(r'\b(gg_|ggf_|gs_)',lambda m:prefix+m[1],g)
entries={}
for name in ['device','host']:
 run('emit-'+name,[C,'-o',S/(name+'-generated.c'),S/(name+'.goose')]);g=(S/(name+'-generated.c')).read_text();main=g[g.rfind('static void gg_function_0('):];calls=[n for n in re.findall(r'\b(gg_function_\d+)\(',main) if n!='gg_function_0'];assert calls;prefix='fg168d_' if name=='device' else 'fg168h_';entry=prefix+calls[0];entries[name]=entry
 if name=='host':
  (S/'host-qualified.c').write_text(qualify(g,prefix));(S/'host-unit.c').write_text('#include "host-ffi.h"\n#define main fg168_unused_main\n#include "host-qualified.c"\n#undef main\nvoid fg168_host_entry(int64_t p){'+entry+'(p,1); }\n')
 else:
  types=g[g.index('typedef struct gg_type_'):g.index('static void gg_quote(')]
  f4=[m for m in re.finditer(r'struct (gg_type_\d+) \{([^}]+)\};',types) if len(re.findall(r'float (f\d+);',m[2]))==4];assert len(f4)==1
  f4name=prefix+f4[0][1];fields=re.findall(r'float (f\d+);',f4[0][2]);(S/'device-types.cuh').write_text('#pragma once\n#include <stdint.h>\n'+qualify(types,prefix)+'\nusing fg168_F4='+f4name+';\nstatic_assert(sizeof(fg168_F4)==16,"Goose float4 ABI");\n')
  effects='static __device__ __forceinline__ fg168_F4 fg168_load4(uint64_t p,int32_t i){float4 v=((const float4*)(uintptr_t)p)[i];return fg168_F4{v.x,v.y,v.z,v.w};}\n'
  effects+='static __device__ __forceinline__ void fg168_store4(uint64_t p,int32_t i,fg168_F4 v){((float4*)(uintptr_t)p)[i]=make_float4('+','.join('v.'+x for x in fields)+');}\n';(S/'device-vector-effects.cuh').write_text(effects)
  funcs=[body(g,m) for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+)\([^;{}]*\)\s*\{',g) if m[1]!='gg_function_0'];assert funcs
  helper='static void gg_fail(const char*){asm("trap;");}\n'
  for hn in ['gg_double_bits','gg_index','gg_div','gg_mod','gg_udiv','gg_umod']:
   m=re.search(r'(?m)^static [^\n{]+? '+hn+r'\([^;{}]*\)\s*\{',g);assert m,hn;helper+=body(g,m)+'\n'
  declarations='\n'.join(f[:f.index('{')].rstrip()+';' for f in funcs)+'\n'
  text=qualify(helper+declarations+'\n'.join(funcs),prefix);text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
  for forbidden in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']:assert forbidden not in text
  (S/'device-generated.cuh').write_text('#pragma once\n#include <stdint.h>\n#include <string.h>\n'+text+'\n#define FG168_DEVICE_LANE '+entry+'\n');(S/'device-extraction.json').write_text(json.dumps(dict(entry=entry,functions=len(funcs),f4_type=f4name,transformation='Select emitted POD types/functions and checked helpers;qualify names and decorate device/forceinline. Typed vector load/store are raw storage effects. Map fatal effect to trap;no arithmetic edits.'),indent=2)+'\n')
source=(S/'native-norm.cu').read_text();names=set(re.findall(r'(?m)^void\s+(ggml_cuda_op_\w+)\s*\(',source));assert len(names)==8
for n in sorted(names,key=len,reverse=True):source=re.sub(r'\b'+n+r'\b','fg168_native_'+n,source)
(S/'norm-native-prefixed.cuh').write_text(source);(S/'cuda-normalization.cu').write_text('#include "norm-native-prefixed.cuh"\n#include "device-types.cuh"\n#include "projection.cuh"\n#include "device-vector-effects.cuh"\n#include "device-generated.cuh"\n#include "cuda-bridge.cuh"\n')
D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');db=json.loads((D/'compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/norm.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(Path(r['file']).parent));cmd.insert(1,'-I'+str(S));cmd[cmd.index('-c')+1]=str(S/'cuda-normalization.cu');obj=B/'cuda-normalization.o';cmd[cmd.index('-o')+1]=str(obj);
device_inputs=['device.goose','device-generated.c','device-types.cuh','device-vector-effects.cuh','device-generated.cuh','projection.cuh','plan.h','native-norm.cu','norm-native-prefixed.cuh','cuda-normalization.cu','cuda-bridge.cuh','host-ffi.h']
if not options.host_only:
 run('cuda-normalization-build',cmd,Path(r['directory']));(B/'device-object-inputs.json').write_text(json.dumps(dict(inputs={n:H(S/n) for n in device_inputs},object_sha256=H(obj)),indent=2)+'\n')
else:
 saved=json.loads((B/'device-object-inputs.json').read_text());assert saved['inputs']=={n:H(S/n) for n in device_inputs} and saved['object_sha256']==H(obj);rows.append(dict(label='cuda-normalization-build',argv=list(map(str,cmd)),cwd=str(r['directory']),exit_code=0,reused_unchanged_device_object=True))
host=B/'host-goose.o';run('host-goose-C-build',['/usr/bin/gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden','-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/'host-unit.c','-o',host]);past=json.loads((F/'runtime/exp167/build/commands.json').read_text());r=next(x for x in past if x['label']=='cuda-broadcast-link');cmd=r['argv'].copy();ref=B/'unchanged-exp167-cuda.so';cmd[cmd.index('-o')+1]=str(ref);cmd=[('-Wl,--dependency-file='+str(B/'unchanged-cuda.d')) if x.startswith('-Wl,--dependency-file=') else x for x in cmd];run('unchanged-exp167-cuda-link-integrity',cmd,Path(r['cwd']));assert H(ref)==H(F/'runtime/exp167/run/libggml-cuda.so');remove=[x for x in cmd if x.endswith('/norm.cu.o')];assert len(remove)==1;cmd=[x for x in cmd if x not in remove];cmd=[('-Wl,--version-script='+str(S/'visibility.map')) if x.startswith('-Wl,--version-script=') else x for x in cmd];cmd +=[str(obj),str(host)];out=R/('libggml-cuda-normalization-'+hashlib.sha256((H(obj)+H(host)+H(S/'visibility.map')).encode()).hexdigest()[:12]+'.so.0.27.0');linkout=B/'relink.so' if out.exists() else out;cmd[cmd.index('-o')+1]=str(linkout);run('cuda-normalization-link',cmd,Path(r['cwd']));
if linkout!=out:assert H(linkout)==H(out)
for p in (F/'runtime/exp167/run').iterdir():
 if p.is_file() or p.is_symlink():
  q=R/p.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(out if p.name.startswith('libggml-cuda') else p.resolve())
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_CORRECTNESS',compiler_sha256=H(C),entries=entries,library=str(out),sha256=H(out),public_entrypoints=json.loads((S/'recipes.json').read_text()),native_prefixed_functions=sorted(names),removed_objects=remove,rollback='GGML_GOOSE_CUDA_NORM=0 or immutable EXP167 runtime',production_changed=False),indent=2)+'\n');print('LINKED_GOOSE_CUDA_NORMALIZATION',out)
