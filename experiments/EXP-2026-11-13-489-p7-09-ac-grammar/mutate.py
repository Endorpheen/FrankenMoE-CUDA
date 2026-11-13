#!/usr/bin/env python3
from pathlib import Path
import subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-13-489-p7-09-ac-grammar';S=E/'source/acg.goose';O=S.read_text()
M=[('excluding loses its leading bar','    tp=appc(out,tp,\'|\');','    tp=appc(out,tp,\' \');'),
   ('excluding bucket class is negated','       tp=emit_class(out,tp,bucket,nb,0);','       tp=emit_class(out,tp,bucket,nb,1);'),
   ('including drops completing chars','    if nc>0 { tp=emit_class(out,tp,comp_c,nc,0); first=false; }','    if nc>99 { tp=emit_class(out,tp,comp_c,nc,0); first=false; }'),
   ('including specific class is not negated','    tp=emit_class(out,tp,spec_c,ns,1);\n    tp=appc(out,tp,\' \');\n    let sno,snl=emit_name(out,tp,kind,pid,0); tp=sno+snl;\n   }\n   let ri=st[4];','    tp=emit_class(out,tp,spec_c,ns,0);\n    tp=appc(out,tp,\' \');\n    let sno,snl=emit_name(out,tp,kind,pid,0); tp=sno+snl;\n   }\n   let ri=st[4];'),
   ('state names lose the leading zero','  if q<10 { p=appc(out,p,\'0\'); }','  if q<0 { p=appc(out,p,\'0\'); }'),
   ('dash is not escaped',' if c==45 || c==93 || c==91 || c==92 { p=appc(out,p,92); return appc(out,p,c as u8); }',' if c==93 || c==91 || c==92 { p=appc(out,p,92); return appc(out,p,c as u8); }'),
   ('terminal states get rules too','  if mt[q]<0 {\n   let no,nl=emit_name','  if mt[q]<-99 {\n   let no,nl=emit_name'),
   ('the empty-delimiter rule is missing',' if mt[0]>=0 {\n  // empty delimiter',' if mt[0]>=999 {\n  // empty delimiter')]
def run():
 try: r=subprocess.run(['bash',str(E/'build_fixture.sh')],text=True,capture_output=True,timeout=120,errors='replace')
 except subprocess.TimeoutExpired: return 1,'timeout'
 line=[x for x in r.stdout.splitlines() if x.startswith('FG489_ACG')]
 if line: return r.returncode,line[-1]
 return None,'build failed'
c=0
try:
 for l,o,n in M:
  if o not in O: print('[skip]',l); continue
  S.write_text(O.replace(o,n,1)); rc,line=run()
  if rc is None: print('[build-fail]',l); continue
  ok=rc!=0; c+=ok; print(f'[{"caught" if ok else "MISSED"}] {l}: {line}')
finally: S.write_text(O); run()
print(f'mutations {c}/{len(M)}')
