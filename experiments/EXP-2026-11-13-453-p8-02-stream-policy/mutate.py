#!/usr/bin/env python3
"""Mutation gate for the stream session policy (EXP453)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-453-p8-02-stream-policy'
SRC = E / 'source' / 'stream.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('oversized chunk keeps its head instead of its tail', '    skip=len-cap;', '    skip=0 as i64;'),
    ('dropped prefix forgets the chunk skip', '    delta=buf_len+skip;', '    delta=buf_len;'),
    ('overflow keeps the old bytes', '     delta=needed-cap;', '     delta=0 as i64;'),
    ('empty chunk rejected after finalize', ' if len==0 as i64 {\n  accept=1 as i64;\n }else{',
     ' if len==0 as i64 {\n  accept=0 as i64;\n }else{'),
    ('finalized session still accepts data', '  if done==0 as i64 {\n   accept=1 as i64;', '  if done==1 as i64 {\n   accept=1 as i64;'),
    ('lost offset treated as a chunk', '  if offset<dropped {\n   status=2 as i64;', '  if offset<dropped {\n   status=1 as i64;'),
    ('chunk length measured from the dropped prefix', '    n=logical_end-offset;', '    n=logical_end-dropped;'),
    ('stop request loses priority', ' if stop!=0 as i64 {\n  status=4 as i64;\n }else{', ' if stop==2 as i64 {\n  status=4 as i64;\n }else{'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG453_STREAM')]
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
