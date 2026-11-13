from pathlib import Path
import json,re,shlex,subprocess,hashlib
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-169-goose-cuda-reductions';S=E/'source/topk';B=F/'runtime/exp169/topk-build';R=F/'runtime/exp169/run';B.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
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
entries={};headers=[]
for ept in [1,2,4,8,9,16,18]:
 name='device-e'+str(ept);prefix='fg169t'+str(ept)+'_';source=(S/'device.goose').read_text().replace('f32[18]','f32['+str(ept)+']');(S/(name+'.goose')).write_text(source);run('emit-'+name,[C,'-o',S/(name+'-generated.c'),S/(name+'.goose')]);g=(S/(name+'-generated.c')).read_text();main=g[g.rfind('static void gg_function_0('):];calls=[n for n in re.findall(r'\b(gg_function_\d+)\(',main) if n!='gg_function_0'];assert len(calls)==1;entry=prefix+calls[0];entries[str(ept)]=entry
 tm=re.search(r'(?m)^typedef struct[^\n]*gg_type_\d+',g);assert tm;start=g.rfind('\n\n',0,tm.start());types=g[start:g.index('static void gg_quote(')];funcs=[body(g,m) for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+)\([^;{}]*\)\s*\{',g) if m[1]!='gg_function_0'];helper='static void gg_fail(const char*){asm("trap;");}\n'
 for hn in ['gg_view','gg_double_bits','gg_index','gg_div','gg_mod','gg_udiv','gg_umod']:
  m=re.search(r'(?m)^static [^\n{]+? '+hn+r'\([^;{}]*\)\s*\{',g);assert m;helper+=body(g,m)+'\n'
 declarations='\n'.join(f[:f.index('{')].rstrip()+';' for f in funcs)+'\n';text=qualify(helper+declarations+'\n'.join(funcs),prefix);text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
 for forbidden in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']:assert forbidden not in text
 header=name+'-generated.cuh';(S/header).write_text('#pragma once\n#include <stdint.h>\n#include <string.h>\n'+qualify('typedef struct { void *data; int64_t len; } gg_slice;\n'+types,prefix)+text+'\n#define FG169T_LANE_'+str(ept)+' '+entry+'\n');headers.append(header)
run('emit-host',[C,'-o',S/'host-generated.c',S/'host.goose']);g=(S/'host-generated.c').read_text();main=g[g.rfind('static void gg_function_0('):];calls=[n for n in re.findall(r'\b(gg_function_\d+)\(',main) if n!='gg_function_0'];assert len(calls)==1;entries['host']='fg169th_'+calls[0];(S/'host-qualified.c').write_text(qualify(g,'fg169th_'));(S/'host-unit.c').write_text('#include "host-ffi.h"\n#define main fg169t_unused_main\n#include "host-qualified.c"\n#undef main\nuint8_t fg169t_host_entry(int64_t p){return '+entries['host']+'(p,1); }\n')
native=(S/'native-topk-moe.cu').read_text();names=['ggml_cuda_op_topk_moe','ggml_cuda_should_use_topk_moe']
for name in names:native=re.sub(r'\b'+name+r'\b','fg169t_native_'+name,native)
(S/'native-prefixed.cuh').write_text(native.rstrip()+'\n');(S/'cuda-topk.cu').write_text('#include "native-prefixed.cuh"\n#include "projection.cuh"\n'+''.join('#include "'+h+'"\n' for h in headers)+'#include "cuda-bridge.cuh"\n')
allow=['fg168_cuda_host_seen','fg168_cuda_device_read','fg169_cuda_host_seen','fg169_cuda_cub_seen','fg169_cuda_live_temp','fg169_cuda_device_read','fg169t_cuda_host_seen','fg169t_cuda_device_read'];(S/'visibility.map').write_text('{ global: '+''.join(n+'; ' for n in allow)+'local: *fg166_native*; *fg167_native*; *fg168_native*; *fg169_native*; *fg169t_native*; *topk_moe_cuda*; fg168_*; fg169_*; fg169t_*; };\n')
D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');db=json.loads((D/'compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/topk-moe.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(Path(r['file']).parent));cmd.insert(1,'-I'+str(S));cmd[cmd.index('-c')+1]=str(S/'cuda-topk.cu');obj=B/'cuda-topk.o';cmd[cmd.index('-o')+1]=str(obj);run('cuda-topk-build',cmd,Path(r['directory']));host=B/'host-goose.o';run('host-goose-C-build',['/usr/bin/gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden','-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/'host-unit.c','-o',host]);past=json.loads((F/'runtime/exp169/build/commands.json').read_text());r=next(x for x in past if x['label']=='cuda-reductions-link');cmd=r['argv'].copy();ref=B/'unchanged-reductions-cuda.so';cmd[cmd.index('-o')+1]=str(ref);cmd=[('-Wl,--dependency-file='+str(B/'unchanged-cuda.d')) if x.startswith('-Wl,--dependency-file=') else x for x in cmd];run('unchanged-reductions-link-integrity',cmd,Path(r['cwd']));base=json.loads((E/'source/manifest.json').read_text())['library'];assert H(ref)==H(base);remove=[x for x in cmd if x.endswith('/topk-moe.cu.o')];assert len(remove)==1;cmd=[x for x in cmd if x not in remove];cmd=[('-Wl,--version-script='+str(S/'visibility.map')) if x.startswith('-Wl,--version-script=') else x for x in cmd];cmd+=[str(obj),str(host)];out=R/('libggml-cuda-routing-'+hashlib.sha256((H(obj)+H(host)+H(S/'visibility.map')+H(ref)).encode()).hexdigest()[:12]+'.so.0.27.0');linkout=B/'relink.so' if out.exists() else out;cmd[cmd.index('-o')+1]=str(linkout);run('cuda-routing-link',cmd,Path(r['cwd']));
if linkout!=out:assert H(linkout)==H(out)
for p in R.iterdir():
 if p.is_symlink() and p.name.startswith('libggml-cuda'):p.unlink();p.symlink_to(out)
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_GPU_AND_REAL_APEX_LIVENESS',compiler_sha256=H(C),entries=entries,library=str(out),sha256=H(out),base_reductions_library=base,removed_objects=remove,rollback='GGML_GOOSE_CUDA_TOPK_MOE=0 or immutable EXP168 profile',production_changed=False),indent=2)+'\n');print('LINKED_GOOSE_FUSED_ROUTING',out)
