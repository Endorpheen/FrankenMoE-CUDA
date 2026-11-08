from pathlib import Path
import json,os,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-166-goose-cuda-activations';S=E/'source';B=F/'runtime/exp166/build';R=F/'runtime/exp166/run';rows=json.loads((B/'commands.json').read_text());env=dict(os.environ,LD_LIBRARY_PATH=str(R));env.pop('LD_PRELOAD',None)
def run(label,args,cwd=B,extra={}):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=dict(env,**extra),capture_output=True,text=True,timeout=300);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2200:],flush=True)
 if q.returncode:print(q.stderr[-3500:]);raise SystemExit(q.returncode)
 return q
r=next(v for v in rows if v['label']=='cuda-activations-build');results=[]
for name in ['fixture','faults']:
 cmd=r['argv'].copy();cmd[cmd.index('-c')+1]=str(S/(name+'.cu'));cmd[cmd.index('-o')+1]=str(B/(name+'.o'));run(name+'-build',cmd,Path(r['cwd']));run(name+'-link',['/usr/bin/g++-13','-o',B/name,B/(name+'.o'),B/'cuda-activations.o',B/'host-goose.o','-L'+str(R),'-Wl,-rpath,'+str(R),'-lggml-cuda','-lggml-base','-lggml','-lcudart','-lcublas','-ldl','-lpthread'])
for label,binary,extra in [('gpu-parity','fixture',{}),('gpu-rollback','fixture',{'GGML_GOOSE_CUDA_ACTIVATIONS':'0'}),('host-preflight','faults',{})]:
 q=run(label,[B/binary],extra=extra);results.append(dict(label=label,exit_code=q.returncode,stdout=q.stdout))
(E/'local-results.json').write_text(json.dumps(dict(status='PASS_GPU_NATIVE_BIT_EXACT_ROLLBACK_AND_HOST_PREFLIGHT',runs=results,production_changed=False),indent=2)+'\n')
