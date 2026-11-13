from pathlib import Path
import json,os,subprocess
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/cpu-backend/backend';v=r/'variants/cpu-backend';out=r/'results'
flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
cmd=json.loads((v/'build/backend-buffer-fixture-command.json').read_text())
cmd=cmd+flags
p=subprocess.run(cmd,capture_output=True,text=True);(v/'build/backend-buffer-fixture-sanitize.log').write_text(p.stdout+p.stderr)
if p.returncode:print(p.stderr[-1300:]);raise SystemExit(p.returncode)
env=dict(os.environ);env.pop('LD_PRELOAD',None);env.update(LD_LIBRARY_PATH=str(v/'run'),GGML_GOOSE_ALLOC='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_CPU_BACKEND='1',GGML_GOOSE_CPU_EXECUTOR='1',GGML_GOOSE_CPU_CONTROL='1',GGML_GOOSE_CPU_BACKEND_API='1',GGML_EXPERT_HOTSTORE='0',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
result=out/'backend-buffer-sanitized.bin'
p=subprocess.run([str(v/'build/backend-buffer-fixture'),str(result)],env=env,capture_output=True,text=True,timeout=180)
(out/'backend-buffer-sanitized.jsonl').write_text(p.stdout);(out/'backend-buffer-sanitized.log').write_text(p.stderr)
same=p.returncode==0 and result.read_bytes()==(out/'backend-buffer-goose-api.bin').read_bytes()
reports={'api':{'exit_code':p.returncode,'records':len(p.stdout.splitlines()),'bytes_equal_release':same}}
print('buffer sanitized',json.dumps(reports['api']),p.stderr[-800:],flush=True)
for arm,build,extra in [('release',v/'build',[]),('sanitized',v/'build',flags)]:
 cmd=['/usr/bin/gcc-13','-O1' if extra else '-O2',str(s/'allocation-contract.c'),str(build/'backend-goose.o'),'-lm','-o',str(build/'backend-allocation-contract')]+extra
 p=subprocess.run(cmd,capture_output=True,text=True);(build/'allocation-contract-build.log').write_text(p.stdout+p.stderr)
 if p.returncode:print(p.stderr[-1200:]);raise SystemExit(p.returncode)
 p=subprocess.run([str(build/'backend-allocation-contract')],env=env,capture_output=True,text=True,timeout=30);(out/('backend-allocation-'+arm+'.jsonl')).write_text(p.stdout);(out/('backend-allocation-'+arm+'.log')).write_text(p.stderr);reports[arm]={'exit_code':p.returncode,'cases':len(p.stdout.splitlines())};print('allocation policy',arm,reports[arm],p.stderr[-500:],flush=True)
passed=reports['api']['bytes_equal_release'] and all(reports[a]['exit_code']==0 and reports[a]['cases']==9 for a in ['release','sanitized'])
report={'status':'PASS_SCOPED_BACKEND_ASAN_UBSAN_LSAN_AND_ALLOCATION_POLICY' if passed else 'FAIL_BACKEND_SAFETY_GATE','checks':reports,'scope':'Sanitized buffer fixture and Goose policy against candidate libs; allocation mock provider, not OS OOM.','production_changed':False,'compiler_changed':False,'model_loaded':False}
(out/'backend-buffer-safety-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
