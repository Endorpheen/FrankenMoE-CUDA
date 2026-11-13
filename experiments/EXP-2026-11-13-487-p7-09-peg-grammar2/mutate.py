#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-487-p7-09-peg-grammar2'
SRC = E / 'source/peggram2.goose'
ORIG = SRC.read_text()

M = [
    ('chars star becomes plus',
     "  else if lo==0 && hi==-1 { p=append_c(out,p,'*'); }\n  else if lo==1 && hi==-1 { p=append_c(out,p,'+'); }",
     "  else if lo==0 && hi==-1 { p=append_c(out,p,'+'); }\n  else if lo==1 && hi==-1 { p=append_c(out,p,'+'); }"),
    ('chars drops its pattern',
     '  var p=append_s(out,at,strs,off,len);\n  let lo=mn[id];',
     '  var p=append_s(out,at,"",0,0);\n  let lo=mn[id];'),
    ('chars bounded form loses the comma',
     "   p=append_c(out,p,'{');\n   p=append_dec(out,p,lo);\n   p=append_c(out,p,',');\n   p=append_dec(out,p,hi);",
     "   p=append_c(out,p,'{');\n   p=append_dec(out,p,lo);\n   p=append_dec(out,p,hi);"),
    ('string class loses the escaped backslash',
     "  p=append_c(out,p,'\\\\');\n  p=append_c(out,p,'\\\\');\n  p=append_c(out,p,']');",
     "  p=append_c(out,p,']');"),
    ('string second class loses the slash',
     "  p=append_c(out,p,'/');\n  p=append_s(out,p,\" bfnrt] | \",0,10);",
     "  p=append_s(out,p,\" bfnrt] | \",0,10);"),
    ('string unicode escape demands three hex digits',
     '  p=append_s(out,p," [0-9a-fA-F]{4} ) )*",0,20);',
     '  p=append_s(out,p," [0-9a-fA-F]{3} ) )*",0,20);'),
    ('string ignores its delimiter',
     '  p=append_c(out,p,delim);\n  p=append_c(out,p,\'\\\\\');',
     '  p=append_c(out,p,\'"\');\n  p=append_c(out,p,\'\\\\\');'),
    ('schema does not delegate',
     ' if k==K_SCHEMA {\n  return to_gbnf(children[cfirst[id]],out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir);\n }',
     ' if k==K_SCHEMA {\n  let si=sidx[id];\n  let off=sdir[2*si+0];\n  let len=sdir[2*si+1];\n  let e=append_s(out,at,strs,off,len);\n  st[0]=e;\n  return at,e-at;\n }'),
    ('string loses the second group opener',
     '  p=append_s(out,p," ( [",0,4);',
     '  p=append_s(out,p," [",0,2);'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG487_PEGGRAM2')]
    if line:
        return r.returncode, line[-1]
    if 'wrote ' not in r.stdout:
        return None, 'emission failed'
    last = r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'no stderr'
    return max(r.returncode, 1), 'build failure: ' + last[:140]


caught = 0
try:
    for label, old, new in M:
        if old not in ORIG:
            print('[skip]', label)
            continue
        SRC.write_text(ORIG.replace(old, new, 1))
        rc, line = run()
        if rc is None:
            print('[build-fail]', label)
            continue
        ok = rc != 0
        caught += ok
        print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
finally:
    SRC.write_text(ORIG)
    run()
print(f'mutations {caught}/{len(M)} caught')
