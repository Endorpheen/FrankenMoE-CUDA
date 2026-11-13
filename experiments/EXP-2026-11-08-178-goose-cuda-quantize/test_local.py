from pathlib import Path
import subprocess,json,shlex,os
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-08-178-goose-cuda-quantize'
B=F/'runtime/exp178/build';R=F/'runtime/exp178/run';S=E/'source';T=E/'tests'
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
r=next(x for x in db if x['file'].endswith('/quantize.cu'))
cmd=shlex.split(r['command'])
cmd.insert(1,'-I'+str(Path(r['file']).parent))
cmd.insert(1,'-I'+str(S))
cmd[cmd.index('-c')+1]=str(T/'gpu-fixture.cu')
cmd[cmd.index('-o')+1]=str(B/'gpu-fixture.o')
run('gpu-fixture-build',cmd,Path(r['directory']))
run('gpu-fixture-link',['g++-13',B/'gpu-fixture.o','-L'+str(R),'-Wl,-rpath,'+str(R),
    '-lggml-cuda','-lggml-base','-L/opt/cuda/targets/x86_64-linux/lib','-lcudart','-o',B/'gpu-fixture'])
run('gpu-fixture-goose',[B/'gpu-fixture'])
back=dict(env);back['GGML_GOOSE_CUDA_Q8_1']='0'
run('gpu-fixture-native',[B/'gpu-fixture'],env2=back)

print('EXP178_LOCAL_GATES_PASS')

# Export surface must differ from the base by exactly the two diagnostics.
base=run('base-exports',['nm','-D','--defined-only',F/'runtime/exp177/run/libggml-cuda.so']).stdout
new=run('candidate-exports',['nm','-D','--defined-only',R/'libggml-cuda.so']).stdout
names=lambda s:{x.split()[-1] for x in s.splitlines() if x.strip()}
added=names(new)-names(base);removed=names(base)-names(new)
assert added=={'fg178q_cuda_host_seen','fg178q_cuda_device_read'} and not removed,(added,removed)
import json as _j
(E/'export-check.json').write_text(_j.dumps(dict(status='PASS',added=sorted(added),removed=sorted(removed)),indent=2)+'\n')
print('EXP178_LOCAL_GATES_PASS')
