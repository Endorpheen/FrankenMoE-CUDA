from pathlib import Path
import json,subprocess,shutil,hashlib,os
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');S=A/'source/context-control';B=A/'build/context-control';I=A/'source/context-io';IB=A/'build/context-io';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
p=S/'context-services.cpp';s=p.read_text();s=s.replace('#include "fg107-route.h"','#include "fg107-route.h"\n#include "fgcio-context-adapter.h"')
for old,new in [
('case 50: F.wio = std::make_unique<Fg107WriteDummy>(a != 0); return 0;','case 50: F.wio.reset(fgcio_adapter_write_dummy(a != 0)); if (!F.wio) throw std::bad_alloc(); return 0;'),
('case 51: F.wio = std::make_unique<Fg107WriteHost>((uint8_t *) (intptr_t) a, (size_t) b); return 0;','case 51: F.wio.reset(fgcio_adapter_write_host((uint8_t *) (intptr_t) a, (size_t) b)); if (!F.wio) throw std::bad_alloc(); return 0;'),
('case 52: F.rio = std::make_unique<Fg107ReadHost>((const uint8_t *) (intptr_t) a, (size_t) b); return 0;','case 52: F.rio.reset(fgcio_adapter_read_host((const uint8_t *) (intptr_t) a, (size_t) b)); if (!F.rio) throw std::bad_alloc(); return 0;'),
('case 63: return fg107_context_access::mem_storage(c).count((llama_seq_id) a) != 0;','case 63: return fgcio_context_has((i64)(intptr_t)c, a);')]:
 assert old in s;s=s.replace(old,new)
start=s.index('            case 64: (void) a;');end=s.index('            case 66:',start)
s=s[:start]+'''            case 64: {
                const auto handle = fgcio_context_write_device((i64)(intptr_t)c, d, a, b);
                F.wio.reset(fgcio_adapter_write_device(handle, (uint8_t *)(intptr_t)a, (size_t)b));
                if (!F.wio) throw std::bad_alloc();
                return 0;
            }
            case 65: {
                const auto handle = fgcio_context_read_device((i64)(intptr_t)c, d, a, b);
                F.rio.reset(fgcio_adapter_read_device_take(handle));
                if (!F.rio) throw std::bad_alloc();
                return 0;
            }
'''+s[end:];p.write_text(s)
p=S/'patched/llama-context.cpp';s=p.read_text().replace('#include "fg107-route.h"','#include "fg107-route.h"\n#include "fgcio-context-adapter.h"');s=s.replace('llama_context::~llama_context() {\n    {','llama_context::~llama_context() {\n    fgcio_context_drop((int64_t)(intptr_t)this);\n    {',1);p.write_text(s)
shutil.copyfile(A/'helpers/omo5/worker-4/active-context-closure/pin-seam.cpp',S/'pin-seam.cpp')
records=[]
def run(label,args,cwd=A,env=None):
 args=[str(x) for x in args];q=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=240);records.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'join-commands.json').write_text(json.dumps(records,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-7000:]);raise SystemExit(q.returncode)
 return q
incs=['-I'+str(S),'-I'+str(I),'-I'+str(A/'source/canonical-io'),'-I'+str(S/'native/src'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src')]
for src,name in [('patched/llama-context.cpp','context'),('context-services.cpp','services'),('pin-seam.cpp','compat')]:
 run('join-context-'+name,['/usr/bin/g++-13','-std=c++17','-O2','-fPIC','-fno-strict-aliasing','-DLLAMA_BUILD','-DLLAMA_SHARED','-DGGML_USE_CUDA',*incs,'-c',S/src,'-o',B/(name+'.o')])
row=next(r for r in json.loads((A/'build/kv-active/commands.json').read_text()) if r['label']=='kv-state-cohort-link');args=row['argv'].copy();old='/home/end0/AI/LlamaServer/exp107-expert-overlap/build/context.o';assert args.count(old)==1;args=[str(B/'context.o') if x==old else x for x in args]
objs=[B/(n+'.o') for n in ['unit-exec','unit-resource','unit-policy','services','compat']]+[IB/(n+'.o') for n in ['unit','abi','adapter']]+[A/'build/canonical-io'/x for x in ['unit.o','abi.o','set-abi.o','real-effects.o']]+[A/'build/canonical-storage-composed/libfgc-apex-storage.a']
args += [str(o) for o in objs];digest=hashlib.sha256(b''.join(p.read_bytes() for p in [B/'context.o',*objs])).hexdigest()[:12];out=R/('libllama-apex-context-io-'+digest+'.so.0.7.0');assert not out.exists();args[args.index('-o')+1]=str(out);run('join-context-cohort-link',args,Path(row['cwd']))
T=B/'run'
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(out if f.name in ['libllama.so','libllama.so.0','libllama.so.0.7.0'] else f.resolve())
env=dict(os.environ,LD_LIBRARY_PATH=str(T)+':'+str(R))
run('join-context-dlopen',['python3','-c','import ctypes,os;ctypes.CDLL('+repr(str(out))+',mode=os.RTLD_NOW);print("PASS full current cohort resolves")'],env=env)
base_env=dict(os.environ,LD_LIBRARY_PATH=str(R));binary=B/'actual-context-fixture'
base=run('join-context-original-host-state',[binary],env=base_env)
candidate=run('join-context-goose-host-state',[binary],env=env)
assert base.stdout==candidate.stdout,(base.stdout,candidate.stdout)
summary=dict(status='PASS_CONTEXT_AND_CANONICAL_IO_SHARED_COHORT',library=str(out),sha256=hashlib.sha256(out.read_bytes()).hexdigest(),previous_library=str((R/'libllama.so').resolve()),host_state_equal=True,stdout=candidate.stdout,device_io_fixture='context-io-real.json',production_changed=False,model_loads=0,pending='Combined CUDA/loader/vocab/server integration and full Apex inference')
(A/'results/accepted-context-io.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
checkpoint=dict(strategy='Integrate all prepared Apex blocks into one common candidate;no separate Batch A acceptance barrier',llama=summary,ggml_cpu=json.loads((A/'results/repack-control-linked.json').read_text()),kv_state=json.loads((A/'results/accepted-kv-state.json').read_text()),production_changed=False)
(A/'results/current-cohort-checkpoint.json').write_text(json.dumps(checkpoint,indent=2)+'\n');print(candidate.stdout);print('PASS context joined with all prior CPU/KV/graph modules;candidate aliases updated only.')
