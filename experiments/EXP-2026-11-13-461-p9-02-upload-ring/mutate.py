#!/usr/bin/env python3
"""Mutation gate for the async-upload staging ring (EXP461)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-461-p9-02-upload-ring'
SRC = E / 'source' / 'upload.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('alignment floor becomes two', ' var m:i64=1 as i64;', ' var m:i64=2 as i64;'),
    ('alignment aggregation keeps the minimum', '  if xs[i]>m { m=xs[i]; }', '  if xs[i]<m { m=xs[i]; }'),
    ('ring does not wrap', ' return (idx+1 as i64)%n;', ' return idx+1 as i64;'),
    ('empty ring returns a buffer index', ' if n<=0 as i64 { return 0 as i64; }', ' if n<=0 as i64 { return 1 as i64; }'),
    ('chunk ignores the remaining bytes', ' if remaining<n { n=remaining; }', ''),
    ('first chunk does not skip the padding', ' if bytes_read==0 as i64 { ptr=from; }', ' if bytes_read==0 as i64 { ptr=0 as i64; }'),
    ('trailing padding is copied as data', ' if bytes_read+n>total-tail { copy=copy-tail; }', ''),
    ('mmap no longer blocks async uploads', ' if use_mmap!=0 as i64 || check_tensors!=0 as i64 { r=1 as i64; }',
     ' if use_mmap==2 as i64 || check_tensors!=0 as i64 { r=1 as i64; }'),
    ('missing host buffer capability is ignored',
     '  if has_async==0 as i64 || has_host==0 as i64 || has_events==0 as i64 { r=5 as i64; }',
     '  if has_async==0 as i64 || has_events==0 as i64 { r=5 as i64; }'),
    ('order ignores the staged flag', ' if a_staged!=b_staged { return a_staged; }',
     ' if a_staged==2 as i64 { return a_staged; }'),
    ('order sorts bytes ascending', ' if a_staged!=0 as i64 && a_bytes>b_bytes { return 1 as i64; }',
     ' if a_staged!=0 as i64 && a_bytes<b_bytes { return 1 as i64; }'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG461_UPLOAD')]
    if line:
        return r.returncode, line[-1]
    if 'wrote ' not in r.stdout:
        return None, 'emission failed'
    last = r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'no stderr'
    return max(r.returncode, 1), 'runtime trap: ' + last


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
