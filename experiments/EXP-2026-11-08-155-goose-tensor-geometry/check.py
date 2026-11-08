from pathlib import Path
import json,subprocess,os
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-155-goose-tensor-geometry';B=F/'runtime/exp155/build';R=F/'runtime/exp155/run';rows=json.loads((B/'commands.json').read_text())
def run(label,args,env=None):
 args=list(map(str,args));p=subprocess.run(args,cwd=B,env=env,capture_output=True,text=True,timeout=120);rows.append(dict(label=label,argv=args,cwd=str(B),exit_code=p.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(p.stdout);(B/(label+'.stderr')).write_text(p.stderr);print(label,p.returncode,p.stdout[-3000:],flush=True)
 if p.returncode:print(p.stderr[-2000:]);raise SystemExit(p.returncode)
 return p
run('metadata-native-differential-build',['/usr/bin/g++-13','-std=c++17','-O2','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include',E/'source/fixture.cpp','-ldl','-o',B/'geometry-fixture'])
p=run('metadata-native-differential',[B/'geometry-fixture',(F/'runtime/run/libggml-base.so').resolve(),R/'libggml-base-geometry-exp150base.so.0.27.0'],dict(os.environ,LD_LIBRARY_PATH=str(R)))
rollback=run('metadata-explicit-native-rollback',[B/'geometry-fixture',(F/'runtime/run/libggml-base.so').resolve(),R/'libggml-base-geometry-exp150base.so.0.27.0'],dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_GOOSE_TENSOR_GEOMETRY='0'))
(E/'local-results.json').write_text(json.dumps(dict(status='PASS_INDEPENDENT_NATIVE_GEOMETRY',result=p.stdout,rollback=rollback.stdout,model_runs=0,production_changed=False),indent=2)+'\n')
