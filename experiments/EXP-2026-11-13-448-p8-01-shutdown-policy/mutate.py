#!/usr/bin/env python3
"""Mutation gate for the server shutdown policy (EXP448)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-448-p8-01-shutdown-policy'
SRC = E / 'source' / 'lifecycle.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('second interrupt runs shutdown instead of forcing exit', ' action=2 as i64;', ' action=1 as i64;'),
    ('second interrupt exits with code 0', '  code=1 as i64;', '  code=0 as i64;'),
    ('router signal plan terminates ctx instead of stopping http', '   put(out,3 as i64,6 as i64);',
     '   put(out,3 as i64,7 as i64);'),
    ('single signal plan order swapped',
     '   put(out,1 as i64,4 as i64);\n   put(out,2 as i64,7 as i64);',
     '   put(out,1 as i64,7 as i64);\n   put(out,2 as i64,4 as i64);'),
    ('router cleanup loses backend_free', '   put(out,5 as i64,5 as i64);\n   n=5 as i64;\n  }else{\n   put(out,2 as i64,6 as i64);',
     '   put(out,5 as i64,4 as i64);\n   n=5 as i64;\n  }else{\n   put(out,2 as i64,6 as i64);'),
    ('single cleanup stops http before the session GC',
     '  put(out,1 as i64,1 as i64);\n  if mode==0 as i64 {',
     '  put(out,1 as i64,6 as i64);\n  if mode==0 as i64 {'),
    ('exit code inverted', ' if outcome!=0 as i64 { code=1 as i64; }', ' if outcome==0 as i64 { code=1 as i64; }'),
    ('no-handler signal still runs shutdown', '  if has_handler!=0 as i64 { action=1 as i64; }',
     '  if has_handler==0 as i64 { action=1 as i64; }'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG448_SHUTDOWN')]
    if not line and r.returncode != 0:
        return None, 'build failed'
    return r.returncode, (line[-1] if line else '')


if __name__ == '__main__':
    caught = 0
    try:
        for label, old, new in MUTATIONS:
            if old not in ORIG:
                print(f'[skip] {label}: anchor missing')
                continue
            SRC.write_text(ORIG.replace(old, new, 1))
            rc, line = run()
            if rc is None:
                print(f'[build-fail] {label}')
                continue
            ok = rc != 0
            caught += ok
            print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
    finally:
        SRC.write_text(ORIG)
        run()
    print(f'mutations {caught}/{len(MUTATIONS)} caught')
