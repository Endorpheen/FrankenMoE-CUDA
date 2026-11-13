from pathlib import Path
import subprocess,json,os,hashlib,struct,re,shutil,math
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/cpu-backend';v=r/'variants/cpu-backend';b=v/'build';shutil.copy2('/tmp/fg143_fixture.c',s/'composite-fixture.c');cmd=['/usr/bin/gcc-13','-O2','-I'+str(r/'source/ggml/include'),str(s/'composite-fixture.c'),'-L'+str(v/'run'),'-lggml-cpu','-lggml-base','-lm','-lpthread','-Wl,-rpath,'+str(v/'run'),'-o',str(b/'composite-fixture')];p=subprocess.run(cmd,capture_output=True,text=True);(b/'composite-compile.log').write_text(p.stdout+p.stderr);(b/'composite-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('composite fixture build',p.returncode,p.stderr[-1000:],flush=True)
if p.returncode:raise SystemExit(p.returncode)
runs={};data={}
for arm,bundle in [('native',r/'run'),('candidate',v/'run')]:
 env=dict(os.environ);env.pop('LD_PRELOAD',None);env.update(LD_LIBRARY_PATH=str(bundle),GGML_GOOSE_ALLOC='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_ALLOC_VERIFY='0',GGML_GOOSE_CPU_BACKEND='1');output=r/'results'/('cpu-backend-composite-'+arm+'.bin');p=subprocess.run([str(b/'composite-fixture'),str(output)],env=env,capture_output=True,text=True,timeout=180);(r/'results'/('cpu-backend-composite-'+arm+'.jsonl')).write_text(p.stdout);(r/'results'/('cpu-backend-composite-'+arm+'.log')).write_text(p.stderr);runs[arm]={'exit_code':p.returncode,'cases':len(p.stdout.splitlines())};print(arm,runs[arm],p.stderr[-1200:],flush=True)
 if p.returncode:continue
 data[arm]=output.read_bytes()
if all(x['exit_code']==0 for x in runs.values()):
 meta=[json.loads(x) for x in (r/'results/cpu-backend-composite-native.jsonl').read_text().splitlines()];gotmeta=[json.loads(x) for x in (r/'results/cpu-backend-composite-candidate.jsonl').read_text().splitlines()];assert meta==gotmeta;offset=0;checks=[]
 for row in meta:
  n=row['bytes'];a=data['native'][offset:offset+n];bdata=data['candidate'][offset:offset+n];passed=a==bdata;maxabs=0.;violations=0
  if row['type']==0:
   aa=struct.unpack('<'+str(n//4)+'f',a);bb=struct.unpack('<'+str(n//4)+'f',bdata)
   for x,y in zip(aa,bb):
    if x==y or math.isnan(x) and math.isnan(y):continue
    if not math.isfinite(x) or not math.isfinite(y):violations+=1;continue
    error=abs(x-y);maxabs=max(maxabs,error)
    if error>2e-5+2e-5*abs(x):violations+=1
   passed=violations==0
  checks.append({**row,'exact':a==bdata,'within_predeclared_tolerance':passed,'max_absolute_error':maxabs,'violations':violations});offset+=n
 assert offset==len(data['native'])==len(data['candidate']);status='PASS_CURRENT_COMPOSITE_SUBSET_NOT_FULL_B1' if all(x['within_predeclared_tolerance'] for x in checks) else 'FAIL_COMPOSITE_CORRECTNESS';report={'status':status,'runs':runs,'checks':checks,'tolerance':'F32 abs2e-5+rel2e-5 chosen before execution; integer/raw equality exact; NaNs compare category only, not payload. This does not close strict NaN payload compatibility.','bytes':offset,'native_sha256':hashlib.sha256(data['native']).hexdigest(),'candidate_sha256':hashlib.sha256(data['candidate']).hexdigest(),'scope':'Actual GGML graph subset,1/3threads; not full operation/type/safety/model coverage and not complete B1. No timing or model run.'}
else:report={'status':'FAIL_COMPOSITE_PROCESS','runs':runs,'scope':'Inspect retained logs; do not report full batch correctness.'}
(r/'results/cpu-backend-composite-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'status':report['status'],'failed_cases':[x['name'] for x in report.get('checks',[]) if not x['within_predeclared_tolerance']]},indent=2))
