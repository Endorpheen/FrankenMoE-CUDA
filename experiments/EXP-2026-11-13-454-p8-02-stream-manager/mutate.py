#!/usr/bin/env python3
"""Mutation gate for the stream session manager policy (EXP454)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-454-p8-02-stream-manager'
SRC = E / 'source' / 'manager.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('replace stops cancelling the previous session', '   cancel=1 as i64;', '   cancel=0 as i64;'),
    ('evict also cancels', '  if op==3 as i64 { cancel=1 as i64; }', '  if op>=2 as i64 { cancel=1 as i64; }'),
    ('evict_and_cancel stops cancelling', '  if op==3 as i64 { cancel=1 as i64; }',
     '  if op==33 as i64 { cancel=1 as i64; }'),
    ('stop all also cancels', ' put(out,0 as i64,0 as i64);  // cancel', ' put(out,0 as i64,1 as i64);  // cancel'),
    ('GC drops live sessions too', ' if completed!=0 as i64 {', ' if completed!=-1 as i64 {'),
    ('GC boundary becomes exclusive', '  if completed<=(now-ttl) { drop=1 as i64; }',
     '  if completed<(now-ttl) { drop=1 as i64; }'),
    ('GC cutoff ignores the TTL', ' put(out,0 as i64,now-ttl);', ' put(out,0 as i64,now);'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG454_MANAGER')]
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
