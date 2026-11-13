#!/usr/bin/env python3
from pathlib import Path
import subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-13-472-p7-09-schema-shape'
SRC=E/'source/schema.goose'; ORIG=SRC.read_text()
M=[
 ('zero maximum no longer empties the rule',' if max_items==0 as i64 {\n  kind=0 as i64;',' if max_items==0 as i64 {\n  kind=4 as i64;'),
 ('optional form needs min 1','  if min_items==0 as i64 && max_items==1 as i64 {','  if min_items==1 as i64 && max_items==1 as i64 {'),
 ('plus form taken for min 0','    if min_items==1 as i64 && has_max==0 as i64 { kind=2 as i64; }','    if min_items==0 as i64 && has_max==0 as i64 { kind=2 as i64; }'),
 ('separator recursion keeps min',' var inner_min:i64=min_items-(1 as i64);',' var inner_min:i64=min_items;'),
 ('separator wrapper dropped',' if min_items==0 as i64 { wrapped=1 as i64; }',' if min_items==99 as i64 { wrapped=1 as i64; }'),
 ('required and optional swapped','  if flags[i]!=0 as i64 { req=req+1 as i64; } else { opt=opt+1 as i64; }','  if flags[i]==0 as i64 { req=req+1 as i64; } else { opt=opt+1 as i64; }'),
 ('additionalProperties not counted',' if has_additional!=0 as i64 { opt=opt+1 as i64; }',''),
 ('comma prefix ignores optionals',' if req>0 as i64 && opt>0 as i64 { sep=1 as i64; }',' if req>0 as i64 { sep=1 as i64; }'),
]
def run():
 r=subprocess.run(['bash',str(E/'build_fixture.sh')],text=True,capture_output=True,timeout=600)
 line=[x for x in r.stdout.splitlines() if x.startswith('FG472_SCHEMA')]
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
