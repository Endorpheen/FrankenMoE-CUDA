from pathlib import Path
import json,os,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-169-goose-cuda-reductions';S=E/'source';B=F/'runtime/exp169/build';R=F/'runtime/exp169/run';rows=json.loads((B/'commands.json').read_text());base=next(r for r in rows if r['label']=='cuda-reductions-build');env=dict(os.environ,LD_LIBRARY_PATH=str(R));env.pop('LD_PRELOAD',None);env.pop('GGML_GOOSE_RMS',None);env.pop('GGML_GOOSE_CUDA_NORM',None);env.pop('GGML_GOOSE_CUDA_REDUCTIONS',None);results=[]
def run(label,args,cwd=B,extra={},expected=0):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=dict(env,**extra),capture_output=True,text=True,timeout=300);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-1500:],flush=True);results.append(dict(label=label,exit_code=q.returncode,expected_exit_code=expected,stdout=q.stdout,stderr=q.stderr[-3000:]));(E/'local-results.json').write_text(json.dumps(dict(status='RUNNING_OR_FAILED',runs=results),indent=2)+'\n')
 if q.returncode!=expected:print(q.stderr[-4500:]);raise SystemExit(q.returncode or 1)
 return q
for name in ['fixture','preflight']:
 cmd=base['argv'].copy();cmd[cmd.index('-c')+1]=str(S/(name+'.cu'));cmd[cmd.index('-o')+1]=str(B/(name+'.o'));run(name+'-build',cmd,Path(base['cwd']));run(name+'-link',['/usr/bin/g++-13','-o',B/name,B/(name+'.o'),B/'cuda-reductions.o',B/'host-goose.o','-L'+str(R),'-Wl,-rpath,'+str(R),'-lggml-cuda','-lggml-base','-lggml','-lcudart','-lcublas','-ldl','-lpthread'])
for label,extra in [('gpu-parity',{}),('gpu-rollback',{'GGML_GOOSE_CUDA_REDUCTIONS':'0'})]:run(label,[B/'fixture'],extra=extra)
run('preflight-run',[B/'preflight'])
(E/'local-results.json').write_text(json.dumps(dict(status='PASS_GPU_NATIVE_INDEPENDENT_GRAPH_ROLLBACK_PREFLIGHT',runs=results,production_changed=False),indent=2)+'\n')
