#!/usr/bin/env python3
"""Mutation gate for the P3-10 dispatch predicates."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-427-p3-10-dispatch'
SRC = E / 'source' / 'dispatch-qualified.c'
ORIG = SRC.read_text()
LINES = ORIG.splitlines(keepends=True)

# (line number, old, new, label)
MUTATIONS = [
    (975, '512', '256', 'mmf ids threshold 512 -> 256'),
    (1067, '16', '15', 'mmf non-ids max batch 16 -> 15'),
    (1684, '8', '7', 'mmvf max batch 8 -> 7'),
    (1645, '3', '2', 'F32 ampere ne11 <= 3 -> <= 2'),
    (1882, '5', '4', 'F16 RDNA4 ne11 <= 5 -> <= 4'),
]


def rebuild_and_run():
    r = subprocess.run(['bash', str(E / 'build_testonly.sh')], text=True,
                       capture_output=True, timeout=900)
    if r.returncode != 0:
        return None, 'build failed'
    q = subprocess.run([str(F / 'runtime/exp427/build/dispatch-fixture')], text=True,
                       capture_output=True, timeout=900)
    last = q.stdout.strip().splitlines()[-1] if q.stdout.strip() else ''
    return q.returncode, last


if __name__ == '__main__':
    caught = 0
    try:
        for ln, old, new, label in MUTATIONS:
            lines = list(LINES)
            i = ln - 1
            if old not in lines[i]:
                print(f'[skip] {label}: anchor not on line {ln}')
                continue
            lines[i] = lines[i].replace(old, new, 1)
            SRC.write_text(''.join(lines))
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
