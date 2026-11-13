#!/usr/bin/env python3
"""Mutation gate for the bounded reader decisions (EXP458)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-458-p9-02-file-io'
SRC = E / 'source' / 'io.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('scalar fit becomes exclusive', ' if size<=remain { return 1 as i64; }', ' if size<remain { return 1 as i64; }'),
    ('array element cap dropped', ' if n>MAX_ARRAY { st=1 as i64; }', ' if n>I64_MAX { st=1 as i64; }'),
    ('array overflow guard dropped', '    if n>(I64_MAX/elem) { st=2 as i64; }', '    if n>I64_MAX { st=2 as i64; }'),
    ('array byte check becomes exclusive', '     if n*elem>remain { st=3 as i64; }', '     if n*elem>=remain { st=3 as i64; }'),
    ('string length cap dropped', '  if size>MAX_STRING { st=1 as i64; }', '  if size>I64_MAX { st=1 as i64; }'),
    ('seek rejects the exact end', '   if abs_off<=end { ok=1 as i64; new_remain=end-abs_off; }',
     '   if abs_off<end { ok=1 as i64; new_remain=end-abs_off; }'),
    ('EOF leaves the remaining count untouched', ' if eof!=0 as i64 { new_remain=0 as i64; }', ''),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG458_FILE_IO')]
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
