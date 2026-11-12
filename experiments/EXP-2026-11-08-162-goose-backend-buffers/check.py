from pathlib import Path
import json,os,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-162-goose-backend-buffers';S=E/'source';B=F/'runtime/exp162/build';R=F/'runtime/exp162/run';P=E.parent/'EXP-2026-11-08-161-goose-allocator';rows=json.loads((B/'commands.json').read_text());env=dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_NO_BACKTRACE='1')
def run(label,args,cwd=B,env=env):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=180);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2200:],flush=True)
 if q.returncode:print(q.stderr[-4000:]);raise SystemExit(q.returncode)
 return q
# Reuse the checked allocator source only for private test reflection;the backend objects are actual stage161/162.
past=json.loads((P/'build-commands.json').read_text());r=next(x for x in past if x['label']=='candidate-allocator-probe-build');cmd=r['argv'].copy();obj=B/'allocator-test-probe.o';cmd[cmd.index('-o')+1]=str(obj);run('allocator-test-probe-build',cmd,Path(r['cwd']));probes=[]
for kind,link in [('native',next(x for x in past if x['label']=='allocator-base-link')),('candidate',next(x for x in rows if x['label']=='buffers-base-link'))]:
 cmd=[str(obj) if x==str(F/'runtime/exp161/build/allocator.o') else x for x in link['argv']];assert cmd.count(str(obj))==1;out=B/('libbackend-'+kind+'.so');cmd[cmd.index('-o')+1]=str(out);run('backend-'+kind+'-probe-link',cmd,Path(link['cwd']));probes.append(out)
inc=['-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src']
for name in ['fixture','bulk','contracts']:run('buffers-'+name+'-build',['/usr/bin/g++-13','-std=c++17','-O2']+inc+[S/(name+'.cpp'),'-ldl','-o',B/name])
p=run('buffers-inherited-allocator-parity',[B/'fixture']+probes);q=run('buffers-native-rollback',[B/'fixture']+probes,env=dict(env,GGML_GOOSE_BACKEND_BUFFERS='0'));r=run('buffers-bulk-ownership',[B/'bulk']+probes);contracts=run('buffers-callback-contracts',[B/'contracts']+probes);(E/'local-results.json').write_text(json.dumps(dict(status='PASS_BACKEND_BUFFERS_BULK_OWNERSHIP_AND_ROLLBACK',inherited=p.stdout,rollback=q.stdout,bulk=r.stdout,contracts=contracts.stdout,model_runs=0,production_changed=False),indent=2)+'\n')
