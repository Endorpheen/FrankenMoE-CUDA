#!/usr/bin/env python3
"""Mutation gate for the MXFP4 MMQ quantizer."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-431-p3-07-mxfp4'
SRC = E / 'source' / 'mxfp4.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('warp max offset 16 -> 8',
     'shfl_f32(a,4294967295 as u32,16 as i32)', 'shfl_f32(a,4294967295 as u32,8 as i32)'),
    ('E8M0 bias -2 -> -1',
     'ei-(2 as i32)', 'ei-(1 as i32)'),
    ('FP4 LUT 3.0 -> 2.5',
     'if fabsv(ax-(3.0 as f32))<err{best=5 as i32;err=fabsv(ax-(3.0 as f32));}',
     'if fabsv(ax-(2.5 as f32))<err{best=5 as i32;err=fabsv(ax-(2.5 as f32));}'),
    ('nibble pack shift 4 -> 5',
     'let pk:i32=((q10<<(4 as i32))|q00)|(((q11<<(4 as i32))|q01)<<(8 as i32));',
     'let pk:i32=((q10<<(5 as i32))|q00)|(((q11<<(5 as i32))|q01)<<(8 as i32));'),
    ('D4 scale high byte shifted',
     '(s1<<(8 as i32))|s0', '(s1<<(9 as i32))|s0'),
]


def rebuild_and_run():
    r = subprocess.run(['bash', str(E / 'rebuild_test.sh')], text=True,
                       capture_output=True, timeout=1800)
    if r.returncode != 0:
        return None, 'build failed'
    q = subprocess.run([str(F / 'runtime/exp431/build/gpu-fixture')], text=True,
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
