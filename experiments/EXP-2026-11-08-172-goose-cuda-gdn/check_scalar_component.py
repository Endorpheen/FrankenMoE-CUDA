from pathlib import Path
import json,re,shlex,subprocess,hashlib
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-172-goose-cuda-gdn';S=E/'source';B=F/'runtime/exp172/build';R=F/'runtime/exp172/run';B.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a';rows=[]
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
for name,prefix in [('scalar-device','fg174sd_'),('scalar-host','fg174sh_')]:
 run('emit-'+name,[C,'-o',S/(name+'-generated.c'),S/(name+'.goose')]);g=(S/(name+'-generated.c')).read_text();main=g[g.rfind('static void gg_function_0('):];calls=[n for n in re.findall(r'\b(gg_function_\d+)\(',main)if n!='gg_function_0'];assert len(calls)==1;entries[name]=prefix+calls[0]
 if name=='scalar-host':
  (S/'scalar-host-qualified.c').write_text(qualify(g,prefix));(S/'scalar-host-unit.c').write_text('#include "host-ffi.h"\n#define main fg174s_unused_main\n#include "scalar-host-qualified.c"\n#undef main\nvoid fg174s_host_entry(int64_t p){'+entries[name]+'(p,1);}\n');continue
 tm=re.search(r'(?m)^typedef struct[^\n]*gg_type_\d+',g);assert tm;start=g.rfind('\n\n',0,tm.start());types=g[start:g.index('static void gg_quote(')];funcs=[body(g,m)for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+)\([^;{}]*\)\s*\{',g)if m[1]!='gg_function_0'];helper='static void gg_fail(const char*){asm("trap;");}\n'
 for hn in ['gg_view','gg_double_bits','gg_index','gg_div','gg_mod','gg_udiv','gg_umod']:
  m=re.search(r'(?m)^static [^\n{]+? '+hn+r'\([^;{}]*\)\s*\{',g);assert m;helper+=body(g,m)+'\n'
 declarations='\n'.join(f[:f.index('{')].rstrip()+';'for f in funcs)+'\n';text=qualify(helper+declarations+'\n'.join(funcs),prefix);text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
 for forbidden in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']:assert forbidden not in text
 (S/'scalar-device-generated.cuh').write_text('#pragma once\n#include <stdint.h>\n#include <string.h>\n'+qualify('typedef struct { void *data; int64_t len; } gg_slice;\n'+types,prefix)+text+'\n#define FG174S_GDN '+entries[name]+'\n')
native=(S/'native-gated_delta_net.cu').read_text()
for n in ['ggml_cuda_op_gated_delta_net','ggml_cuda_op_gated_delta_net_fused_cache','ggml_cuda_op_gated_delta_net_impl','gated_delta_net_cuda']:native=re.sub(r'\b'+n+r'\b','fg174s_native_'+n,native)
(S/'scalar-native-prefixed.cuh').write_text(native.rstrip()+'\n');(S/'cuda-scalar.cu').write_text('#include "scalar-native-prefixed.cuh"\n#include "scalar-projection.cuh"\n#include "scalar-device-generated.cuh"\n#include "scalar-bridge.cuh"\n')
D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');db=json.loads((D/'compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/gated_delta_net.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(Path(r['file']).parent));cmd.insert(1,'-I'+str(S));cmd[cmd.index('-c')+1]=str(S/'cuda-scalar.cu');obj=B/'cuda-scalar.o';cmd[cmd.index('-o')+1]=str(obj);run('cuda-scalar-build',cmd,Path(r['directory']));run('scalar-host-C-build',['/usr/bin/gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden','-Werror=implicit-int','-Werror=implicit-function-declaration','-c',S/'scalar-host-unit.c','-o',B/'scalar-host.o']);run('scalar-component-test-stub-build',['/usr/bin/gcc-13','-O3','-c',S/'scalar-component-test-stub.c','-o',B/'test-only-no-mma.o']);fixture=cmd.copy();fixture.insert(1,'-DFG_GDN_SCALAR_COMPONENT_ONLY');fixture[fixture.index('-c')+1]=str(S/'fixture.cu');fixture[fixture.index('-o')+1]=str(B/'scalar-fixture.o');run('scalar-fixture-build',fixture,Path(r['directory']));R=F/'runtime/exp171/run';run('scalar-component-test-link',['/usr/bin/g++-13','-o',B/'scalar-fixture',B/'scalar-fixture.o',obj,B/'scalar-host.o',B/'test-only-no-mma.o','-L'+str(R),'-Wl,-rpath,'+str(R),'-lggml-cuda','-lggml-base','-lggml','-lcudart','-lcublas','-ldl','-lpthread'])
import os
env=dict(os.environ,LD_LIBRARY_PATH=str(R));env.pop('LD_PRELOAD',None);env.pop('GGML_GOOSE_CUDA_GDN',None);results=[]
for label,flags in [('scalar-component-gpu-parity',{}),('scalar-component-gpu-rollback',{'GGML_GOOSE_CUDA_GDN':'0'})]:
 p=subprocess.run([str(B/'scalar-fixture')],env=dict(env,**flags),capture_output=True,text=True,timeout=300);(B/(label+'.stdout')).write_text(p.stdout);(B/(label+'.stderr')).write_text(p.stderr);results.append(dict(label=label,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr));(E/'scalar-local-results.json').write_text(json.dumps(dict(status='RUNNING_OR_FAILED_COMPONENT_ONLY',runs=results),indent=2)+'\n');print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-7000:]);raise SystemExit(p.returncode)
(E/'scalar-local-results.json').write_text(json.dumps(dict(status='PASS_SCALAR_COMPONENT_ONLY_NOT_RUNTIME_ACCEPTANCE',runs=results,producer_sha256=H(C),entries=entries,excluded='Three native MMA-eligible test cases;test-only adapter returnsfalse;never link this adapter into Apex/runtime candidate. Full MMA integration/GPU/model gate pending.',production_changed=False),indent=2)+'\n')
