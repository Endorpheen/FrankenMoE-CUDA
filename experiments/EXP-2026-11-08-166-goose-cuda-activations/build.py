from pathlib import Path
import json,re,shlex,subprocess,hashlib
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-166-goose-cuda-activations';S=E/'source';B=F/'runtime/exp166/build';R=F/'runtime/exp166/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=900);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-7500:]);raise SystemExit(q.returncode)
 return q
entries={}
for name in ['device','host']:
 run('emit-'+name,[C,'-o',S/(name+'-generated.c'),S/(name+'.goose')]);g=(S/(name+'-generated.c')).read_text();main=g[g.rfind('static void gg_function_0('):];calls=re.findall(r'\b(gg_function_\d+)\(',main);calls=[n for n in calls if n!='gg_function_0'];assert calls;prefix='fg166d_' if name=='device' else 'fg166h_';entry=prefix+calls[0];entries[name]=entry
 if name=='host':
  (S/'host-qualified.c').write_text(re.sub(r'\b(gg_|ggf_|gs_)',lambda m:prefix+m[1],g));(S/'host-unit.c').write_text('#include "host-ffi.h"\n#define main fg166_unused_main\n#include "host-qualified.c"\n#undef main\nvoid fg166_host_entry(int64_t p){'+entry+'(p,1);}\n')
 else:
  funcs=[]
  for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+)\([^;{}]*\)\s*\{',g):
   if m[1]=='gg_function_0':continue
   at=m.end();depth=1
   while depth:
    depth+=(g[at]=='{')-(g[at]=='}');at+=1
   funcs.append(g[m.start():at])
  assert funcs
  helper='static void gg_fail(const char*){asm("trap;");}\n'
  for name0 in ['gg_double_bits','gg_div','gg_mod']:
   m0=re.search(r'(?m)^static [^\n{]+? '+name0+r'\([^;{}]*\)\s*\{',g);assert m0,name0;at=m0.end();depth=1
   while depth:
    depth+=(g[at]=='{')-(g[at]=='}');at+=1
   helper+=g[m0.start():at]+'\n'
  text=helper+'\n'.join(funcs);text=re.sub(r'\b(gg_|ggf_|gs_)',lambda m:prefix+m[1],text);text=re.sub(r'(?m)^static ', 'static __device__ __forceinline__ ',text);assert 'gg_stack' not in text and 'gg_reference' not in text
  (S/'device-generated.cuh').write_text('#pragma once\n#include <stdint.h>\n#include <string.h>\n'+text+'\n#define FG166_DEVICE_LANE '+entry+'\n');(S/'device-extraction.json').write_text(json.dumps(dict(entry=entry,functions=len(funcs),transformation='Select emitted scalar functions and checked divide/modulo helpers;qualify names and add CUDA device/forceinline. Map terminal fatal effect to trap;no emitted arithmetic edits.'),indent=2)+'\n')
# Preserve original CUDA functions behind unique names for a native oracle and rollback.
s='';public=[]
for name in ['unary','scale','clamp']:
 source=(S/('native-'+name+'.cu')).read_text();names=set(re.findall(r'(?m)^(?:static )?void\s+(ggml_cuda_op_\w+)\s*\(',source));public+=sorted(names)
 for n in sorted(names,key=len,reverse=True):source=re.sub(r'\b'+n+r'\b','fg166_native_'+n,source)
 (S/(name+'-native-prefixed.cuh')).write_text(source);s+='#include "'+name+'-native-prefixed.cuh"\n'
s+='#include "device-effects.cuh"\n#include "device-generated.cuh"\n#include "cuda-bridge.cuh"\n';(S/'cuda-activations.cu').write_text(s)
D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');db=json.loads((D/'compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/unary.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(Path(r['file']).parent));cmd.insert(1,'-I'+str(S));cmd[cmd.index('-c')+1]=str(S/'cuda-activations.cu');obj=B/'cuda-activations.o';cmd[cmd.index('-o')+1]=str(obj);run('cuda-activations-build',cmd,Path(r['directory']));host=B/'host-goose.o';run('host-goose-C-build',['/usr/bin/gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden','-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/'host-unit.c','-o',host]);past=json.loads((F/'experiments/EXP-2026-11-08-160-goose-shared-fusion/build-commands.json').read_text());r=next(x for x in past if x['label']=='shared-cuda-link');cmd=r['argv'].copy();ref=B/'unchanged-exp165-cuda.so';cmd[cmd.index('-o')+1]=str(ref);cmd=[('-Wl,--dependency-file='+str(B/'unchanged-cuda.d')) if x.startswith('-Wl,--dependency-file=') else x for x in cmd];run('unchanged-exp165-cuda-link-integrity',cmd,Path(r['cwd']));assert H(ref)==H(F/'runtime/exp165/run/libggml-cuda.so');remove=[x for x in cmd if x.endswith(('/unary.cu.o','/scale.cu.o','/clamp.cu.o'))];assert len(remove)==3;cmd=[x for x in cmd if x not in remove];cmd +=[str(obj),str(host),'-Wl,--version-script='+str(S/'visibility.map')];out=R/('libggml-cuda-activations-'+hashlib.sha256((H(obj)+H(host)+H(S/'visibility.map')).encode()).hexdigest()[:12]+'.so.0.27.0');linkout=B/'relink.so' if out.exists() else out;cmd[cmd.index('-o')+1]=str(linkout);run('cuda-activations-link',cmd,Path(r['cwd']));
if linkout!=out:assert H(linkout)==H(out)
for p in (F/'runtime/exp165/run').iterdir():
 if p.is_file() or p.is_symlink():
  q=R/p.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(out if p.name.startswith('libggml-cuda') else p.resolve())
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_CORRECTNESS',compiler_sha256=H(C),entries=entries,library=str(out),sha256=H(out),public_entrypoints=json.loads((S/'recipes.json').read_text()),native_prefixed_functions=public,removed_objects=remove,rollback='GGML_GOOSE_CUDA_ACTIVATIONS=0 with legacy SiLU/scale preloads or immutable EXP165 runtime',production_changed=False),indent=2)+'\n');print('LINKED_GOOSE_CUDA_ACTIVATIONS',out)
