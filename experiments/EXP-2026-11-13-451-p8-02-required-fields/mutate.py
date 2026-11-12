#!/usr/bin/env python3
"""Mutation gate for the required-field contract (EXP451)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-451-p8-02-required-fields'
SRC = E / 'source' / 'required.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('/completion loses its prompt requirement', ' if cat==11 as i64 {return 1 as i64;}',
     ' if cat==11 as i64 {return 0 as i64;}'),
    ('infill requires only input_prefix', ' if cat==20 as i64 {return 24 as i64;}',
     ' if cat==20 as i64 {return 8 as i64;}'),
    ('rerank drops the documents requirement', ' if cat==23 as i64 {return 192 as i64;}',
     ' if cat==23 as i64 {return 64 as i64;}'),
    ('embeddings require both input and content',
     '  if (mask & 36 as i64)==0 as i64 { miss=36 as i64; } else { miss=0 as i64; }',
     '  miss=need ^ (mask & need);'),
    ('chat requires prompt instead of messages', ' if cat==13 as i64 {return 2 as i64;}',
     ' if cat==13 as i64 {return 1 as i64;}'),
    ('transcription file bit becomes array-body bit', ' if cat==16 as i64 {return 2048 as i64;}',
     ' if cat==16 as i64 {return 1024 as i64;}'),
    ('control id requirement removed', ' if cat==14 as i64 {return 512 as i64;}',
     ' if cat==14 as i64 {return 0 as i64;}'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG451_REQUIRED')]
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
