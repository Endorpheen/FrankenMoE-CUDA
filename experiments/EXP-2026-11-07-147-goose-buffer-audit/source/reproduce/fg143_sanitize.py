from pathlib import Path
import json,subprocess,os,hashlib,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');old=r/'variants/cpu-backend';v=r/'variants/cpu-backend-sanitize';(v/'build').mkdir(parents=True,exist_ok=True);(v/'run').mkdir(exist_ok=True);records=[]
for mapping in (old/'build').glob('rename-*.txt'):shutil.copy2(mapping,v/'build'/mapping.name)
def remap(a):return a.replace(str(old/'build')+'/',str(v/'build')+'/').replace(str(old/'run')+'/',str(v/'run')+'/')
for row in json.loads((old/'build/integration-commands.json').read_text()):
 cmd=[remap(a) for a in row['argv']];label=row['label']
 if not label.startswith('rename-'):
  cmd=[('-O1' if a=='-O3' else a) for a in cmd];cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
 p=subprocess.run(cmd,cwd=row['cwd'],capture_output=True,text=True);(v/'build'/(label+'.log')).write_text(p.stdout+p.stderr);print('safety build',label,p.returncode,(p.stdout+p.stderr)[-1000:],flush=True)
 if p.returncode:raise SystemExit(p.returncode)
 records.append({'label':label,'cwd':row['cwd'],'argv':cmd})
for path in (old/'run').iterdir():
 if path.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for suffix in ['.so','.so.0']:
 path=v/'run'/('libggml-cpu'+suffix)
 if not path.exists() and not path.is_symlink():path.symlink_to('libggml-cpu.so.0.27.0')
cmd=json.loads((old/'build/composite-command.json').read_text());cmd=[remap(a) for a in cmd]+['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'];p=subprocess.run(cmd,capture_output=True,text=True);(v/'build/fixture-build.log').write_text(p.stdout+p.stderr)
if p.returncode:print(p.stderr[-1000:]);raise SystemExit(p.returncode)
env=dict(os.environ);env.pop('LD_PRELOAD',None);env.update(LD_LIBRARY_PATH=str(v/'run'),GGML_GOOSE_ALLOC='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_ALLOC_VERIFY='0',GGML_GOOSE_CPU_BACKEND='1',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');output=r/'results/cpu-backend-composite-sanitized.bin';p=subprocess.run([str(v/'build/composite-fixture'),str(output)],env=env,capture_output=True,text=True,timeout=180);(r/'results/cpu-backend-composite-sanitized.jsonl').write_text(p.stdout);(r/'results/cpu-backend-composite-sanitized.log').write_text(p.stderr);same=p.returncode==0 and output.read_bytes()==(r/'results/cpu-backend-composite-candidate.bin').read_bytes();report={'status':'PASS_CURRENT_SCOPED_COMPOSITE_ASAN_UBSAN_LSAN' if p.returncode==0 and same else 'FAIL_CURRENT_SCOPED_COMPOSITE_SANITIZERS','exit_code':p.returncode,'cases':len(p.stdout.splitlines()),'bytes_equal_release_candidate':same,'instrumented':'New common Goose unit,modified CPU executor/core,new metadata/service bridge and composite fixture','uninstrumented':'Preserved native/earlier Goose objects,base and other runtime modules; no TSan,CUDAsafety,whole CPU/type/model coverage or safety-superiority claim','production_changed':False,'compiler_changed':False,'model_loaded':False};(r/'results/cpu-backend-composite-sanitizer-summary.json').write_text(json.dumps(report,indent=2)+'\n');(v/'build/commands.json').write_text(json.dumps(records,indent=2)+'\n');print(json.dumps(report,indent=2));
if p.returncode:print(p.stderr[-2500:])
if report['status'] == 'PASS_CURRENT_SCOPED_COMPOSITE_ASAN_UBSAN_LSAN':
 report['instrumented']='New common Goose unit,Goose graph executor,stateful module and CPU control module,modified CPU core,metadata bridges and composite fixture'
 (r/'results/cpu-backend-composite-sanitizer-summary.json').write_text(json.dumps(report,indent=2)+'\n')
 cmd=json.loads((old/'build/control-planner-command.json').read_text());cmd=[remap(a) for a in cmd]+['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'];p=subprocess.run(cmd,capture_output=True,text=True);(v/'build/control-planner-build.log').write_text(p.stdout+p.stderr)
 if p.returncode:print(p.stderr[-1500:]);raise SystemExit(p.returncode)
 plan_output=r/'results/cpu-control-planner-sanitized.bin';p=subprocess.run([str(v/'build/control-planner-fixture'),str(plan_output)],env=env,capture_output=True,text=True,timeout=180);(r/'results/cpu-control-planner-sanitized.jsonl').write_text(p.stdout);(r/'results/cpu-control-planner-sanitized.log').write_text(p.stderr);same=p.returncode==0 and plan_output.read_bytes()==(r/'results/cpu-control-planner-previous-b1.bin').read_bytes();report={'status':'PASS_SCOPED_PLANNER_ASAN_UBSAN_LSAN' if same else 'FAIL_SCOPED_PLANNER_SANITIZERS','exit_code':p.returncode,'cases':len(p.stdout.splitlines()),'bytes_equal_previous_b1':same,'scope':'Instrumented new Goose CPU control and modified core/metadata primitives,other retained objects uninstrumented; no whole-backend or model equivalence claim'};(r/'results/cpu-control-planner-sanitizer-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
 if p.returncode:print(p.stderr[-2500:]);raise SystemExit(p.returncode)
 if report['status'] == 'PASS_SCOPED_PLANNER_ASAN_UBSAN_LSAN':
  cmd=json.loads((r/'source/cpu-backend/apex-contract-command.json').read_text());cmd=[remap(a) for a in cmd]+['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'];p=subprocess.run(cmd,capture_output=True,text=True);(v/'build/apex-contract-build.log').write_text(p.stdout+p.stderr)
  if p.returncode:print(p.stderr[-1500:]);raise SystemExit(p.returncode)
  contract_output=r/'results/cpu-backend-apex-contract-sanitized.bin';p=subprocess.run([str(v/'build/apex-contract-fixture'),str(old/'build/apex-embedding-selected-rows.local.bin'),str(contract_output)],env=env,capture_output=True,text=True,timeout=180);(r/'results/cpu-backend-apex-contract-sanitized.jsonl').write_text(p.stdout);(r/'results/cpu-backend-apex-contract-sanitized.log').write_text(p.stderr);same=p.returncode==0 and contract_output.read_bytes()==(r/'results/cpu-backend-apex-contract-candidate.bin').read_bytes();report={'status':'PASS_SCOPED_APEX_CONTRACT_ASAN_UBSAN_LSAN' if same else 'FAIL_SCOPED_APEX_CONTRACT_SANITIZERS','exit_code':p.returncode,'cases':len(p.stdout.splitlines()),'bytes_equal_release_candidate':same,'scope':'Selected real embedding and wide GLU functional graphs; no full-model inference or whole-backend safety claim'};(r/'results/cpu-backend-apex-contract-sanitizer-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
  if p.returncode:print(p.stderr[-2500:]);raise SystemExit(p.returncode)
