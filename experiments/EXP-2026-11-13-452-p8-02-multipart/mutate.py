#!/usr/bin/env python3
"""Mutation gate for the multipart / path-parameter helpers (EXP452)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-452-p8-02-multipart'
SRC = E / 'source' / 'multipart.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('hex digits accept one extra character', 'if c<=57 as u32', 'if c<=58 as u32'),
    ('decoded escape consumes two bytes', '     i=i+3 as i64;', '     i=i+2 as i64;'),
    ('invalid escape is decoded anyway',
     '    if hi>=0 as i64 { if lo>=0 as i64 { ok=1 as i64; } }',
     '    ok=1 as i64;'),
    ('repeated form keys never group', '  if same { return 1 as i64; }',
     '  if same { return 0 as i64; }'),
    ('key comparison ignores bytes', ' if an==0 as i64 {return true;}\n return keys[ao..ao+an]==keys[bo..bo+bn];',
     ' return true;'),
    ('path parameter loses to the query value', ' if has_path!=0 as i64 { return 2 as i64; }',
     ' if has_path!=0 as i64 { return 1 as i64; }'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG452_MULTIPART')]
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
