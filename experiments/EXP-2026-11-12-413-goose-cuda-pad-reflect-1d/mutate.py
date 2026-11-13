#!/usr/bin/env python3
"""Mutation gate for the pad_reflect_1d port.

Each mutation changes one decision the module is responsible for, rebuilds the
fixture from the mutated generated code, and requires the differential to fail.
A mutation that still passes means the fixture does not exercise that decision.
"""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-413-goose-cuda-pad-reflect-1d'
SRC = E / 'source' / 'device-functions-generated.cuh'
ORIG = SRC.read_text()

MUTATIONS = [
    ('left reflection sign', '(uint64_t)(t4) - (uint64_t)(t5)', '(uint64_t)(t4) + (uint64_t)(t5)'),
    ('middle bound becomes <=', 't8 = ((t6) < (t7));', 't8 = ((t6) <= (t7));'),
    ('right reflection uses +2 instead of -2', '(uint64_t)(t11) - (uint64_t)(t12)',
     '(uint64_t)(t11) + (uint64_t)(t12)'),
    ('row selector uses ne01+1', 'fg413dd_gg_mod(t17, t18)',
     'fg413dd_gg_mod(t17, (t18) + (int64_t)1)'),
    ('destination ignores the padded row stride', '(uint64_t)(t65) * (uint64_t)(t66)',
     '(uint64_t)(t65) * (uint64_t)((int64_t)(v2) * (int64_t)4)'),
]


def rebuild_and_run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=1800)
    if r.returncode != 0:
        return None, 'build failed'
    q = subprocess.run([str(F / 'runtime/exp413/build/gpu-fixture')], text=True,
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
