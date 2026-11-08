from pathlib import Path
import json,os,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-156-goose-tensor-constructors';B=F/'runtime/exp156/build';R=F/'runtime/exp156/run';rows=json.loads((B/'commands.json').read_text());m=json.loads((E/'source/manifest.json').read_text())
def run(label,args,env=None):
 args=list(map(str,args));p=subprocess.run(args,cwd=B,env=env,capture_output=True,text=True,timeout=120);rows.append(dict(label=label,argv=args,cwd=str(B),exit_code=p.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(p.stdout);(B/(label+'.stderr')).write_text(p.stderr);print(label,p.returncode,p.stdout[-3000:],flush=True)
 if p.returncode:print(p.stderr[-3000:]);raise SystemExit(p.returncode)
 return p
run('constructors-fixture-build',['/usr/bin/g++-13','-std=gnu++17','-O2','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include',E/'source/fixture.cpp','-ldl','-o',B/'constructors-fixture'])
cmd=[B/'constructors-fixture',(F/'runtime/exp155/run/libggml-base.so').resolve(),m['library']];p=run('constructors-native-corpus',cmd,dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_NO_BACKTRACE='1'));q=run('constructors-explicit-rollback',cmd,dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_GOOSE_TENSOR_CONSTRUCTORS='0',GGML_NO_BACKTRACE='1'));(E/'local-results.json').write_text(json.dumps(dict(status='PASS_INDEPENDENT_NATIVE_CONSTRUCTOR_CORPUS',goose=p.stdout,rollback=q.stdout,model_runs=0,production_changed=False),indent=2)+'\n')
