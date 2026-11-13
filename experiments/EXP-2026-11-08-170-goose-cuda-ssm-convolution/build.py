from pathlib import Path
import json,re,shlex,subprocess,hashlib
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-170-goose-cuda-ssm-convolution';S=E/'source';B=F/'runtime/exp170/build';R=F/'runtime/exp170/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
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
for nc in [3,4,5,7,9,15]:
 name='device-c'+str(nc);prefix='fg170s'+str(nc)+'_';source=(S/'device.goose').read_text().replace('f32[15]','f32['+str(nc)+']');(S/(name+'.goose')).write_text(source);run('emit-'+name,[C,'-o',S/(name+'-generated.c'),S/(name+'.goose')]);g=(S/(name+'-generated.c')).read_text();main=g[g.rfind('static void gg_function_0('):];calls=[n for n in re.findall(r'\b(gg_function_\d+)\(',main) if n!='gg_function_0'];assert len(calls)==2;entries[str(nc)]=[prefix+n for n in calls]
 tm=re.search(r'(?m)^typedef struct[^\n]*gg_type_\d+',g);assert tm;start=g.rfind('\n\n',0,tm.start());types=g[start:g.index('static void gg_quote(')];funcs=[body(g,m) for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+)\([^;{}]*\)\s*\{',g) if m[1]!='gg_function_0'];helper='static void gg_fail(const char*){asm("trap;");}\n'
 for hn in ['gg_view','gg_double_bits','gg_index','gg_div','gg_mod','gg_udiv','gg_umod']:
  m=re.search(r'(?m)^static [^\n{]+? '+hn+r'\([^;{}]*\)\s*\{',g);assert m;helper+=body(g,m)+'\n'
 declarations='\n'.join(f[:f.index('{')].rstrip()+';' for f in funcs)+'\n';text=qualify(helper+declarations+'\n'.join(funcs),prefix);text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
 for forbidden in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']:assert forbidden not in text
 header=name+'-generated.cuh';(S/header).write_text('#pragma once\n#include <stdint.h>\n#include <string.h>\n'+qualify('typedef struct { void *data; int64_t len; } gg_slice;\n'+types,prefix)+text+'\n');headers.append(header)
serial=[];long=[]
for nc in [3,4,5,7,9,15]:
 common='p.ptrs[0],p.ptrs[1],p.ptrs[2],p.ptrs[3],';strides='(int32_t)(p.strides[0][0]*4),(int32_t)(p.strides[0][1]*4),(int32_t)(p.strides[0][2]*4),(int32_t)(p.strides[1][1]*4),(int32_t)(p.strides[3][0]*4),(int32_t)(p.strides[3][1]*4),(int32_t)(p.strides[3][2]*4),'
 serial.append(('if' if nc==3 else 'else if')+' constexpr(NC=='+str(nc)+'){'+entries[str(nc)][0]+'('+common+strides+'(int32_t)p.iv[4],NC,SiLU,(uint8_t)(p.iv[1]!=0),threadIdx.x,blockIdx.x,blockIdx.y,1);}')
 long.append(('if' if nc==3 else 'else if')+' constexpr(NC=='+str(nc)+'){'+entries[str(nc)][1]+'('+common+'(uint64_t)(uintptr_t)smem,'+strides+'(int32_t)p.iv[4],NC,SiLU,(uint8_t)(p.iv[1]!=0),1,threadIdx.x,blockIdx.x,blockIdx.y,blockIdx.z,1);}')
(S/'entries.cuh').write_text('#define FG170S_SERIAL_DISPATCH '+' '.join(serial)+'\n#define FG170S_LONG_DISPATCH '+' '.join(long)+'\n')
run('emit-host',[C,'-o',S/'host-generated.c',S/'host.goose']);g=(S/'host-generated.c').read_text();main=g[g.rfind('static void gg_function_0('):];calls=[n for n in re.findall(r'\b(gg_function_\d+)\(',main) if n!='gg_function_0'];assert len(calls)==1;entries['host']='fg170sh_'+calls[0];(S/'host-qualified.c').write_text(qualify(g,'fg170sh_'));(S/'host-unit.c').write_text('#include "host-ffi.h"\n#define main fg170s_unused_main\n#include "host-qualified.c"\n#undef main\nvoid fg170s_host_entry(int64_t p){'+entries['host']+'(p,1);}\n')
native=(S/'native-ssm-conv.cu').read_text();native=re.sub(r'\bggml_cuda_op_ssm_conv\b','fg170s_native_ggml_cuda_op_ssm_conv',native);(S/'native-prefixed.cuh').write_text(native.rstrip()+'\n');(S/'cuda-ssm.cu').write_text('#include "native-prefixed.cuh"\n#include "projection.cuh"\n'+''.join('#include "'+h+'"\n' for h in headers)+'#include "entries.cuh"\n#include "cuda-bridge.cuh"\n')
oldmap=(F/'experiments/EXP-2026-11-08-169-goose-cuda-reductions/source/topk/visibility.map').read_text();(S/'visibility.map').write_text(oldmap.replace('local:','fg170s_cuda_host_seen; fg170s_cuda_device_read; local:').replace(' };',' *fg170s_native*; *ssm_conv_f32*; *ssm_conv_long_token_f32*; fg170s_*; };'))
D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');db=json.loads((D/'compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/ssm-conv.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(Path(r['file']).parent));cmd.insert(1,'-I'+str(S));cmd[cmd.index('-c')+1]=str(S/'cuda-ssm.cu');obj=B/'cuda-ssm.o';cmd[cmd.index('-o')+1]=str(obj);run('cuda-ssm-build',cmd,Path(r['directory']));host=B/'host-goose.o';run('host-goose-C-build',['/usr/bin/gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden','-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/'host-unit.c','-o',host]);past=json.loads((F/'runtime/exp169/topk-build/commands.json').read_text());r=next(x for x in past if x['label']=='cuda-routing-link');cmd=r['argv'].copy();ref=B/'unchanged-exp169.so';cmd[cmd.index('-o')+1]=str(ref);cmd=[('-Wl,--dependency-file='+str(B/'unchanged-cuda.d')) if x.startswith('-Wl,--dependency-file=') else x for x in cmd];run('unchanged-exp169-link-integrity',cmd,Path(r['cwd']));base=F/'runtime/exp169/run/libggml-cuda.so';assert H(ref)==H(base);remove=[x for x in cmd if x.endswith('/ssm-conv.cu.o')];assert len(remove)==1;cmd=[x for x in cmd if x not in remove];cmd=[('-Wl,--version-script='+str(S/'visibility.map')) if x.startswith('-Wl,--version-script=') else x for x in cmd];cmd+=[str(obj),str(host)];out=R/('libggml-cuda-ssm-'+hashlib.sha256((H(obj)+H(host)+H(S/'visibility.map')+H(ref)).encode()).hexdigest()[:12]+'.so.0.27.0');linkout=B/'relink.so' if out.exists() else out;cmd[cmd.index('-o')+1]=str(linkout);run('cuda-ssm-link',cmd,Path(r['cwd']));
if linkout!=out:assert H(linkout)==H(out)
for p in (F/'runtime/exp169/run').iterdir():
 if p.is_file() or p.is_symlink():
  q=R/p.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(out if p.name.startswith('libggml-cuda') else p.resolve())
(S/'manifest.json').write_text(json.dumps(dict(status='LINKED_PENDING_GPU_AND_APEX',compiler_sha256=H(C),entries=entries,library=str(out),sha256=H(out),previous_library_sha256=H(base),removed_objects=remove,rollback='GGML_GOOSE_CUDA_SSM=0 or immutable EXP169',production_changed=False),indent=2)+'\n');print('LINKED_GOOSE_SSM',out)
