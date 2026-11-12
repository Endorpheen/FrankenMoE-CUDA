#!/usr/bin/env python3
"""Mutation gate for D4/D2S6 MMQ Q8_1 layouts."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-430-p3-07-d4-d2s6'
SRC = E / 'source' / 'device.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('D2S6 sum shuffle offset 2 -> 4',
     's=s+shfl_xor(s,4294967295 as u32,2);\n  s=s+shfl_xor(s,4294967295 as u32,1);',
     's=s+shfl_xor(s,4294967295 as u32,4);\n  s=s+shfl_xor(s,4294967295 as u32,1);'),
    ('D2S6 sum slot stride 2 -> 3',
     'st_half(y,(yb+((2 as i64)+(iqs>>(4 as i64)))*2) as u64,sum);',
     'st_half(y,(yb+((2 as i64)+(iqs>>(4 as i64)))*3) as u64,sum);'),
    ('D2S6 scale index 6 -> 7',
     'st_half(y,(yb+(iqs>>(6 as i64))*2) as u64,d);',
     'st_half(y,(yb+(iqs>>(7 as i64))*2) as u64,d);'),
    ('D2S6 sum window 96 -> 64',
     'if (iqs&(15 as i64))==0&&iqs<96{',
     'if (iqs&(15 as i64))==0&&iqs<64{'),
    ('D4 scale slot 5 -> 4',
     'st_f32(y,(yb+(iqs>>(5 as i64))*(4 as i64)) as u64,d);',
     'st_f32(y,(yb+(iqs>>(4 as i64))*(4 as i64)) as u64,d);'),
]


def rebuild_and_run():
    r = subprocess.run(['bash', str(E / 'rebuild_test.sh')], text=True,
                       capture_output=True, timeout=1800)
    if r.returncode != 0:
        return None, 'build failed'
    q = subprocess.run([str(F / 'runtime/exp430/build/gpu-fixture')], text=True,
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
