from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess

r = Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s = r/'source/cpu-backend/control'
b = r/'variants/cpu-backend/build'
out = r/'results'
shutil.copy2('/tmp/fg143_control_fixture.c', s/'planner-fixture.c')
cmd = ['/usr/bin/gcc-13','-O2','-I'+str(r/'source/ggml/include'),'-I'+str(r/'source/ggml/src'),
       str(s/'planner-fixture.c'),'-L'+str(r/'variants/cpu-backend/run'),'-lggml-cpu','-lggml-base',
       '-lm','-lpthread','-o',str(b/'control-planner-fixture')]
p = subprocess.run(cmd, capture_output=True, text=True)
(b/'control-planner-compile.log').write_text(p.stdout+p.stderr)
(b/'control-planner-command.json').write_text(json.dumps(cmd, indent=2)+'\n')
if p.returncode:
    print(p.stderr[-1800:]);raise SystemExit(p.returncode)
runs, records, binary = {}, {}, {}
for arm, bundle, control in [('previous-b1',r/'variants/cpu-backend-control-reference/run','0'),
                             ('goose-control',r/'variants/cpu-backend/run','1'),
                             ('rollback',r/'variants/cpu-backend/run','0')]:
    env = dict(os.environ)
    env.pop('LD_PRELOAD',None)
    for k in list(env):
        if k.startswith('GGML_GOOSE_'):env[k]='0'
    env.update(LD_LIBRARY_PATH=str(bundle),GGML_GOOSE_CPU_BACKEND='1',GGML_GOOSE_CPU_EXECUTOR='1',GGML_GOOSE_CPU_CONTROL=control)
    output = out/('cpu-control-planner-'+arm+'.bin')
    p = subprocess.run([str(b/'control-planner-fixture'),str(output)],env=env,capture_output=True,text=True,timeout=90)
    (out/('cpu-control-planner-'+arm+'.jsonl')).write_text(p.stdout)
    (out/('cpu-control-planner-'+arm+'.log')).write_text(p.stderr)
    runs[arm]={'exit_code':p.returncode,'cases':len(p.stdout.splitlines())}
    print(arm,runs[arm],p.stderr[-800:],flush=True)
    if p.returncode==0:
        records[arm]=[json.loads(line) for line in p.stdout.splitlines()]
        binary[arm]=output.read_bytes()
checks={};mismatches=[]
if len(binary)==3:
    for arm in ['goose-control','rollback']:
        checks[arm]=binary[arm]==binary['previous-b1'] and records[arm]==records['previous-b1']
        mismatches.extend({'arm':arm,'previous':a,'current':b} for a,b in zip(records['previous-b1'],records[arm]) if a!=b)
report={'status':'PASS_SELECTED_PLANNING_AND_ROLLBACK' if len(checks)==2 and all(checks.values()) else 'FAIL_CPU_PLANNING_CONTRACT',
        'runs':runs,'exact':checks,'mismatches':mismatches[:30],
        'sha256':{k:hashlib.sha256(v).hexdigest() for k,v in binary.items()},
        'scope':'Planning metadata corpus only,not operator compute or actual model. Includes every registered op,unary/GLU selectors,custom task limits and typed scratch branches.',
        'no_timings':True,'no_model_run':True,'production_changed':False}
(out/'cpu-control-planner-summary.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2),flush=True)
