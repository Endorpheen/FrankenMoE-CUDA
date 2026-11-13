#!/usr/bin/env python3
"""Mutation gate for the transposed CPY tile."""
from pathlib import Path
import subprocess
F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-434-p3-03-cpy-transpose'
SRC = E / 'source' / 'device.goose'
ORIG = SRC.read_text()
MUTATIONS = [
 ('load tile transposed', '(ty+j)*(33 as i64)+tx', 'tx*(33 as i64)+(ty+j)'),
 ('write tile untransposed', 'cur*(1056 as i64)+tx*(33 as i64)+(ty+j)', 'cur*(1056 as i64)+(ty+j)*(33 as i64)+tx'),
 ('destination offset loses ne00', '(drow+j)*ne00+dcol', '(drow+j)+dcol'),
 ('source offset loses ne01', '(sy+j)*ne01+sx', '(sy+j)+sx'),
 ('destination column uses bx', 'let dcol:i64=by*(32 as i64)+tx;', 'let dcol:i64=bx*(32 as i64)+tx;'),
]
def run():
 r=subprocess.run(['bash',str(E/'build_fixture.sh')],text=True,capture_output=True,timeout=1800)
 if r.returncode: return None,'build failed'
 q=subprocess.run([str(F/'runtime/exp434/build/gpu-fixture')],text=True,capture_output=True,timeout=900)
 return q.returncode,(q.stdout.strip().splitlines()[-1] if q.stdout.strip() else '')
caught=0
try:
 for label,old,new in MUTATIONS:
  if old not in ORIG: print('[skip]',label); continue
  SRC.write_text(ORIG.replace(old,new,1))
  rc,line=run()
  if rc is None: print('[build-fail]',label); continue
  ok=rc!=0; caught+=ok
  print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
finally:
 SRC.write_text(ORIG); run()
print(f'mutations {caught}/{len(MUTATIONS)} caught')
