from pathlib import Path
import os,json,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-159-goose-graph-lifecycle';B=F/'runtime/exp159/build';S=E/'source';R=F/'runtime/exp159/run';rows=json.loads((B/'commands.json').read_text());env=dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_NO_BACKTRACE='1')
def run(label,args,cwd=B,env=env):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=180);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2200:],flush=True)
 if q.returncode:print(q.stderr[-4000:]);raise SystemExit(q.returncode)
 return q
r=next(x for x in rows if x['label']=='graph-core-build');build=r['argv'].copy();old=build[build.index('-o')+1];r_link=next(x for x in rows if x['label']=='graph-base-link');libraries=[]
for kind in ['native','candidate']:
 cmd=build.copy();cmd.insert(1,'-DFG159_TESTING');cmd[cmd.index('-c')+1]=str(S/(kind+'-probe.c'));obj=B/(kind+'-probe.o');cmd[cmd.index('-o')+1]=str(obj);run(kind+'-probe-build',cmd,Path(r['cwd']));cmd=[str(obj) if x==old else x for x in r_link['argv']];out=B/('libggml-'+kind+'-probe.so');cmd[cmd.index('-o')+1]=str(out);run(kind+'-probe-link',cmd,Path(r_link['cwd']));libraries.append(out)
run('graph-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src',S/'fixture.cpp','-ldl','-lpthread','-o',B/'graph-fixture']);args=[B/'graph-fixture']+libraries;p=run('graph-independent-native-corpus',args);q=run('graph-explicit-native-rollback',args,env=dict(env,GGML_GOOSE_GRAPH_LIFECYCLE='0'));(E/'local-results.json').write_text(json.dumps(dict(status='PASS_INFERENCE_GRAPH_HASH_TRAVERSAL_LIFECYCLE',goose=p.stdout,rollback=q.stdout,model_runs=0,production_changed=False,test_probe_scope='Wrappers expose unchanged EXP158 private algorithms only in a separate oracle DSO;candidate probe delegates actual production Goose algorithms;no test exports in production image'),indent=2)+'\n')

for kind in ['faults','concurrency']:
 run('graph-'+kind+'-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src',S/(kind+'.cpp'),'-ldl','-lpthread','-o',B/kind])
p=run('graph-failure-injection',[B/'faults',libraries[1]]);q=run('graph-independent-thread-lifecycle',[B/'concurrency',json.loads((S/'manifest.json').read_text())['library']]);path=E/'local-results.json';d=json.loads(path.read_text());d['failure_cleanup']=p.stdout;d['concurrent_graphs']=q.stdout;path.write_text(json.dumps(d,indent=2)+'\n')
