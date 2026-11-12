#!/usr/bin/env python3
"""Mutation gate for the GGUF offset indexing (EXP457)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-457-p9-02-gguf-offsets'
SRC = E / 'source' / 'offsets.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('round-up adds a full alignment too many', 'fn align_up(x:i64,n:i64)->i64{ return (x+n-1) & ((0 as i64)-n); }',
     'fn align_up(x:i64,n:i64)->i64{ return (x+n) & ((0 as i64)-n); }'),
    ('alignment check drops the power-of-two test', ' if (a & (a-1))!=0 as i64 { return 0 as i64; }',
     ' if a>1000000 as i64 { return 0 as i64; }'),
    ('zero alignment accepted', ' if a<=0 as i64 { return 0 as i64; }', ' if a<-1000000 as i64 { return 0 as i64; }'),
    ('byte size rounds the block count up', ' nb=tsize*(nelements/blk);', ' nb=tsize*(nelements/blk+1 as i64);'),
    ('divisibility check dropped', ' if (nelements % blk)!=0 as i64 { put(out,0 as i64,-(3 as i64)); return -(3 as i64); }', ''),
    ('absolute offset forgets the data start', '  out[i]=data_start+rel[i];', '  out[i]=rel[i];'),
    ('gaps between descriptors accepted', '  if rel[i]!=size { return -(2 as i64); }', '  if rel[i]<0 as i64 { return -(2 as i64); }'),
    ('total accumulates unpadded sizes', '  size=size+align_up(nb[i],align);', '  size=size+nb[i];'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=1800)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG457_OFFSETS')]
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
