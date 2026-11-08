from pathlib import Path
import subprocess,json,shlex,os
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-174-goose-cuda-set-rows';B=F/'runtime/exp174/build';R=F/'runtime/exp174/run';S=E/'source';T=E/'tests';rows=[]
env=dict(os.environ);env['LD_LIBRARY_PATH']=str(R)+':/opt/cuda/targets/x86_64-linux/lib:'+env.get('LD_LIBRARY_PATH','')
def run(label,args,cwd=B,env2=env):
 q=subprocess.run(list(map(str,args)),cwd=cwd,env=env2,text=True,capture_output=True,timeout=240);(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);rows.append(dict(label=label,argv=list(map(str,args)),cwd=str(cwd),exit_code=q.returncode));(E/'local-test-commands.json').write_text(json.dumps(rows,indent=2)+'\n');print(label,q.returncode,(q.stdout+q.stderr)[-6000:] if 'exports' not in label else 'symbol table saved',flush=True)
 if q.returncode:raise SystemExit(q.returncode)
 return q
run('host-fixture-build',['g++-13','-std=c++17','-O2',T/'host-fixture.cpp',B/'host.o','-o',B/'host-fixture'])
run('host-fixture', [B/'host-fixture'])
db=json.loads(Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/compile_commands.json').read_text());r=next(x for x in db if x['file'].endswith('/set-rows.cu'));cmd=shlex.split(r['command']);cmd.insert(1,'-I'+str(Path(r['file']).parent));cmd[cmd.index('-c')+1]=str(T/'gpu-fixture.cu');cmd[cmd.index('-o')+1]=str(B/'gpu-fixture.o');run('gpu-fixture-build',cmd,Path(r['directory']))
run('gpu-fixture-link',['g++-13',B/'gpu-fixture.o','-L'+str(R),'-Wl,-rpath,'+str(R),'-lggml-cuda','-lggml-base','-L/opt/cuda/targets/x86_64-linux/lib','-lcudart','-o',B/'gpu-fixture'])
run('gpu-fixture',[B/'gpu-fixture'])
back=dict(env);back['GGML_GOOSE_CUDA_SET_ROWS']='0';run('gpu-fixture-rollback',[B/'gpu-fixture'],env2=back)
run('gpu-invalid',[B/'gpu-fixture','invalid'])
base=run('base-exports',['nm','-D','--defined-only',F/'runtime/exp173/run/libggml-cuda.so']).stdout
new=run('candidate-exports',['nm','-D','--defined-only',R/'libggml-cuda.so']).stdout
names=lambda s:{x.split()[-1] for x in s.splitlines() if x.strip()}
added=names(new)-names(base);removed=names(base)-names(new)
assert added=={'fg176r_cuda_host_seen','fg176r_cuda_device_read'} and not removed,(added,removed)
(E/'export-check.json').write_text(json.dumps(dict(status='PASS',added=sorted(added),removed=sorted(removed)),indent=2)+'\n')
print('EXP174_LOCAL_GATES_PASS')
