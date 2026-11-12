from pathlib import Path
import json,subprocess,os
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-157-goose-context-core';B=F/'runtime/exp157/build';R=F/'runtime/exp157/run';rows=json.loads((B/'commands.json').read_text());m=json.loads((E/'source/manifest.json').read_text())
def run(label,args,env=None):
 args=list(map(str,args));p=subprocess.run(args,cwd=B,env=env,capture_output=True,text=True,timeout=120);rows.append(dict(label=label,argv=args,cwd=str(B),exit_code=p.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(p.stdout);(B/(label+'.stderr')).write_text(p.stderr);print(label,p.returncode,p.stdout[-2500:],flush=True)
 if p.returncode:print(p.stderr[-3000:]);raise SystemExit(p.returncode)
 return p
run('context-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include',E/'source/fixture.cpp','-ldl','-lpthread','-o',B/'core-fixture'])
cmd=[B/'core-fixture',(F/'runtime/exp156/run/libggml-base.so').resolve(),m['library']];env=dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_NO_BACKTRACE='1');p=run('context-native-registry-lifecycle-corpus',cmd,env);q=run('context-explicit-native-rollback',cmd,dict(env,GGML_GOOSE_CONTEXT_CORE='0'));r=run('context-independent-thread-lifecycle',[B/'core-fixture','--concurrency',m['library']],env);(E/'local-results.json').write_text(json.dumps(dict(status='PASS_CONTEXT_REGISTRY_LIFETIME_AND_CONCURRENCY',goose=p.stdout,rollback=q.stdout,concurrency=r.stdout,model_runs=0,production_changed=False),indent=2)+'\n')
