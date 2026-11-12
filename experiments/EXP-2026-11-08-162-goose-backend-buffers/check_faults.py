from pathlib import Path
import json,os,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-162-goose-backend-buffers';S=E/'source';B=F/'runtime/exp162/build';R=F/'runtime/exp162/run';rows=json.loads((B/'commands.json').read_text());env=dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_NO_BACKTRACE='1')
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=180);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2200:],flush=True)
 if q.returncode:print(q.stderr[-4000:]);raise SystemExit(q.returncode)
 return q
r=next(x for x in rows if x['label']=='buffers-backend-build');cmd=r['argv'].copy();old=cmd[cmd.index('-o')+1];obj=B/'backend-buffers-test.o';cmd.insert(1,'-DFG162_TESTING');cmd[cmd.index('-o')+1]=str(obj);run('buffers-fault-probe-build',cmd,Path(r['cwd']));link=next(x for x in rows if x['label']=='backend-candidate-probe-link');cmd=[str(obj) if x==old else x for x in link['argv']];assert cmd.count(str(obj))==1;library=B/'libbackend-fault.so';cmd[cmd.index('-o')+1]=str(library);run('buffers-fault-probe-link',cmd,Path(link['cwd']))
inc=['-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src']
for name in ['faults','concurrency']:run('buffers-'+name+'-build',['/usr/bin/g++-13','-std=c++17','-O2']+inc+[S/(name+'.cpp'),'-ldl','-lpthread','-o',B/name])
a=run('buffers-failure-injection',[B/'faults',library]);b=run('buffers-independent-threads',[B/'concurrency',library]);p=E/'local-results.json';d=json.loads(p.read_text());d['oom']=a.stdout;d['concurrency']=b.stdout;p.write_text(json.dumps(d,indent=2)+'\n')
