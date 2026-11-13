#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-499-p7-09-resolve-refs'
SRC = E / 'source/resolve.goose'
ORIG = SRC.read_text()

M = [
    ('sequences are skipped', '  if k==K_SEQ || k==K_CHOICE {', '  if k==K_CHOICE {'),
    ('choices are skipped', '  if k==K_SEQ || k==K_CHOICE {', '  if k==K_SEQ {'),
    ('single-child kinds are skipped', '  } else if k==K_REP || k==K_AND || k==K_NOT || k==K_RULE || k==K_ATOMIC || k==K_TAG || k==K_GBNF || k==K_AC || k==K_SCHEMA {',
     '  } else if k==K_TAG {'),
    ('schemas are skipped', '  } else if k==K_REP || k==K_AND || k==K_NOT || k==K_RULE || k==K_ATOMIC || k==K_TAG || k==K_GBNF || k==K_AC || k==K_SCHEMA {',
     '  } else if k==K_REP || k==K_AND || k==K_NOT || k==K_RULE || k==K_ATOMIC || k==K_TAG || k==K_GBNF || k==K_AC {'),
    ('rules are skipped', '  } else if k==K_REP || k==K_AND || k==K_NOT || k==K_RULE || k==K_ATOMIC || k==K_TAG || k==K_GBNF || k==K_AC || k==K_SCHEMA {',
     '  } else if k==K_REP || k==K_AND || k==K_NOT || k==K_ATOMIC || k==K_TAG || k==K_GBNF || k==K_AC || k==K_SCHEMA {'),
    ('the root is not flattened', ' if root>=0 && kind[root]==K_REF {', ' if false {'),
    ('the ref child is left alone', ' children[slot]=r;', ' children[slot]=c;'),
    ('an unknown name is ignored', ' if r<0 {\n  out[1]=sidx[c];\n  return 1;\n }', ' if r<0 {\n  return 0;\n }'),
    ('the missing name is not reported', '  out[1]=sidx[c];\n  return 1;', '  out[1]=-1;\n  return 1;'),
    ('only the first parser is visited', ' var i:i64=0;\n let n=kind.len;\n while i<n {', ' var i:i64=0;\n let n=kind.len;\n while i<1 {'),
    ('every child is rewritten, ref or not', ' if kind[c]!=K_REF { return 0; }', ' if false { return 0; }'),
    ('the first rule always wins', '  if str_eq(strs,sdir,want,rname[i]) { return rid[i]; }', '  if i>=0 { return rid[0]; }'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG499_RESOLVE')]
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
