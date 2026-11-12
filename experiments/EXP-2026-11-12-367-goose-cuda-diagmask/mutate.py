#!/usr/bin/env python3
"""Mutation gate for the diag_mask_inf port.

Each mutation changes one thing the kernel is responsible for, rebuilds the
fixture from the mutated generated code, and requires the differential to fail.
A mutation that still passes means the gate does not exercise that decision.
"""
from pathlib import Path
import re, shutil, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-367-goose-cuda-diagmask'
SRC = E / 'source' / 'device-functions-generated.cuh'
ORIG = SRC.read_text()

MUTATIONS = [
    ('FLT_MAX replaced by zero (mask disappears)', '5183643170566569984', '0'),
    ('mask comparison > becomes >=', 't19 = ((t17) > (t18));', 't19 = ((t17) >= (t18));'),
    ('mask subtraction becomes addition', '((t20) - (t22))', '((t20) + (t22))'),
    ('row modulo replaced by no modulo',
     't11 = ((int64_t)(((int64_t)fg367dd_gg_mod(t9, t10))));',
     't11 = ((int64_t)(((int64_t)(t9))));'),
    ('destination index uses row without ncols', 't24) * (uint64_t)(t25)', 't24) + (uint64_t)(t25)'),
]


def rebuild_and_run():
    r = subprocess.run(['bash', '/tmp/d367-build.sh'], cwd='/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda',
                       text=True, capture_output=True, timeout=1800)
    if r.returncode != 0:
        return None, 'build failed'
    q = subprocess.run([str(F / 'runtime/exp367/build/gpu-fixture')], text=True, capture_output=True, timeout=900)
    return q.returncode, q.stdout.strip().splitlines()[-1] if q.stdout.strip() else ''


if __name__ == '__main__':
    caught = 0
    results = []
    try:
        for label, old, new in MUTATIONS:
            if old not in ORIG:
                print(f'[skip] {label}: anchor not present')
                results.append((label, 'anchor-missing'))
                continue
            SRC.write_text(ORIG.replace(old, new, 1))
            rc, line = rebuild_and_run()
            if rc is None:
                print(f'[build-fail] {label}')
                results.append((label, 'build-fail'))
                continue
            ok = rc != 0
            caught += ok
            print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
            results.append((label, 'caught' if ok else 'missed'))
    finally:
        SRC.write_text(ORIG)
        rebuild_and_run()
    print(f'mutations {caught}/{len(MUTATIONS)} caught')
