#!/usr/bin/env python3
from pathlib import Path
import subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-13-470-p8-02-response'
SRC=E/'src/response.goose'; ORIG=SRC.read_text()
M=[
 ('plain body sent chunked',' if streaming!=0 as i64 { return 1 as i64; }\n return 0 as i64;\n}\n\n// 1 when X-Accel',' return 1 as i64;\n}\n\n// 1 when X-Accel'),
 ('accel override inverted',' if streaming!=0 as i64 { return 1 as i64; }\n return 0 as i64;\n}\n\n// The status',' if streaming==0 as i64 { return 1 as i64; }\n return 0 as i64;\n}\n\n// The status'),
 ('error status forced to 200',' if is_error!=0 as i64 { return payload_code; }',' if is_error!=0 as i64 { return 200 as i64; }'),
 ('models GET no longer skipped',' if cat==5 as i64 { return 1 as i64; }',' if cat==99 as i64 { return 1 as i64; }'),
 ('unknown route skipped too',' if cat==1 as i64 { return 1 as i64; }',' if cat>=0 as i64 { return 1 as i64; }'),
 ('handler content type beats the stream',' if streaming!=0 as i64 { kind=2 as i64; }',' if streaming!=0 as i64 { kind=1 as i64; }'),
]
def run():
 r=subprocess.run(['bash',str(E/'build_fixture.sh')],text=True,capture_output=True,timeout=600)
 line=[x for x in r.stdout.splitlines() if x.startswith('FG470_RESPONSE')]
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
