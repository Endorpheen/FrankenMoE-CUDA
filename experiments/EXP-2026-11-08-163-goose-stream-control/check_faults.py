from pathlib import Path
import json,subprocess,os
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-163-goose-stream-control';S=E/'source';B=F/'runtime/exp163/build';R=F/'runtime/exp163/run';rows=json.loads((B/'commands.json').read_text());env=dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_NO_BACKTRACE='1')
def run(label,args):
 args=list(map(str,args));q=subprocess.run(args,cwd=B,env=env,capture_output=True,text=True,timeout=180);rows.append(dict(label=label,argv=args,cwd=str(B),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2200:],flush=True)
 if q.returncode:print(q.stderr[-3000:]);raise SystemExit(q.returncode)
 return q
inc=['-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src']
for name in ['faults','concurrency']:run('stream-'+name+'-build',['/usr/bin/g++-13','-std=c++17','-O2']+inc+[S/(name+'.cpp'),'-ldl','-lpthread','-o',B/name])
a=run('stream-invalid-and-staging-failures',[B/'faults',B/'libstream-candidate.so']);b=run('stream-independent-threads',[B/'concurrency',B/'libstream-candidate.so']);p=E/'local-results.json';d=json.loads(p.read_text());d['failure_checks']=a.stdout;d['concurrency']=b.stdout;d['exception_namespace']='Primary matches actual server;success parity remains independent namespaces';p.write_text(json.dumps(d,indent=2)+'\n')
