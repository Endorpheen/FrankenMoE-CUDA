#!/usr/bin/env python3
from pathlib import Path
import subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-13-471-p8-02-limits'
SRC=E/'source/limits.goose'; ORIG=SRC.read_text()
M=[
 ('timeout default becomes 1800',' if explicit==0 as i64 { v=3600 as i64; }',' if explicit==0 as i64 { v=1800 as i64; }'),
 ('reuseaddr not requested',' put(out,0 as i64,1 as i64);',' put(out,0 as i64,0 as i64);'),
 ('reuseport applied even when unsupported','  if supported!=0 as i64 { rp=1 as i64; } else { warn=1 as i64; }','  rp=1 as i64;'),
 ('unsupported reuseport does not warn','  if supported!=0 as i64 { rp=1 as i64; } else { warn=1 as i64; }','  if supported!=0 as i64 { rp=1 as i64; }'),
 ('error handler owns every status',' if action_status==404 as i64 { own=1 as i64; }',' if action_status>=400 as i64 { own=1 as i64; }'),
]
def run():
 r=subprocess.run(['bash',str(E/'build_fixture.sh')],text=True,capture_output=True,timeout=600)
 line=[x for x in r.stdout.splitlines() if x.startswith('FG471_LIMITS')]
 if line: return r.returncode,line[-1]
 if 'wrote ' not in r.stdout: return None,'emission failed'
 return max(r.returncode,1),'runtime trap'
caught=0
try:
 for label,old,new in M:
  if old not in ORIG: print('[skip]',label); continue
  SRC.write_text(ORIG.replace(old,new,1)); rc,line=run()
  if rc is None: print('[build-fail]',label); continue
  ok=rc!=0; caught+=ok; print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
finally:
 SRC.write_text(ORIG); run()
print(f'mutations {caught}/{len(M)} caught')
