#!/usr/bin/env python3
"""Mutation gate for the request error policy (EXP450)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-450-p8-02-request-errors'
SRC = E / 'source' / 'errors.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('JSON error becomes a server error', ' if kind==2 as i64 { t=0 as i64; }', ' if kind==2 as i64 { t=2 as i64; }'),
    ('not_found code 404 -> 400', ' if t==3 as i64 { code=404 as i64; }', ' if t==3 as i64 { code=400 as i64; }'),
    ('unavailable code 503 -> 500', ' if t==5 as i64 { code=503 as i64; }', ' if t==5 as i64 { code=500 as i64; }'),
    ('exceed-context code 400 -> 500', ' if t==7 as i64 { code=400 as i64; }', ' if t==7 as i64 { code=500 as i64; }'),
    ('status fallback 500 -> 200', ' var s:i64=500 as i64;', ' var s:i64=200 as i64;'),
    ('status ignores the payload code', ' if has_code!=0 as i64 { s=payload_code; }', ' if has_code==2 as i64 { s=payload_code; }'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG450_REQUESTS')]
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
