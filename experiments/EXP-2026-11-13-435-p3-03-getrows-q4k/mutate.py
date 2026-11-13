#!/usr/bin/env python3
from pathlib import Path
import subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-13-435-p3-03-getrows-q4k'
SRC=E/'source/device.goose'; ORIG=SRC.read_text()
M=[
 ('scale mask 63 -> 31','&63) as i64','&31) as i64'),
 ('affine sign flips','d1*(lo as f32)-m1','d1*(lo as f32)+m1'),
 ('source row index shifted','i01*nb01+i11*nb02+i12*nb03','(i01+1)*nb01+i11*nb02+i12*nb03'),
 ('destination column shifted','i10*s1+i11*s2+i12*s3','i10*s1+1+i11*s2+i12*s3'),
 ('thread id halved','q4k(src_row,dst_row,ib,tx as i32);','q4k(src_row,dst_row,ib,(tx/(2 as i32)) as i32);'),
]
def run():
 r=subprocess.run(['bash',str(E/'build_fixture.sh')],text=True,capture_output=True,timeout=1800)
 if r.returncode: return None,'build failed'
 q=subprocess.run([str(F/'runtime/exp435/build/gpu-fixture')],text=True,capture_output=True,timeout=900)
 return q.returncode,(q.stdout.strip().splitlines()[-1] if q.stdout.strip() else '')
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
