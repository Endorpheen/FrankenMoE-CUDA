#!/usr/bin/env python3
"""Mutation gate for the server pre-routing policy (EXP446)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-446-p8-02-prerouting'
SRC = E / 'source' / 'prerouting.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('CORS star+credentials stops echoing', 'if cors_cred!=0 as i64 { allow_mode=1 as i64; allow_len=origin_len; }',
     'if cors_cred!=0 as i64 { allow_mode=2 as i64; allow_len=cors_lit_len; }'),
    ('localhost mode accepts any origin', 'let ok:i64=origin_is_localhost(origin,origin_len);',
     'let ok:i64=1 as i64;'),
    ('scheme detection keeps the scheme bytes', 'start=p+3 as i64; stop=1 as i64;', 'start=p; stop=1 as i64;'),
    ('OPTIONS short-circuit removed', 'if is_options!=0 as i64 {', 'if is_options==2 as i64 {'),
    ('readiness gate loses the frontend exception', 'if is_ready==0 as i64 && is_frontend==0 as i64 {',
     'if is_ready==0 as i64 {'),
    ('API-key gate ignores public endpoints', 'if n_keys>0 as i64 && is_public==0 as i64 {', 'if n_keys>0 as i64 {'),
    ('Bearer prefix shortened to six bytes', 'src=src+7 as u64; len=len-7 as i64;', 'src=src+6 as u64; len=len-6 as i64;'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG446_PREROUTE')]
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
