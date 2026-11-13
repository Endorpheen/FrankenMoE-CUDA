#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-497-p7-09-ast-helpers'
SRC = E / 'source/asthelp.goose'
ORIG = SRC.read_text()

M = [
    ('one extra level is searched', '  if depth>1 {', '  if depth>0 {'),
    ('the search stops at the first level', '  if depth>1 {', '  if depth>100 {'),
    ('a deeper match wins over a shallower one', '   if eqb(str_of(strs,sdir,sid),want) { return ch; }', '   if false { return ch; }'),
    ('a node also matches itself', ' var i:i64=0;\n let n=a_cc[parent];', ' if kind==0 { if a_tag[parent]>=0 { if eqb(str_of(strs,sdir,a_tag[parent]),want) { return parent; } } }\n var i:i64=0;\n let n=a_cc[parent];'),
    ('rule lookups read the tag field', '  if kind==0 { sid=a_tag[ch]; } else { sid=a_rule[ch]; }', '  if kind==0 || true { sid=a_tag[ch]; } else { sid=a_rule[ch]; }'),
    ('visit is post-order', ' out[st[0]]=id;\n st[0]=st[0]+1;\n var i:i64=0;', ' var i:i64=0;'),
    ('visit skips the children', '  visit_rec(achild[base+i],a_cs,a_cc,achild,out,st);\n  i=i+1;', '  i=i+1;'),
    ('sanitized text keeps the invalid bytes', '  p=appc(out,p,239);\n  p=appc(out,p,191);\n  p=appc(out,p,189);', '  p=app(out,p,input,off+pos,run_len);'),
    ('the replacement is two bytes', '  p=appc(out,p,239);\n  p=appc(out,p,191);\n  p=appc(out,p,189);', '  p=appc(out,p,239);\n  p=appc(out,p,191);'),
    ('the run cursor advances by one', '  cur=pos+run_len;', '  cur=pos+1;'),
    ('the text after the last run is dropped', ' if cur<len { p=app(out,p,input,off+cur,len-cur); }', ''),
    ('success is spelled differently', ' if t==R_SUCCESS { n="success"; }', ' if t==R_SUCCESS { n="succeeded"; }'),
    ('unknown becomes a question mark', ' var n: const u8[:] = "unknown";', ' var n: const u8[:] = "?";'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG497_ASTHELP')]
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
