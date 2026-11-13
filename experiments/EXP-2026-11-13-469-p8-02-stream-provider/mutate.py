#!/usr/bin/env python3
from pathlib import Path
import subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-13-469-p8-02-stream-provider'
SRC=E/'source/provider.goose'; ORIG=SRC.read_text()
M=[
 ('failed write continues the stream','  action=1 as i64;','  action=2 as i64;'),
 ('sink never finishes',' if action==3 as i64 || action==5 as i64 { done=1 as i64; }',''),
 ('finish still asks for more',' if action==2 as i64 || action==4 as i64 { keep_going=1 as i64; }',' keep_going=1 as i64;'),
 ('stream predicate inverted',' if next_present!=0 as i64 { return 1 as i64; }',' if next_present==0 as i64 { return 1 as i64; }'),
 ('nginx header inverted',' if streaming!=0 as i64 { return 1 as i64; }',' if streaming==0 as i64 { return 1 as i64; }'),
 ('last chunk skipped, only finish','   if chunk_len>0 as i64 { action=3 as i64; } else { action=5 as i64; }','   action=5 as i64;'),
 ('complete ordering swapped',' put(out,0 as i64,if streaming!=0 as i64 {2 as i64} else {1 as i64});',' put(out,0 as i64,if streaming!=0 as i64 {1 as i64} else {2 as i64});'),
]
def run():
 r=subprocess.run(['bash',str(E/'build_fixture.sh')],text=True,capture_output=True,timeout=900)
 line=[x for x in r.stdout.splitlines() if x.startswith('FG469_PROVIDER')]
 if line: return r.returncode,line[-1]
 if 'wrote ' not in r.stdout: return None,'emission failed'
 last=r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'no stderr'
 return max(r.returncode,1),'runtime trap: '+last
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
