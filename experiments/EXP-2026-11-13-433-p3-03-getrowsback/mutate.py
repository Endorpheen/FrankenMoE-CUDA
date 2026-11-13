#!/usr/bin/env python3
"""Mutation gate for GET_ROWS_BACK."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-433-p3-03-getrowsback'
SRC = E / 'source' / 'device.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('row equality inverted', 'if r==dr{', 'if r!=dr{'),
    ('sum becomes difference', 'sum=sum+ld_f32', 'sum=sum-ld_f32'),
    ('destination offset loses ncols', '((dr*ncols+col)*(4 as i64))', '((dr+col)*(4 as i64))'),
    ('row-index stride 4 -> 8', 'i*(4 as i64)', 'i*(8 as i64)'),
    ('destination stride gy -> gy+1', 'dr=dr+gy;', 'dr=dr+gy+(1 as i64);'),
]


def rebuild_and_run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=1800)
    if r.returncode != 0:
        return None, 'build failed'
    q = subprocess.run([str(F / 'runtime/exp433/build/gpu-fixture')], text=True,
                       capture_output=True, timeout=900)
    last = q.stdout.strip().splitlines()[-1] if q.stdout.strip() else ''
    return q.returncode, last


if __name__ == '__main__':
    caught = 0
    try:
        for label, old, new in MUTATIONS:
            if old not in ORIG:
                print(f'[skip] {label}: anchor missing')
                continue
            SRC.write_text(ORIG.replace(old, new, 1))
            rc, line = rebuild_and_run()
            if rc is None:
                print(f'[build-fail] {label}')
                continue
            ok = rc != 0
            caught += ok
            print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
    finally:
        SRC.write_text(ORIG)
        rebuild_and_run()
    print(f'mutations {caught}/{len(MUTATIONS)} caught')
