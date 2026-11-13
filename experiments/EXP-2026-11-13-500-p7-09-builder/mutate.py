#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-500-p7-09-builder'
SRC = E / 'source/builder.goose'
ORIG = SRC.read_text()

M = [
    ('invalid runs are not collapsed', '   if !run {', '   if true {'),
    ('a dot is a valid rule byte', '  if b==\'-\' { ok=true; }', '  if b==\'-\' || b==\'.\' { ok=true; }'),
    ('invalid runs become underscores', "    if n<out.len { out[n]='-'; }", "    if n<out.len { out[n]='_'; }"),
    ('the nested parser is not spliced', '  if kind[id]==want {', '  if false {'),
    ('the splice is reversed', '  let id=ids[i];', '  let id=ids[n-1-i];'),
    ('only the first child is spliced', '   while j<ccount[id] {', '   while j<1 {'),
    ('the plain rule creates two parsers', '  out[0]=1; out[1]=1; out[2]=1;', '  out[0]=2; out[1]=1; out[2]=1;'),
    ('the builder overload adds no placeholder', ' out[0]=2; out[1]=1; out[2]=1;', ' out[0]=1; out[1]=1; out[2]=1;'),
    ('an existing rule creates a parser', '  out[0]=0; out[1]=0; out[2]=1;', '  out[0]=1; out[1]=1; out[2]=1;'),
    ('no ref is returned', '  out[0]=1; out[1]=1; out[2]=1;', '  out[0]=1; out[1]=1; out[2]=0;'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG500_BUILDER')]
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
