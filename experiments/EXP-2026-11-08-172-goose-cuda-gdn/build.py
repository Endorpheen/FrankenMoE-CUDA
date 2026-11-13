from pathlib import Path
import json,re,shlex,subprocess,hashlib
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-172-goose-cuda-gdn';S=E/'source';B=F/'runtime/exp172/combined-build';R=F/'runtime/exp172/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
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
for name,prefix in [('scalar-device','fg174sd_'),('scalar-host','fg174sh_'),('mma-device','fg174md_'),('mma-host','fg174mh_')]:
 run('emit-'+name,[C,'-o',S/(name+'-generated.c'),S/(name+'.goose')]);g=(S/(name+'-generated.c')).read_text();main=g[g.rfind('static void gg_function_0('):];calls=[n for n in re.findall(r'\b(gg_function_\d+)\(',main)if n!='gg_function_0'];assert len(calls)==(2 if name=='mma-device'else 1);entries[name]=[prefix+n for n in calls]
 if name.endswith('host'):
  tag='fg174m'if name=='mma-host'else'fg174s';ret='uint8_t'if name=='mma-host'else'void';ffi='mma-host-ffi.h'if name=='mma-host'else'host-ffi.h';(S/(name+'-qualified.c')).write_text(qualify(g,prefix));(S/(name+'-unit.c')).write_text('#include "'+ffi+'"\n#define main '+tag+'_unused_main\n#include "'+name+'-qualified.c"\n#undef main\n'+ret+' '+tag+'_host_entry(int64_t p){'+('return 'if ret!='void'else'')+entries[name][0]+'(p,1);}\n');continue
 tm=re.search(r'(?m)^typedef struct[^\n]*gg_type_\d+',g);assert tm;start=g.rfind('\n\n',0,tm.start());types=g[start:g.index('static void gg_quote(')];funcs=[body(g,m)for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+)\([^;{}]*\)\s*\{',g)if m[1]!='gg_function_0'];helper='static void gg_fail(const char*){asm("trap;");}\n'
 for hn in ['gg_view','gg_double_bits','gg_index','gg_div','gg_mod','gg_udiv','gg_umod']:
  m=re.search(r'(?m)^static [^\n{]+? '+hn+r'\([^;{}]*\)\s*\{',g);assert m;helper+=body(g,m)+'\n'
 declarations='\n'.join(f[:f.index('{')].rstrip()+';'for f in funcs)+'\n';text=qualify(helper+declarations+'\n'.join(funcs),prefix);text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
 for forbidden in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']:assert forbidden not in text
 types=qualify('typedef struct { void *data; int64_t len; } gg_slice;\n'+types,prefix)
 if name=='mma-device':
  layouts=[]
  for m in re.finditer(r'struct (gg_type_\d+) \{\n([^}]+)\};',g):
   fields=re.findall(r'\b(float|uint32_t) (f\d+);',m[2]);layouts.append((m[1],fields))
  f4=next((t,fs)for t,fs in layouts if len(fs)==4 and all(tp=='float'for tp,_ in fs));u4=next((t,fs)for t,fs in layouts if len(fs)==4 and all(tp=='uint32_t'for tp,_ in fs));u2=next((t,fs)for t,fs in layouts if len(fs)==2 and all(tp=='uint32_t'for tp,_ in fs));aliases=''
  for nm,(tp,fields)in [('F4',f4),('U4',u4),('U2',u2)]:aliases+='using fg174m_'+nm+' = '+prefix+tp+';\n'
  aliases+=''.join('#define FG174M_F'+xy+' '+field+'\n'for xy,(_,field)in zip('XYZW',f4[1]));(S/'mma-types-generated.cuh').write_text('#pragma once\n#include <stdint.h>\n'+types+aliases);(S/'mma-functions-generated.cuh').write_text('#pragma once\n'+text+'\n#define FG174M_V32 '+entries[name][0]+'\n#define FG174M_V128 '+entries[name][1]+'\n');(S/'mma-pod-abi.json').write_text(json.dumps(dict(F4=f4,U4=u4,U2=u2),indent=2)+'\n')
 else:(S/'scalar-device-generated.cuh').write_text('#pragma once\n#include <stdint.h>\n#include <string.h>\n'+types+text+'\n#define FG174S_GDN '+entries[name][0]+'\n')
native=(S/'native-gated-delta-net-mma.cu').read_text().replace('ggml_cuda_gated_delta_net_mma','fg174m_native');native=re.sub(r'\bggml_cuda_gdn_mma_launch\b','fg174m_native_ggml_cuda_gdn_mma_launch',native);(S/'mma-native-prefixed.cuh').write_text(native.rstrip()+'\n')
native=(S/'native-gated_delta_net.cu').read_text()
for n in ['ggml_cuda_op_gated_delta_net','ggml_cuda_op_gated_delta_net_fused_cache','ggml_cuda_op_gated_delta_net_impl','gated_delta_net_cuda']:native=re.sub(r'\b'+n+r'\b','fg174s_native_'+n,native)
native=re.sub(r'\bggml_cuda_gdn_mma_launch\b','fg174m_native_ggml_cuda_gdn_mma_launch',native);(S/'scalar-native-combined.cuh').write_text(native.rstrip()+'\n');(S/'cuda-gdn.cu').write_text('#include "mma-native-prefixed.cuh"\n#include "scalar-native-combined.cuh"\n#include "scalar-projection.cuh"\n#include "scalar-device-generated.cuh"\n#include "mma-primitives.cuh"\n#include "mma-functions-generated.cuh"\n#include "scalar-bridge.cuh"\n#include "mma-bridge.cuh"\n')
oldmap=(F/'experiments/EXP-2026-11-08-171-goose-cuda-rope/source/visibility.map').read_text();(S/'visibility.map').write_text(oldmap.replace('local:','fg174s_cuda_host_seen; fg174s_cuda_device_read; fg174m_cuda_host_seen; fg174m_cuda_device_read; local:').replace(' };',' *fg174s_native*; *fg174m_native*; fg174s_*; fg174m_*; };'))
pins=json.loads((E/'external-header-pins.json').read_text());assert all(H(path)==sha for path,sha in pins.items()),'Pinned external header changed;review before rebuilding'
D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');db=json.loads((D/'compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/gated_delta_net.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(Path(r['file']).parent));cmd.insert(1,'-I'+str(S));cmd[cmd.index('-c')+1]=str(S/'cuda-gdn.cu');obj=B/'cuda-gdn.o';cmd[cmd.index('-o')+1]=str(obj);run('cuda-gdn-build',cmd,Path(r['directory']));hostobjs=[]
for name in ['scalar-host','mma-host']:
 host=B/(name+'.o');hostobjs.append(host);run(name+'-C-build',['/usr/bin/gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden','-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/(name+'-unit.c'),'-o',host])
past=json.loads((F/'runtime/exp171/build/commands.json').read_text());r=next(x for x in past if x['label']=='cuda-rope-link');cmd=r['argv'].copy();ref=B/'unchanged-exp171.so';cmd[cmd.index('-o')+1]=str(ref);cmd=[('-Wl,--dependency-file='+str(B/'unchanged-cuda.d'))if x.startswith('-Wl,--dependency-file=')else x for x in cmd];run('unchanged-exp171-link-integrity',cmd,Path(r['cwd']));base=F/'runtime/exp171/run/libggml-cuda.so';assert H(ref)==H(base);remove=[x for x in cmd if x.endswith(('/gated_delta_net.cu.o','/gated-delta-net-mma.cu.o'))];assert len(remove)==2;cmd=[x for x in cmd if x not in remove];cmd=[('-Wl,--version-script='+str(S/'visibility.map'))if x.startswith('-Wl,--version-script=')else x for x in cmd];cmd+=[str(obj),*map(str,hostobjs)];assert not any('test-only' in x for x in cmd);out=R/('libggml-cuda-gdn-'+hashlib.sha256((H(obj)+''.join(H(p)for p in hostobjs)+H(S/'visibility.map')+H(ref)).encode()).hexdigest()[:12]+'.so.0.27.0');linkout=B/'relink.so'if out.exists()else out;cmd[cmd.index('-o')+1]=str(linkout);run('cuda-gdn-link',cmd,Path(r['cwd']));
if linkout!=out:assert H(linkout)==H(out)
for p in (F/'runtime/exp171/run').iterdir():
 if p.is_file()or p.is_symlink():
  q=R/p.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(out if p.name.startswith('libggml-cuda')else p.resolve())
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_FULL_GDN_GPU_AND_APEX',compiler_sha256=H(C),entries=entries,library=str(out),sha256=H(out),previous_library_sha256=H(base),removed_objects=remove,rollback='GGML_GOOSE_CUDA_GDN=0 or immutable EXP171',test_only_adapter_linked=False,production_changed=False),indent=2)+'\n');print('LINKED_GOOSE_GDN',out)
