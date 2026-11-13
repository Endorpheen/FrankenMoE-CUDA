#!/usr/bin/env python3
from pathlib import Path
import subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-13-490-p7-09-schema-add';S=E/'source/peggram3.goose';O=S.read_text()
M=[('schema never calls add_schema','  if schema_add[id]!=0 {','  if schema_add[id]>=99 {'),
   ('the schema entry text is empty','mname[idx]=si; mval[idx]=at; mlen[idx]=8;','mname[idx]=si; mval[idx]=at; mlen[idx]=0;'),
   ('the schema returns the child name','   let e2=append_s(out,e,strs,off,len);\n   st[0]=e2;\n   return e,e2-e;','   let e2=append_s(out,e,strs,off,len);\n   st[0]=e2;\n   return e,0;'),
   ('delegating schemas call add_schema','  if schema_add[id]!=0 {','  if schema_add[id]>=0 {'),
   ('the schema entry uses the child index','mname[idx]=si; mval[idx]=at; mlen[idx]=8;','mname[idx]=sidx[children[cfirst[id]]]; mval[idx]=at; mlen[idx]=8;')]
def run():
 try: r=subprocess.run(['bash',str(E/'build_fixture.sh')],text=True,capture_output=True,timeout=180,errors='replace')
 except subprocess.TimeoutExpired: return 1,'timeout'
 line=[x for x in r.stdout.splitlines() if x.startswith('FG490_PEGSCHEMA')]
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
