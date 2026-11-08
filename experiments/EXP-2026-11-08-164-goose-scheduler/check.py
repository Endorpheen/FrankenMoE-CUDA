from pathlib import Path
import json,os,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-164-goose-scheduler';S=E/'source';B=F/'runtime/exp164/build';R=F/'runtime/exp164/run';P=E.parent/'EXP-2026-11-08-163-goose-stream-control';rows=json.loads((B/'commands.json').read_text());past=json.loads((P/'build-commands.json').read_text());env=dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_NO_BACKTRACE='1')
def run(label,args,cwd=B,env=env):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=180);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2200:],flush=True)
 if q.returncode:print(q.stderr[-4000:]);raise SystemExit(q.returncode)
 return q
probes=[]
for kind,build,link,old in [('native',next(x for x in past if x['label']=='stream-backend-build'),next(x for x in past if x['label']=='stream-base-link'),str(F/'runtime/exp163/build/backend-stream.o')),('candidate',next(x for x in rows if x['label']=='scheduler-backend-build'),next(x for x in rows if x['label']=='scheduler-base-link'),str(B/'backend-scheduler.o'))]:
 cmd=build['argv'].copy();cmd.insert(1,'-DFG164_TESTING');obj=B/('probe-'+kind+'.o');cmd[cmd.index('-c')+1]=str(S/('probe-'+kind+'.cpp'));cmd[cmd.index('-o')+1]=str(obj);run('stream-'+kind+'-probe-build',cmd,Path(build['cwd']));cmd=[str(obj) if x==old else x for x in link['argv']];assert cmd.count(str(obj))==1;out=B/('libstream-'+kind+'.so');cmd[cmd.index('-o')+1]=str(out);run('stream-'+kind+'-probe-link',cmd,Path(link['cwd']));probes.append(out)
run('scheduler-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src',S/'fixture.cpp','-ldl','-lpthread','-o',B/'fixture'])
results=[]
for label,extra,delta in [('parity',[],{}),('rollback',[],{'GGML_GOOSE_SCHEDULER':'0'}),('faults',['faults'],{}),('threads',['threads'],{})]:
 q=run('scheduler-'+label,[B/'fixture']+probes+extra,env=dict(env,**delta));results.append(dict(label=label,exit_code=q.returncode,stdout=q.stdout))
(E/'local-results.json').write_text(json.dumps(dict(status='PASS_SCHEDULER_NATIVE_PARITY_ROLLBACK_FAILURE_THREADS_EXPERT',runs=results,production_changed=False),indent=2)+'\n')
