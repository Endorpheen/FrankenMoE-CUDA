#!/usr/bin/env python3
"""Mutation gate for the reduce_rows port.

Each mutation changes one association, offset or decision the module owns. The
fixture must fail; a mutation that passes means the case matrix does not reach
that decision.
"""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-414-goose-cuda-reduce-rows'
SRC = E / 'source' / 'device-functions-generated.cuh'
ORIG = SRC.read_text()

MUTATIONS = [
    ('first accumulator adds becomes subtracts',
     'v9 = ((float)(((t88) + (t89))));', 'v9 = ((float)(((t88) - (t89))));'),
    ('first warp shuffle offset 16 becomes 8',
     't3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));',
     't3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));'),
    ('cross-warp shared read ignores lane stride',
     't23 = ((int64_t)(v6));\n  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));',
     't23 = ((int64_t)(v6));\n  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));'),
    ('mean divides becomes multiplies',
     't134 = ((float)(((t132) / (t133))));', 't134 = ((float)(((t132) * (t133))));'),
    ('strided row selector uses ne1+1',
     'v15 = ((int64_t)(((int64_t)fg414dd_gg_mod(t16, t17))));',
     'v15 = ((int64_t)(((int64_t)fg414dd_gg_mod(t16, (t17) + (int64_t)1))));'),
]


def rebuild_and_run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=1800)
    if r.returncode != 0:
        return None, 'build failed'
    q = subprocess.run([str(F / 'runtime/exp414/build/gpu-fixture')], text=True,
                       capture_output=True, timeout=900)
    last = q.stdout.strip().splitlines()[-1] if q.stdout.strip() else ''
    return q.returncode, last


if __name__ == '__main__':
    caught = 0
    try:
        for label, old, new in MUTATIONS:
            if old not in ORIG:
                print(f'[skip] {label}: anchor not present')
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
