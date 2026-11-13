#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-498-p7-09-chars'
SRC = E / 'source/chars.goose'
ORIG = SRC.read_text()

M = [
    ('the leading bracket is kept', ' if len>0 && pat[off]==(91 as u8) { off=off+1; len=len-1; }', ''),
    ('a real trailing byte is dropped', ' if len>0 && pat[off+len-1]==(93 as u8) { len=len-1; }', ' if len>0 { len=len-1; }'),
    ('negation is never detected', ' if len>0 && pat[off]==(94 as u8) { st[0]=1; off=off+1; len=len-1; }', ' if len>0 && pat[off]==(94 as u8) { off=off+1; len=len-1; }'),
    ('a range needs two bytes after the dash', '  if i+1<s.len && s[i]==(45 as u8) {', '  if i+2<s.len && s[i]==(45 as u8) {'),
    ('the range end is not skipped', '   i=i+1+used2;', '   i=i+used2;'),
    ('newline escapes to the letter n', "  if e=='n' { return 10,2; }", "  if e=='n' { return 110,2; }"),
    ('bad hex yields zero instead of the letter', '   if n>0 { return v,2+n; }\n   return 120,2;', '   if n>0 { return v,2+n; }\n   return 0,2;'),
    ('x escapes read one hex digit', '   let v,n=hex_escape(s,pos+2,2);', '   let v,n=hex_escape(s,pos+2,1);'),
    ('an unknown escape yields a backslash', '  return (e as i64),2;', '  return 92,2;'),
    ('plain bytes are masked to ASCII', ' return (s[pos] as i64),1;', ' return ((s[pos] as i64) & 127),1;'),
    ('U escapes read six digits', '   let v,n=hex_escape(s,pos+2,8);', '   let v,n=hex_escape(s,pos+2,6);'),
    ('the caret is always stripped', " if len>0 && pat[off]==(94 as u8) { st[0]=1; off=off+1; len=len-1; }", " if len>0 { off=off+1; len=len-1; st[0]=1; }"),
    ('a trailing backslash is dropped', ' return (s[pos] as i64),1;', ' return 0,1;'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG498_CHARS')]
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
