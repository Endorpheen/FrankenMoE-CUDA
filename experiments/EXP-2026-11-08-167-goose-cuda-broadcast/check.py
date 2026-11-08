from pathlib import Path
import json,os,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-167-goose-cuda-broadcast';S=E/'source';B=F/'runtime/exp167/build';R=F/'runtime/exp167/run';rows=json.loads((B/'commands.json').read_text());env=dict(os.environ,LD_LIBRARY_PATH=str(R));env.pop('LD_PRELOAD',None)
def run(label,args,cwd=B,extra={},expected=0):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=dict(env,**extra),capture_output=True,text=True,timeout=300);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2200:],flush=True)
 if q.returncode!=expected:print(q.stderr[-3500:]);raise SystemExit(q.returncode or 1)
 return q
r=next(v for v in rows if v['label']=='cuda-broadcast-build');cmd=r['argv'].copy();cmd[cmd.index('-c')+1]=str(S/'fixture.cu');cmd[cmd.index('-o')+1]=str(B/'fixture.o');run('fixture-build',cmd,Path(r['cwd']));run('fixture-link',['/usr/bin/g++-13','-o',B/'fixture',B/'fixture.o',B/'cuda-broadcast.o',B/'host-goose.o','-L'+str(R),'-Wl,-rpath,'+str(R),'-lggml-cuda','-lggml-base','-lggml','-lcudart','-lcublas','-ldl','-lpthread']);results=[]
for label,extra in [('gpu-parity',{}),('gpu-rollback',{'GGML_GOOSE_CUDA_BROADCAST':'0'})]:
 q=run(label,[B/'fixture'],extra=extra);results.append(dict(label=label,exit_code=q.returncode,stdout=q.stdout))
q=run('native-mixed-type-rejection',[B/'fixture','native-mixed'],expected=81);results.append(dict(label='native-mixed-type-rejection',exit_code=q.returncode,expected_exit_code=81,stderr=q.stderr[-1200:]));(E/'local-results.json').write_text(json.dumps(dict(status='PASS_GPU_BROADCAST_NATIVE_TYPED_REFERENCE_ROLLBACK',runs=results,production_changed=False),indent=2)+'\n')
