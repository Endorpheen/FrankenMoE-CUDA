#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-476-p7-09-bounds'
SRC = E / 'source/bounds.goose'
ORIG = SRC.read_text()

M = [
    ('more_digits drops the single-digit shortcut',
     ' if mn==mx && mn==1 { return q; }',
     ' if mn==mx && mn==2 { return q; }'),
    ('digit_range always prints a range',
     ' if from==to {\n  q=append_c(out,q,from);',
     ' if from==to && from==(200 as u8) {\n  q=append_c(out,q,from);'),
    ('uniform_range cuts one shared digit short',
     ' if i>0 {\n  q=append_c(out,q,\'"\');\n  q=append_s(out,q,from,0,i);',
     ' if i>0 {\n  q=append_c(out,q,\'"\');\n  q=append_s(out,q,from,0,i-1);'),
    ('uniform_range always emits the tail branch',
     '   if !to_reached {',
     '   if true {'),
    ('the digit-length loop stops one early',
     '  while digits<max_digits {',
     '  while digits<max_digits-1 {'),
    ('the next minimum starts with 2',
     '   var op=append_c(buf,one_at,\'1\');',
     "   var op=append_c(buf,one_at,'2');"),
    ('negative bounds swap the endpoints',
     '   q=build_mm_at(out,q,0-hi,0-lo,decimals_left,1,buf,ast);',
     '   q=build_mm_at(out,q,0-lo,0-hi,decimals_left,1,buf,ast);'),
    ('negative minimum recursion keeps the sign',
     '   q=build_mm_at(out,q,I64_MIN,0-lo,decimals_left,0,buf,ast);',
     '   q=build_mm_at(out,q,I64_MIN,lo,decimals_left,0,buf,ast);'),
    ('zero minimum loses the top-level split',
     '    q=append_s(out,q,"[0] | [1-9] ",0,12);',
     '    q=append_s(out,q,"[0-9] ",0,6);'),
    ('small minimum starts the range at zero',
     '   if top_level!=0 { start=\'1\'; }\n   if c>start {',
     '   if top_level!=0 { start=\'0\'; }\n   if c>start {'),
    ('multi-digit minimum recurses on the whole string',
     '  let rest=parse_digits(ms[1..ml]);',
     '  let rest=parse_digits(ms[0..ml]);'),
    ('negative maximum allows a zero leading digit',
     '    q=append_s(out,q,"\\"-\\" [1-9] ",0,10);',
     '    q=append_s(out,q,"\\"-\\" [0-9] ",0,10);'),
    ('negative maximum recurses from zero',
     '  q=build_mm_at(out,q,0-max_v,I64_MAX,decimals_left,0,buf,ast);',
     '  q=build_mm_at(out,q,0,I64_MAX,decimals_left,0,buf,ast);'),
    ('no bounds no longer signal an error',
     ' if !has_min && !has_max { return -1; }',
     ' if !has_min && !has_max { return p; }'),
    ('const escapes a dash',
     "  } else if c=='\\\\' {",
     "  } else if c=='-' {\n   p=append_c(out,p,'\\\\');\n   p=append_c(out,p,'-');\n  } else if c=='\\\\' {"),
    ('enum separator is a comma',
     '  if i>0 { p=append_s(out,p," | ",0,3); }',
     '  if i>0 { p=append_s(out,p," , ",0,3); }'),
    ('more_digits prints a bound even when equal',
     ' if mx!=mn {',
     ' if mx==mx {'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG476_BOUNDS')]
    if line:
        return r.returncode, line[-1]
    if 'wrote ' not in r.stdout:
        return None, 'emission failed'
    last = r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'no stderr'
    return max(r.returncode, 1), 'build failure: ' + last[:140]


caught = 0
try:
    for label, old, new in M:
        if old not in ORIG:
            print('[skip]', label)
            continue
        SRC.write_text(ORIG.replace(old, new, 1))
        rc, line = run()
        if rc is None:
            print('[build-fail]', label)
            continue
        ok = rc != 0
        caught += ok
        print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
finally:
    SRC.write_text(ORIG)
    run()
print(f'mutations {caught}/{len(M)} caught')
