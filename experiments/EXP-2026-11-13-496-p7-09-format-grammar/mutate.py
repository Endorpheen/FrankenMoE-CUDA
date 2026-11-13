#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-496-p7-09-format-grammar'
SRC = E / 'source/schemafull.goose'
ORIG = SRC.read_text()

M = [
    ('the sort never shifts', '   if less { idx[j]=prev; j=j-1; } else { stop=true; }', '   if false { idx[j]=prev; j=j-1; } else { stop=true; }'),
    ('the sort is descending', '   if less { idx[j]=prev; j=j-1; } else { stop=true; }', '   if !less { idx[j]=prev; j=j-1; } else { stop=true; }'),
    ('the prefix tie-break is reversed', '   if !decided && lb<la { less=true; }', '   if !decided && la<lb { less=true; }'),
    ('the separators lose their spaces', '  p=app(out,p," ::= ",0,5);', '  p=app(out,p,"::=",0,3);'),
    ('the rule line loses its newline', '  p=appc(out,p,(10 as u8));\n  z=z+1;', '  z=z+1;'),
    ('the key and text swap places', '  p=app(out,p,buf,koff[e],klen[e]);\n  p=app(out,p," ::= ",0,5);\n  p=app(out,p,buf,toff[e],tlen[e]);',
     '  p=app(out,p,buf,toff[e],tlen[e]);\n  p=app(out,p," ::= ",0,5);\n  p=app(out,p,buf,koff[e],klen[e]);'),
    ('the failure prefix loses its newline', ' var p=app(out,0,"JSON schema conversion failed:\\n",0,31);', ' var p=app(out,0,"JSON schema conversion failed:",0,30);'),
    ('the failure drops the message', ' p=app(out,p,err,0,errlen);\n return p;', ' return p;'),
    ('warnings join with a comma', '  if i>0 { p=app(out,p,"; ",0,2); }', '  if i>0 { p=app(out,p,", ",0,2); }'),
    ('the warning prefix loses its space', ' var p=app(out,0,"WARNING: JSON schema conversion was incomplete: ",0,48);', ' var p=app(out,0,"WARNING: JSON schema conversion was incomplete:",0,47);'),
    ('the warning loses its newline', ' p=appc(out,p,(10 as u8));\n return p;\n}', ' return p;\n}'),
    ('only the first warning is printed', '  p=app(out,p,buf,off[i],len[i]);\n  i=i+1;', '  p=app(out,p,buf,off[i],len[i]);\n  i=n;'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=240, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG496_FMT')]
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
