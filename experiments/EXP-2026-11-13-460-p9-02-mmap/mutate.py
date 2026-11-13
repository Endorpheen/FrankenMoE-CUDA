#!/usr/bin/env python3
"""Mutation gate for the mmap/direct-I/O/mlock decisions (EXP460)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-460-p9-02-mmap'
SRC = E / 'source' / 'mmap.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('AUTO stops using mmap', ' if mode==LOAD_MODE_AUTO as i64 || mode==LOAD_MODE_MMAP as i64 || mode==LOAD_MODE_MMAP_MLOCK as i64 {',
     ' if mode==LOAD_MODE_MMAP as i64 || mode==LOAD_MODE_MMAP_MLOCK as i64 {'),
    ('mmap loads also set direct I/O', ' if mode==LOAD_MODE_DIRECT_IO as i64 { dio=1 as i64; }',
     ' if mode>=LOAD_MODE_MMAP as i64 { dio=1 as i64; }'),
    ('MLOCK alone stops locking', ' if mode==LOAD_MODE_MLOCK as i64 || mode==LOAD_MODE_MMAP_MLOCK as i64 { mlock=1 as i64; }',
     ' if mode==LOAD_MODE_MMAP_MLOCK as i64 { mlock=1 as i64; }'),
    ('unsupported mmap is not dropped', ' if mmap!=0 as i64 && mmap_supported==0 as i64 { mmap=0 as i64; }', ''),
    ('alignment gate accepts an unaligned offset', ' if (data_offset % align)==0 as i64 { return 1 as i64; }',
     ' if (data_offset % align)>=0 as i64 { return 1 as i64; }'),
    ('staged read is not capped at 64 MiB', ' if buf>DIRECT_IO_BUFFER as i64 { buf=DIRECT_IO_BUFFER as i64; }', ''),
    ('staged byte count rounds down', ' let bytes:i64=round_up(from+size,a);', ' let bytes:i64=round_down(from+size,a);'),
    ('staging buffer ignores the buffered case', ' if alignment!=1 as i64 { n=DIRECT_IO_BUFFER+2 as i64*alignment; }',
     ' if alignment>=1 as i64 { n=DIRECT_IO_BUFFER+2 as i64*alignment; }'),
    ('mlock keeps retrying after a failure', ' if failed==0 as i64 {', ' if failed==1 as i64 {'),
    ('empty unmap range is not clamped', ' if l<=f { l=f; }', ''),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG460_MMAP')]
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
