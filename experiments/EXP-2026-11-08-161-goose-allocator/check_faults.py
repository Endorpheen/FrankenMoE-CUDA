from pathlib import Path
import json,subprocess,os
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-161-goose-allocator';S=E/'source';B=F/'runtime/exp161/build';R=F/'runtime/exp161/run';rows=json.loads((B/'commands.json').read_text());env=dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_NO_BACKTRACE='1')
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=180);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2500:],flush=True)
 if q.returncode:print(q.stderr[-4000:]);raise SystemExit(q.returncode)
 return q
r=next(x for x in rows if x['label']=='candidate-allocator-probe-build');run('final-candidate-test-probe-build',r['argv'],Path(r['cwd']));r=next(x for x in rows if x['label']=='candidate-allocator-probe-link');run('final-candidate-test-probe-link',r['argv'],Path(r['cwd']))
for kind in ['faults','concurrency']:
 run('allocator-'+kind+'-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src',S/(kind+'.cpp'),'-ldl','-lpthread','-o',B/kind])
a=run('allocator-failure-injection',[B/'faults',B/'liballocator-candidate.so']);b=run('allocator-independent-threads',[B/'concurrency',B/'liballocator-candidate.so']);p=E/'local-results.json';d=json.loads(p.read_text());d['failure_injection']=a.stdout;d['concurrency']=b.stdout;p.write_text(json.dumps(d,indent=2)+'\n')
