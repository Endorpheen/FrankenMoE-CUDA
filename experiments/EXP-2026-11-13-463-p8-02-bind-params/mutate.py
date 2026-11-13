#!/usr/bin/env python3
"""Mutation gate for handler binding and path-parameter extraction (EXP463)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-463-p8-02-bind-params'
SRC = E / 'source' / 'bind.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('exact-length fragment is rejected', '  if start+fl>path.len {', '  if start+fl>=path.len {'),
    ('fragment bytes never compare equal', '    if frags[fo+k]!=path[start+k] { ok=0 as i64; }',
     '    if frags[fo+k]==path[start+k] { ok=0 as i64; }'),
    ('capture stops at a space instead of a slash', '      if path[sep]==47 as u8 { found=1 as i64; }',
     '      if path[sep]==32 as u8 { found=1 as i64; }'),
    ('capture does not skip the separator', '     start=sep+1 as i64;', '     start=sep;'),
    ('fully consumed path is not accepted', '  if start>=path.len { matched=1 as i64; }',
     '  if start>path.len { matched=1 as i64; }'),
    ('duplicate parameter names are accepted', '    if same!=0 as i64 { return 1 as i64; }',
     '    if same!=0 as i64 { return 0 as i64; }'),
    ('registration overwrites the first handler', ' if existing!=0 as i64 { return 1 as i64; }',
     ' if existing!=0 as i64 { return 0 as i64; }'),
    ('registered path is route + prefix', ' while i<prefix.len {\n  if o<dst.len { dst[o]=prefix[i]; }',
     ' var i0:i64=0 as i64;\n while i0<path.len {\n  if o<dst.len { dst[o]=path[i0]; }\n  o=o+1 as i64;\n  i0=i0+1 as i64;\n }\n i=prefix.len;\n while i<prefix.len {\n  if o<dst.len { dst[o]=prefix[i]; }'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG463_BIND')]
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
