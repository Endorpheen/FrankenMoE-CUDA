from pathlib import Path
import hashlib,json,os,re,subprocess
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/cpu-backend/backend';b=r/'variants/cpu-backend/build';out=r/'results';native=(r/'source/ggml/include/ggml.h').read_text();native=re.sub(r'/\*.*?\*/|//[^\n]*','',native,flags=re.S);names=[k for k,v in re.findall(r'(GGML_TYPE_[A-Z0-9_]+)\s*=\s*(\d+)',native) if k!='GGML_TYPE_COUNT'];interface=json.loads((s/'interface-map.json').read_text());text=Path(str(r/'source/cpu-backend/reproduce/fg146_backend_fixture.c')).read_text().replace('ACTIVE_TYPES',','.join(names)).replace('PROC_NAMES',','.join(json.dumps(x) for x in interface['proc_names']));(s/'fixture.c').write_text(text)
cmd=['/usr/bin/gcc-13','-O2','-I'+str(r/'source/ggml/include'),'-I'+str(r/'source/ggml/src'),str(s/'fixture.c'),'-L'+str(r/'variants/cpu-backend/run'),'-lggml-cpu','-lggml-base','-lm','-lpthread','-o',str(b/'backend-buffer-fixture')];p=subprocess.run(cmd,capture_output=True,text=True);(b/'backend-buffer-fixture-build.log').write_text(p.stdout+p.stderr);(b/'backend-buffer-fixture-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('backend fixture compile',p.returncode,p.stderr[-1500:],flush=True)
if p.returncode:raise SystemExit(p.returncode)
runs={};outputs={};metadata={}
for arm,bundle,enabled in [('previous-b1',r/'variants/cpu-backend-api-reference/run','0'),('goose-api',r/'variants/cpu-backend/run','1'),('rollback',r/'variants/cpu-backend/run','0')]:
 env=dict(os.environ);env.pop('LD_PRELOAD',None)
 for k in list(env):
  if k.startswith('GGML_GOOSE_'):env.pop(k)
 env.update(LD_LIBRARY_PATH=str(bundle),GGML_GOOSE_ALLOC='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_CPU_BACKEND='1',GGML_GOOSE_CPU_EXECUTOR='1',GGML_GOOSE_CPU_CONTROL='1',GGML_GOOSE_CPU_BACKEND_API=enabled,GGML_EXPERT_HOTSTORE='0')
 output=out/('backend-buffer-'+arm+'.bin');p=subprocess.run([str(b/'backend-buffer-fixture'),str(output)],env=env,capture_output=True,text=True,timeout=180);(out/('backend-buffer-'+arm+'.jsonl')).write_text(p.stdout);(out/('backend-buffer-'+arm+'.log')).write_text(p.stderr);runs[arm]={'exit_code':p.returncode,'records':len(p.stdout.splitlines())};print(arm,runs[arm],p.stderr[-900:],flush=True)
 if p.returncode==0:outputs[arm]=output.read_bytes();metadata[arm]=[json.loads(line) for line in p.stdout.splitlines()]
equal={};mismatches=[]
if len(outputs)==3:
 for arm in ['goose-api','rollback']:
  equal[arm]=outputs[arm]==outputs['previous-b1'] and metadata[arm]==metadata['previous-b1']
  if not equal[arm] and metadata[arm]==metadata['previous-b1']:
   offset=0
   for row in metadata[arm]:
    n=row['bytes']
    if outputs[arm][offset:offset+n]!=outputs['previous-b1'][offset:offset+n]:mismatches.append({'arm':arm,'name':row['name']})
    offset+=n
report={'status':'PASS_SELECTED_BACKEND_BUFFER_AND_ROLLBACK' if len(equal)==2 and all(equal.values()) else 'FAIL_BACKEND_API_FUNCTIONAL_GATE','runs':runs,'exact':equal,'mismatches':mismatches,'types':len(names),'capability_queries':101*len(names)*8,'source_sha256':hashlib.sha256((s/'kernel.goose').read_bytes()).hexdigest(),'scope':'Actual backend/device/registry API,capability matrix,buffer wrappers,context/pool changes,work growth/reuse,planned/direct compute,abort and recovery;1/2/8/16workers. Not full type/operator/physical ownership/model coverage.','model_loaded':False,'microbenchmarks':False,'production_changed':False};(out/'backend-buffer-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
