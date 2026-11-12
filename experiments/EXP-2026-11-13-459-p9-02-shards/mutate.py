#!/usr/bin/env python3
"""Mutation gate for the shard naming and validation (EXP459)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-459-p9-02-shards'
SRC = E / 'source' / 'shards.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('padding width becomes four', ' if d>5 as i64 { return d; }\n return 5 as i64;',
     ' if d>4 as i64 { return d; }\n return 4 as i64;'),
    ('pointer is one-based in both places', ' let n:i64=split_no+1 as i64;', ' let n:i64=split_no as i64;'),
    ('capacity check ignores the terminator', ' if total>=out.len { return 0 as i64; }',
     ' if total>out.len { return 0 as i64; }'),
    ('separator becomes a slash', ' out[o]=45 as u8; o=o+1 as i64;                       // \'-\'\n o=put_num(n,wa,out,o);',
     ' out[o]=47 as u8; o=o+1 as i64;                       // \'-\'\n o=put_num(n,wa,out,o);'),
    ('first-shard check accepts any index', ' if idx==0 as i64 { return 1 as i64; }\n return 0 as i64;',
     ' if idx>=0 as i64 { return 1 as i64; }\n return 0 as i64;'),
    ('identical-body checks compare with <=', ' if n_split==listed { return 1 as i64; }',
     ' if n_split<=listed { return 1 as i64; }'),
    ('big-endian magic accepted', ' if v==1179993927 as i64 { return 1 as i64; }',
     ' if v==1179993927 as i64 || v==1196444487 as i64 { return 1 as i64; }'),
    ('version 1 accepted', ' if v==2 as i64 { return 1 as i64; }', ' if v==1 as i64 { return 1 as i64; }'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG459_SHARDS')]
    if line:
        return r.returncode, line[-1]
    if 'PASS shards emission' not in r.stdout:
        return None, 'emission failed'
    # the module compiled but the fixture trapped (Goose bounds check, assert or
    # signal) before printing a summary: a caught mutation, not a build failure
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
