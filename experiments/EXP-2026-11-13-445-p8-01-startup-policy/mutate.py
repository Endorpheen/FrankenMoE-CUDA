#!/usr/bin/env python3
"""Mutation gate for the server startup/config policy (EXP445)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-445-p8-01-startup-policy'
SRC = E / 'source' / 'startup.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('router guard inverted', 'if has_model != 0 as i64 {', 'if has_model == 0 as i64 {'),
    ('embedding clamp direction flipped', 'if embedding != 0 as i64 && nb > nub { nb = nub; }',
     'if embedding != 0 as i64 && nb < nub { nb = nub; }'),
    ('auto parallel 4 -> 8', 'if np < 0 as i64 { np = 4 as i64; ku = 1 as i64; }',
     'if np < 0 as i64 { np = 8 as i64; ku = 1 as i64; }'),
    ('kv sizing ignores fit min ctx', 'fit_min_ctx != 4294967295 as i64', 'fit_min_ctx == 4294967295 as i64'),
    ('alias fallback accepts empty name', 'alias_empty != 0 as i64 && model_name_len > 0 as i64',
     'alias_empty != 0 as i64 && model_name_len >= 0 as i64'),
    ('slot cap takes the larger value', 'if ctx_train < slot { slot = ctx_train; }',
     'if ctx_train > slot { slot = ctx_train; }'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    if r.returncode != 0 and not r.stdout.strip().startswith('PASS'):
        if 'FG445_STARTUP' not in r.stdout:
            return None, 'build failed'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG445_STARTUP')]
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
