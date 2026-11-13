from pathlib import Path
import subprocess,json,shlex,os
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-08-186-goose-cuda-mmvq-q6k'
B=F/'runtime/exp186/build';R=F/'runtime/exp186/run';S=E/'source';T=E/'tests'
rows=[]
env=dict(os.environ);env['LD_LIBRARY_PATH']=str(R)+':/opt/cuda/targets/x86_64-linux/lib:'+env.get('LD_LIBRARY_PATH','')
def run(label,args,cwd=B,env2=None):
    q=subprocess.run(list(map(str,args)),cwd=cwd,env=env2 or env,text=True,capture_output=True,timeout=900)
    (B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr)
    rows.append(dict(label=label,argv=[str(x) for x in args],cwd=str(cwd),exit_code=q.returncode))
    (E/'local-test-commands.json').write_text(json.dumps(rows,indent=2)+'\n')
    print('---',label,'rc=',q.returncode,flush=True)
    out=(q.stdout+q.stderr).strip()
    if out: print(out[-4000:],flush=True)
    if q.returncode and q.returncode!=77: raise SystemExit(q.returncode)
    return q

db=json.loads(Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/compile_commands.json').read_text())
r=next(x for x in db if x['file'].endswith('/mmvq.cu'))
cmd=shlex.split(r['command'])
cmd.insert(1,'-I'+str(Path(r['file']).parent))
cmd.insert(1,'-I'+str(S))
cmd[cmd.index('-c')+1]=str(T/'gpu-fixture.cu')
cmd[cmd.index('-o')+1]=str(B/'gpu-fixture.o')
run('gpu-fixture-build',cmd,Path(r['directory']))
run('gpu-fixture-link',['g++-13',B/'gpu-fixture.o','-L'+str(R),'-Wl,-rpath,'+str(R),
    '-lggml-cuda','-lggml-base','-L/opt/cuda/targets/x86_64-linux/lib','-lcudart','-o',B/'gpu-fixture'])
probe=list(cmd);probe[probe.index('-c')+1]=str(T/'probe.cu');probe[probe.index('-o')+1]=str(B/'probe.o')
run('probe-build',probe,Path(r['directory']))
run('probe-link',['g++-13',B/'probe.o','-L'+str(R),'-Wl,-rpath,'+str(R),
    '-lggml-cuda','-lggml-base','-L/opt/cuda/targets/x86_64-linux/lib','-lcudart','-o',B/'probe'])
run('probe-goose',[B/'probe'])
pb=dict(env);pb['GGML_GOOSE_CUDA_MMVQ_Q6K']='0'
run('probe-native',[B/'probe'],B,pb)
kb=list(cmd);kb[kb.index('-c')+1]=str(T/'kernel-bench.cu');kb[kb.index('-o')+1]=str(B/'kernel-bench.o')
run('kernel-bench-build',kb,Path(r['directory']))
run('kernel-bench-link',['g++-13',B/'kernel-bench.o','-L'+str(R),'-Wl,-rpath,'+str(R),
    '-lggml-cuda','-lggml-base','-L/opt/cuda/targets/x86_64-linux/lib','-lcudart','-o',B/'kernel-bench'])
run('gpu-fixture-goose',[B/'gpu-fixture'])
back=dict(env);back['GGML_GOOSE_CUDA_MMVQ_Q6K']='0'
run('gpu-fixture-native',[B/'gpu-fixture'],env2=back)

import subprocess as _sp, shutil as _sh, os as _os
_gd=B/'dump-goose';_nd=B/'dump-native'
for d in (_gd,_nd):
    _sh.rmtree(d,ignore_errors=True);d.mkdir(parents=True)
_e=dict(env)
_e['FG186Q_DUMP']=str(_gd);(_e.pop('GGML_GOOSE_CUDA_MMVQ_Q6K',None))
run('dump-goose',[B/'gpu-fixture'],env2=_e)
_e2=dict(env);_e2['FG186Q_DUMP']=str(_nd);_e2['GGML_GOOSE_CUDA_MMVQ_Q6K']='0'
run('dump-native',[B/'gpu-fixture'],env2=_e2)
_files=sorted(p.name for p in _gd.iterdir())
_same=[n for n in _files if (_nd/n).exists() and (_gd/n).read_bytes()==(_nd/n).read_bytes()]
assert len(_same)==len(_files) and _files, (_files,_same)
(E/'byte-compare.json').write_text(json.dumps(dict(status='PASS',identical=_same),indent=2)+'\n')
print('EXP186_LOCAL_GATES_PASS')

# Export surface must differ from the base by exactly the two diagnostics.
base=run('base-exports',['nm','-D','--defined-only',F/'runtime/exp185/run/libggml-cuda.so']).stdout
new=run('candidate-exports',['nm','-D','--defined-only',R/'libggml-cuda.so']).stdout
names=lambda s:{x.split()[-1] for x in s.splitlines() if x.strip()}
added=names(new)-names(base);removed=names(base)-names(new)
assert added=={'fg186q_cuda_host_seen','fg186q_cuda_device_read'} and not removed,(added,removed)
import json as _j
(E/'export-check.json').write_text(_j.dumps(dict(status='PASS',added=sorted(added),removed=sorted(removed)),indent=2)+'\n')
import subprocess as _sp, shutil as _sh, os as _os
_gd=B/'dump-goose';_nd=B/'dump-native'
for d in (_gd,_nd):
    _sh.rmtree(d,ignore_errors=True);d.mkdir(parents=True)
_e=dict(env)
_e['FG186Q_DUMP']=str(_gd);(_e.pop('GGML_GOOSE_CUDA_MMVQ_Q6K',None))
run('dump-goose',[B/'gpu-fixture'],env2=_e)
_e2=dict(env);_e2['FG186Q_DUMP']=str(_nd);_e2['GGML_GOOSE_CUDA_MMVQ_Q6K']='0'
run('dump-native',[B/'gpu-fixture'],env2=_e2)
_files=sorted(p.name for p in _gd.iterdir())
_same=[n for n in _files if (_nd/n).exists() and (_gd/n).read_bytes()==(_nd/n).read_bytes()]
assert len(_same)==len(_files) and _files, (_files,_same)
(E/'byte-compare.json').write_text(json.dumps(dict(status='PASS',identical=_same),indent=2)+'\n')
print('EXP186_LOCAL_GATES_PASS')
