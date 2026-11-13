from pathlib import Path
import json,os,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-160-goose-shared-fusion';S=E/'source';B=F/'runtime/exp160/build';R=F/'runtime/exp160/run';rows=json.loads((B/'commands.json').read_text());m=json.loads((S/'manifest.json').read_text());env=dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_NO_BACKTRACE='1')
def run(label,args,env=env):
 args=list(map(str,args));q=subprocess.run(args,cwd=B,env=env,capture_output=True,text=True,timeout=120);rows.append(dict(label=label,argv=args,cwd=str(B),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2000:],flush=True)
 if q.returncode:print(q.stderr[-3000:]);raise SystemExit(q.returncode)
 return q
includes=['-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src'];probes=[]
for kind,library in [('native',(F/'runtime/exp159/run/libggml-base.so').resolve()),('candidate',m['libraries']['base']['path'])]:
 out=B/('libprobe-'+kind+'.so');flags=['-DFG160_CANDIDATE'] if kind=='candidate' else [];run('shared-'+kind+'-probe',['/usr/bin/g++-13','-std=c++17','-O2','-DNDEBUG','-shared','-fPIC']+includes+flags+[S/'probe.cpp',library,'-Wl,--disable-new-dtags,-rpath,'+str(Path(library).parent),'-o',out]);probes.append(out)
run('shared-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2']+includes+[S/'fixture.cpp','-ldl','-o',B/'shared-fixture']);args=[B/'shared-fixture']+probes;p=run('shared-native-parity',args);q=run('shared-fusion-native-rollback',args,env=dict(env,GGML_GOOSE_FUSION='0'));r=run('shared-all-native-rollback',args,env=dict(env,GGML_GOOSE_FUSION='0',GGML_GOOSE_GRAPH_LIFECYCLE='0'));(E/'local-results.json').write_text(json.dumps(dict(status='PASS_SHARED_FUSION_PARAMETERS_AND_COMPOSITION',goose=p.stdout,fusion_rollback=q.stdout,all_rollback=r.stdout,model_runs=0,production_changed=False),indent=2)+'\n')
