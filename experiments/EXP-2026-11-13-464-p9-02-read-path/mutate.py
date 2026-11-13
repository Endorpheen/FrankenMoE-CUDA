#!/usr/bin/env python3
"""Mutation gate for the mmap/pread read decisions (EXP464)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-464-p9-02-read-path'
SRC = E / 'source' / 'readpath.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('buffered read is not truncated at EOF', '  let left:i64=size-off;\n  if left<to_read { to_read=left; }',
     '  to_read=len;'),
    ('short read is never an error', ' if to_read>0 as i64 { short_is_error=1 as i64; }', ''),
    ('EINTR becomes fatal', ' if e==1 as i64 { a=0 as i64; }', ' if e==1 as i64 { a=2 as i64; }'),
    ('EFAULT becomes fatal', ' if e==2 as i64 || e==3 as i64 { a=1 as i64; }', ' if e==3 as i64 { a=1 as i64; }'),
    ('zero-fill allowed past the file end', ' if pos==size as i64 { zero_ok=1 as i64; }',
     ' if pos>=size as i64 { zero_ok=1 as i64; }'),
    ('alignment always the block size', ' var a:i64=1 as i64;\n if fd_open!=0 as i64 && blksize>0 as i64 { a=blksize; }',
     ' var a:i64=blksize;'),
    ('source ignores the mmap flag', ' if use_mmap!=0 as i64 && has_mapping!=0 as i64 { k=1 as i64; }',
     ' if has_mapping!=0 as i64 { k=1 as i64; }'),
    ('mapping offset adds the inner offset twice', ' if kind!=0 as i64 { o=weight_offs+inner; }',
     ' if kind!=0 as i64 { o=weight_offs+inner+inner; }'),
    ('mapping offset ignores the weight offset', ' if kind!=0 as i64 { o=weight_offs+inner; }',
     ' if kind!=0 as i64 { o=inner; }'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG464_READPATH')]
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
