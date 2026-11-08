from pathlib import Path
import json,re,shlex,subprocess,hashlib,argparse
ap=argparse.ArgumentParser();ap.add_argument('--host-only',action='store_true');options=ap.parse_args()
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-169-goose-cuda-reductions';S=E/'source';B=F/'runtime/exp169/build';R=F/'runtime/exp169/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
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
entries={}
for name,prefix in [('row-device','fg169r_'),('arg-device','fg169a_'),('host','fg169h_')]:
 run('emit-'+name,[C,'-o',S/(name+'-generated.c'),S/(name+'.goose')]);g=(S/(name+'-generated.c')).read_text();main=g[g.rfind('static void gg_function_0('):];calls=[n for n in re.findall(r'\b(gg_function_\d+)\(',main) if n!='gg_function_0'];assert calls;entries[name]=[prefix+n for n in calls]
 if name=='host':
  assert len(calls)==1;(S/'host-qualified.c').write_text(qualify(g,prefix));(S/'host-unit.c').write_text('#include "host-ffi.h"\n#define main fg169_unused_main\n#include "host-qualified.c"\n#undef main\nvoid fg169_host_entry(int64_t p){'+prefix+calls[0]+'(p,1); }\n')
 else:
  tm=re.search(r'(?m)^typedef struct[^\n]*gg_type_\d+',g);assert tm;start=g.rfind('\n\n',0,tm.start());types=g[start:g.index('static void gg_quote(')]
  funcs=[body(g,m) for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+)\([^;{}]*\)\s*\{',g) if m[1]!='gg_function_0'];assert funcs
  helper='static void gg_fail(const char*){asm("trap;");}\n'
  for hn in ['gg_double_bits','gg_index','gg_div','gg_mod','gg_udiv','gg_umod']:
   m=re.search(r'(?m)^static [^\n{]+? '+hn+r'\([^;{}]*\)\s*\{',g);assert m,hn;helper+=body(g,m)+'\n'
  declarations='\n'.join(f[:f.index('{')].rstrip()+';' for f in funcs)+'\n';text=qualify(helper+declarations+'\n'.join(funcs),prefix);text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
  for forbidden in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']:assert forbidden not in text
  (S/(name+'-generated.cuh')).write_text('#pragma once\n#include <stdint.h>\n#include <string.h>\n'+qualify(types,prefix)+text+'\n')
assert len(entries['row-device'])==5 and len(entries['arg-device'])==1
macros=['FG169_FINISH_MEAN','FG169_ROW_SUM','FG169_ROW_MEAN','FG169_ROW_SUM_STRIDED','FG169_ROW_MEAN_STRIDED'];(S/'entries.cuh').write_text(''.join('#define '+k+' '+v+'\n' for k,v in zip(macros,entries['row-device']))+'#define FG169_ARGMAX '+entries['arg-device'][0]+'\n')
names={'ggml_cuda_op_sum_rows','sum_rows_f32_cuda','ggml_cuda_op_mean','ggml_cuda_argmax','ggml_cuda_op_sum','sum_f32_cuda'};native=''
for n in ['sumrows','mean','argmax','sum']:
 text=(S/('native-'+n+'.cu')).read_text().replace('#include "reduce_rows.cuh"','')
 for fn in sorted(names,key=len,reverse=True):text=re.sub(r'\b'+fn+r'\b','fg169_native_'+fn,text)
 native+=text+'\n'
(S/'native-prefixed.cuh').write_text('#include "native-reduce_rows.cuh"\n'+native);(S/'cuda-reductions.cu').write_text('#include "native-prefixed.cuh"\n#include "projection.cuh"\n#include "row-device-generated.cuh"\n#include "arg-device-generated.cuh"\n#include "entries.cuh"\n#include "cuda-bridge.cuh"\n')
D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');db=json.loads((D/'compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/mean.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(Path(r['file']).parent));cmd.insert(1,'-I'+str(S));cmd[cmd.index('-c')+1]=str(S/'cuda-reductions.cu');obj=B/'cuda-reductions.o';cmd[cmd.index('-o')+1]=str(obj);deps=['row-device.goose','arg-device.goose','row-device-generated.c','arg-device-generated.c','row-device-generated.cuh','arg-device-generated.cuh','entries.cuh','cuda-bridge.cuh','projection.cuh','plan.h','host-ffi.h','native-prefixed.cuh','native-reduce_rows.cuh','cuda-reductions.cu']
if options.host_only:
 saved=json.loads((B/'device-object-inputs.json').read_text());assert saved['inputs']=={n:H(S/n) for n in deps} and saved['object_sha256']==H(obj);rows.append(dict(label='cuda-reductions-build',argv=list(map(str,cmd)),cwd=str(r['directory']),exit_code=0,reused_unchanged_device_object=True))
else:
 run('cuda-reductions-build',cmd,Path(r['directory']));(B/'device-object-inputs.json').write_text(json.dumps(dict(inputs={n:H(S/n) for n in deps},object_sha256=H(obj)),indent=2)+'\n')
host=B/'host-goose.o';run('host-goose-C-build',['/usr/bin/gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden','-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/'host-unit.c','-o',host]);past=json.loads((F/'runtime/exp168/build/commands.json').read_text());r=next(x for x in past if x['label']=='cuda-normalization-link');cmd=r['argv'].copy();ref=B/'unchanged-exp168-cuda.so';cmd[cmd.index('-o')+1]=str(ref);cmd=[('-Wl,--dependency-file='+str(B/'unchanged-cuda.d')) if x.startswith('-Wl,--dependency-file=') else x for x in cmd];run('unchanged-exp168-cuda-link-integrity',cmd,Path(r['cwd']));assert H(ref)==H(F/'runtime/exp168/run/libggml-cuda.so');remove=[x for x in cmd if any(x.endswith('/'+n+'.cu.o') for n in ['sumrows','mean','argmax','sum'])];assert len(remove)==4;cmd=[x for x in cmd if x not in remove];cmd=[('-Wl,--version-script='+str(S/'visibility.map')) if x.startswith('-Wl,--version-script=') else x for x in cmd];cmd +=[str(obj),str(host)];out=R/('libggml-cuda-reductions-'+hashlib.sha256((H(obj)+H(host)+H(S/'visibility.map')).encode()).hexdigest()[:12]+'.so.0.27.0');linkout=B/'relink.so' if out.exists() else out;cmd[cmd.index('-o')+1]=str(linkout);run('cuda-reductions-link',cmd,Path(r['cwd']));
if linkout!=out:assert H(linkout)==H(out)
for p in (F/'runtime/exp168/run').iterdir():
 if p.is_file() or p.is_symlink():
  q=R/p.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(out if p.name.startswith('libggml-cuda') else p.resolve())
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_CORRECTNESS',compiler_sha256=H(C),entries=entries,library=str(out),sha256=H(out),native_prefixed_functions=sorted(names),removed_objects=remove,external_numerical_primitive='NVIDIA CUB DeviceReduce::Sum for original device-wide paths;Goose owns selection/temp lifecycle and mean post-division',rollback='GGML_GOOSE_CUDA_REDUCTIONS=0 or immutable EXP168 runtime',production_changed=False),indent=2)+'\n');print('LINKED_GOOSE_CUDA_REDUCTIONS',out)
