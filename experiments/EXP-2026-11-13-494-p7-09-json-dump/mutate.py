#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-494-p7-09-json-dump'
SRC = E / 'source/jsondump.goose'
ORIG = SRC.read_text()

M = [
    ('quotes are not escaped', "  if c=='\"' { p=appc(out,p,92); p=appc(out,p,'\"'); i=i+1; }", "  if c=='\u0001' { p=appc(out,p,92); p=appc(out,p,'\"'); i=i+1; }"),
    ('backslashes are not escaped', "  else if c=='\\\\' { p=appc(out,p,92); p=appc(out,p,'\\\\'); i=i+1; }", "  else if c=='\\\\' { p=appc(out,p,c); i=i+1; }"),
    ('backspace loses its short form', "  else if c==8 { p=appc(out,p,92); p=appc(out,p,'b'); i=i+1; }", "  else if c==8 { p=appc(out,p,c); i=i+1; }"),
    ('control bytes are raw', "  else if c<0x20 { p=hexlow(out,p,c as i64); i=i+1; }", "  else if c<0x20 { p=appc(out,p,c); i=i+1; }"),
    ('the escape hex is upper case', ' let d: const u8[:] = "0123456789abcdef";', ' let d: const u8[:] = "0123456789ABCDEF";'),
    ('pretty has no space after the colon', "  if indent>=0 { p=appc(out,p,' '); }", "  if indent<0 { p=appc(out,p,' '); }"),
    ('compact has a space after the colon', "  if indent>=0 { p=appc(out,p,' '); }", "  if indent>=0 || true { p=appc(out,p,' '); }"),
    ('the indented level is off by one', "    p=indent_to(out,p,indent,level+1);", "    p=indent_to(out,p,indent,level);"),
    ('an empty array gets a newline', '  if k==K_ARRAY { return app(out,p,"[]",0,2); }', '  if k==K_ARRAY { return app(out,p,"[\\n]",0,3); }'),
    ('arrays keep a trailing comma', "   if i+1<n { p=appc(out,p,','); }", "   p=appc(out,p,',');"),
    ('objects keep a trailing comma', "  if i2+1<n { p=appc(out,p,','); }", "  p=appc(out,p,',');"),
    ('object keys are reversed', "  p=esc_string(out,p,sbuf,jkey[base+i2],jkl[base+i2],safe);", "  p=esc_string(out,p,sbuf,jkey[base+n-1-i2],jkl[base+n-1-i2],safe);"),
    ('integers lose their sign', " if neg { p=appc(dst,p,'-'); }", " if false { p=appc(dst,p,'-'); }"),
    ('unsigned values print as signed', "  bits=raw as u64;", "  bits=(0-(raw+1)) as u64+1;"),
    ('the fixed threshold is one higher', " if k<=n && n<=15 {", " if k<=n && n<=16 {"),
    ('integral doubles lose the .0', '  return app(out,p,".0",0,2);', '  return p;'),
    ('the shortest search starts at two digits', " var p:i64=1;\n var done:bool=false;", " var p:i64=2;\n var done:bool=false;"),
    ('negative zero loses its sign', "  if is_inf(r) && r<0.0 { neg=true; }", "  if is_inf(r) && r<0.0 { neg=false; }"),
    ('the exponent loses its padding', " if ax<10 { p=appc(out,p,'0'); }", " if ax<0 { p=appc(out,p,'0'); }"),
    ('the exponent sign is always plus', " if ex<0 { p=appc(out,p,'-'); } else { p=appc(out,p,'+'); }", " p=appc(out,p,'+');"),
    ('the leading zeros branch is off', "  while z<0-n { p=appc(out,p,'0'); z=z+1; }", "  while z<1-n { p=appc(out,p,'0'); z=z+1; }"),
    ('the safe replacement is one byte', "    p=appc(out,p,239);\n    p=appc(out,p,191);\n    p=appc(out,p,189);", "    p=appc(out,p,239);"),
    ('safe mode copies invalid runs raw', "   } else if safe!=0 {", "   } else if safe!=1 {"),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=240, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG494_JSONDUMP')]
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
