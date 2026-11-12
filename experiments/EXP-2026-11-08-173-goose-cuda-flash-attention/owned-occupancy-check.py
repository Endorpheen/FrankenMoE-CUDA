from pathlib import Path
import json,subprocess,os,hashlib
F=Path(__file__).resolve().parents[2];E=F/'experiments/EXP-2026-11-08-173-goose-cuda-flash-attention';B=F/'runtime/exp173/build';R=F/'runtime/exp173/run'
env=dict(os.environ,LD_LIBRARY_PATH=str(R));env.pop('LD_PRELOAD',None)
for k in list(env):
 if k.startswith('GGML_GOOSE'):env.pop(k)
rows=[]
for stem in ['full-fixture','long-random-fixture']:
 out=B/(stem+'-owned-occupancy')
 link=['/usr/bin/g++-13','-o',str(out),str(B/(stem+'.o')),str(B/'cuda-runtime.o'),str(B/'host.o'),str(B/'native-fattn-host.o'),'-L'+str(R),'-Wl,-rpath,'+str(R),'-lggml-cuda','-lggml-base','-lggml','-lcudart','-lcublas','-ldl','-lpthread']
 for label,cmd,extra in [(stem+'-link',link,{}),(stem+'-goose',[str(out)],{}),(stem+'-original-rollback',[str(out)],{'GGML_GOOSE_CUDA_FA':'0'})]:
  q=subprocess.run(cmd,cwd=B,env=dict(env,**extra),capture_output=True,text=True,timeout=600);rows.append(dict(label=label,argv=cmd,exit_code=q.returncode,stdout=q.stdout,stderr=q.stderr[-7000:]));print(label,q.returncode,flush=True)
  (B/(label+'-owned-occupancy.stdout')).write_text(q.stdout);(B/(label+'-owned-occupancy.stderr')).write_text(q.stderr)
  (E/'owned-occupancy-local-results.json').write_text(json.dumps(dict(status='RUNNING_OR_FAILED',library=str((R/'libggml-cuda.so').resolve()),library_sha256=hashlib.sha256((R/'libggml-cuda.so').read_bytes()).hexdigest(),runs=rows,production_changed=False),indent=2)+'\n')
  if q.returncode:print((q.stdout+q.stderr)[-9000:]);raise SystemExit(q.returncode)
p=E/'owned-occupancy-local-results.json';d=json.loads(p.read_text());d['status']='PASS_WHOLE_FA_AND_LONG_RANDOM_NATIVE_BITWISE_ROLLBACK';p.write_text(json.dumps(d,indent=2)+'\n')
