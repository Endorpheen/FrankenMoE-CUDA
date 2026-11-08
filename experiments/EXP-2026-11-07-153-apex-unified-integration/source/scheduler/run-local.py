#!/usr/bin/env python3
"""Compile and verify scheduler control without a model, GPU execution or full rebuild."""
from pathlib import Path
import hashlib,json,os,subprocess
s=Path(__file__).resolve().parent;r=s.parent.parent;b=r/'build/scheduler';out=r/'results/scheduler'
compiler=r/'compiler/stage3-f849233b999c';pin=json.loads((s/'base.json').read_text())['compiler_sha256']
assert hashlib.sha256(compiler.read_bytes()).hexdigest()==pin,'Pinned compiler hash mismatch'
commands=json.loads((s/'commands.json').read_text());records=[]
def run(label,cmd,cwd,env=None):
 p=subprocess.run(cmd,cwd=cwd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 records.append(dict(label=label,argv=cmd,cwd=str(cwd),status=p.returncode,output=p.stdout))
 (out/'local-verification.json').write_text(json.dumps(records,indent=2)+'\n')
 print(label,p.returncode,p.stdout[-200:] if p.returncode else '')
 if p.returncode:raise SystemExit(p.returncode)
for label in ['kernel','unit','backend','link','cpu','mock']:
 item=commands[label];run(label,item['argv'],item['cwd'])
run_path=str(r/'variants/cpu-backend/run')
for name in ['cpu','mock']:
 for mode in ['0','1']:
  env=dict(os.environ,GGML_GOOSE_SCHEDULER=mode,GGML_GOOSE_SCHEDULER_TRACE='1',LD_LIBRARY_PATH=str(b)+':'+run_path)
  run(name+'-mode-'+mode,[str(b/(name+'-fixture'))],r,env)
for mode in ['0','1']:
 cpu=next(x for x in records if x['label']=='cpu-mode-'+mode)['output']
 mock=next(x for x in records if x['label']=='mock-mode-'+mode)['output']
 assert 'PASS_SCHEDULER_TYPED_LAYOUT' in cpu and '"checksum":42840' in cpu
 assert 'PASS_SCHEDULER_MOCK_ASYNC' in mock and '"cases":19' in mock
 assert ('FG153_SCHEDULER mode=13' in cpu)==(mode=='1')
print('PASS_LOCAL_SCHEDULER_DIFFERENTIAL_AND_OWNERSHIP')
