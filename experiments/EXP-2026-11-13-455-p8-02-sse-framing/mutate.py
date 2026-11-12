#!/usr/bin/env python3
"""Mutation gate for the SSE framing plan (EXP455)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-455-p8-02-sse-framing'
SRC = E / 'source' / 'sse.goose'
ORIG = SRC.read_text()

MUTATIONS = [
    ('chat frame loses the trailing blank line',
     '''  put(out,3 as i64,5 as i64);
  put(out,4 as i64,5 as i64);
  n=4 as i64;
 }else{
  if flavor==2 as i64 {''',
     '''  put(out,3 as i64,5 as i64);
  put(out,4 as i64,6 as i64);
  n=4 as i64;
 }else{
  if flavor==2 as i64 {'''),
    ('responses frame swaps the event and data parts',
     '''   put(out,1 as i64,3 as i64);
   put(out,2 as i64,4 as i64);''',
     '''   put(out,1 as i64,4 as i64);
   put(out,2 as i64,3 as i64);'''),
    ('anthropic inverts the event branch', '    if has_event!=0 as i64 {', '    if has_event==0 as i64 {'),
    ('terminator emits the payload step instead of [DONE]', '     put(out,2 as i64,7 as i64);',
     '     put(out,2 as i64,2 as i64);'),
    ('keep-alive sends a data prefix', '     put(out,1 as i64,8 as i64);', '     put(out,1 as i64,1 as i64);'),
    ('event prefix measured as six bytes', '  if s==3 as i64 { total=total+7 as i64; }   // "event: "',
     '  if s==3 as i64 { total=total+6 as i64; }   // "event: "'),
    ('newline measured as two bytes', '  if s==5 as i64 { total=total+1 as i64; }   // newline',
     '  if s==5 as i64 { total=total+2 as i64; }   // newline'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG455_SSE')]
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
