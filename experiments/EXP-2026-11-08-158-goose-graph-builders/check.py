from pathlib import Path
import os,json,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-158-goose-graph-builders';B=F/'runtime/exp158/build';R=F/'runtime/exp158/run';rows=json.loads((B/'commands.json').read_text());m=json.loads((E/'source/manifest.json').read_text())
def run(label,args,env=None):
 args=list(map(str,args));q=subprocess.run(args,cwd=B,env=env,capture_output=True,text=True,timeout=120);rows.append(dict(label=label,argv=args,cwd=str(B),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2500:],flush=True)
 if q.returncode:print(q.stderr[-3000:]);raise SystemExit(q.returncode)
 return q
run('builders-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include',E/'source/fixture.cpp','-ldl','-o',B/'builders-fixture']);env=dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_NO_BACKTRACE='1');args=[B/'builders-fixture',(F/'runtime/exp157/run/libggml-base.so').resolve(),m['library']];p=run('builders-independent-corpus',args,env);q=run('builders-explicit-native-rollback',args,dict(env,GGML_GOOSE_GRAPH_BUILDERS='0'));(E/'local-results.json').write_text(json.dumps(dict(status='PASS_GRAPH_BUILDER_PARITY_AND_ROLLBACK',goose=p.stdout,rollback=q.stdout,model_runs=0,production_changed=False),indent=2)+'\n')
