#!/usr/bin/env python3
from pathlib import Path
import subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-13-488-p7-09-ac';S=E/'source/ac.goose';O=S.read_text()
M=[('fail links keep self','if fe>=0 && eto[fe]!=v { fail[v]=eto[fe]; } else { fail[v]=0; }','if fe>=0 { fail[v]=v; } else { fail[v]=0; }'),
   ('terminal propagation stops at the node','else if u!=0 { mt[u]=mt[fail[u]]; }','else if u!=0 { mt[u]=-1; }'),
   ('next does not follow fail links','if e>=0 { return eto[e]; }\n  state=fail[state];','if e>=0 { return eto[e]; }\n  return 0;'),
   ('duplicates overwrite the pattern','if pat[node]<0 {\n  pat[node]=st[2];','if pat[node]>=-2 {\n  pat[node]=st[2];')]
def run():
 try:
  r=subprocess.run(['bash',str(E/'build_fixture.sh')],text=True,capture_output=True,timeout=180,errors='replace')
 except subprocess.TimeoutExpired: return 1,'timeout'
 line=[x for x in r.stdout.splitlines() if x.startswith('FG488_AC')]
 if line: return r.returncode,line[-1]
 return None,'build failed'
c=0
try:
 for l,o,n in M:
  if o not in O: print('[skip]',l); continue
  S.write_text(O.replace(o,n,1)); rc,line=run()
  if rc is None: print('[build-fail]',l); continue
  ok=rc!=0; c+=ok; print(f'[{"caught" if ok else "MISSED"}] {l}: {line}')
finally:
 S.write_text(O); run()
print(f'mutations {c}/{len(M)}')
