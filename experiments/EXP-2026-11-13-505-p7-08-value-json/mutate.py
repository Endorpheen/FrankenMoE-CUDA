#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-505-p7-08-value-json'
SRC = E / 'source/valuesjson.goose'
ORIG = SRC.read_text()

M = [
    ('the fixed form stops one digit earlier', ' if point>-4 && point<=16 {', ' if point>-4 && point<=15 {'),
    ('the exponent loses its two-digit padding', "  if ax<10 { p=appc(out,p,'0'); }", ''),
    ('the exponent is always positive', "  if exp10<0 { p=appc(out,p,'-'); } else { p=appc(out,p,'+'); }",
     "  p=appc(out,p,'+');"),
    ('the reads-back check is inverted', '  if prec==16 || back==mag {', '  if prec==16 || back!=mag {'),
    ('the leading-zero branch is off by one', '   while z<0-point { p=appc(out,p,\'0\'); z=z+1; }',
     '   while z<1-point { p=appc(out,p,\'0\'); z=z+1; }'),
    ('an integral float loses its .0', '   p=apps(out,p,".0");', ''),
    ('the scientific mantissa keeps all digits', '''  p=appc(out,p,digits[0]);
  if dlen>1 {
   p=appc(out,p,'.');
   var z:i64=1;
   while z<dlen { p=appc(out,p,digits[z]); z=z+1; }
  }''', '''  var z:i64=0;
  while z<dlen { p=appc(out,p,digits[z]); z=z+1; }'''),
    ('NaN is lowercase', ' if is_nan(val) { return apps(out,at,"NaN"); }', ' if is_nan(val) { return apps(out,at,"nan"); }'),
    ('negative infinity loses its sign', ' if is_inf(val) { if val<0.0 { return apps(out,at,"-Infinity"); } return apps(out,at,"Infinity"); }',
     ' if is_inf(val) { return apps(out,at,"Infinity"); }'),
    ('quotes are not escaped in JSON strings', '  if c==\'"\' { p=appc(out,p,92); p=appc(out,p,\'"\'); }', '  if false { p=appc(out,p,92); p=appc(out,p,\'"\'); }'),
    ('control bytes are raw in JSON strings', '  else if c<0x20 {', '  else if c<0 {'),
    ('the JSON hex escape is upper case', '   let d: const u8[:] = "0123456789abcdef";',
     '   let d: const u8[:] = "0123456789ABCDEF";'),
    ('arrays lose the newline after the opener', '''  if n>0 {
   if indent>=0 { p=appc(out,p,10); }
   var i:i64=0;''', '''  if n>0 {
   var i:i64=0;'''),
    ('array items lose their separator', '    if i<n-1 { p=apps(out,p,", "); }', '    if false { p=apps(out,p,", "); }'),
    ('the key separator has no space', '    p=apps(out,p,": ");', '    p=apps(out,p,":");'),
    ('a float key uses as_string', '    if vk(A,st,key)==V_FLOAT {', '    if false {'),
    ('a tuple is not a JSON array', ''' if k==V_ARRAY || k==V_TUPLE {
  p=appc(out,p,'[');''', ''' if k==V_ARRAY {
  p=appc(out,p,'[');'''),
    ('none serializes as its type name', ' if k==V_NONE || k==V_UNDEF { return apps(out,at,"null"); }',
     ' if k==V_NONE || k==V_UNDEF { if k==V_NONE { return apps(out,at,"None"); } return apps(out,at,"null"); }'),
    ('the object closing indent is skipped', '''   if indent>0 { p=spaces_to(out,p,lvl*indent); }
  }
  return appc(out,p,'}');''', '''  }
  return appc(out,p,'}');'''),
    ('the item indent misses the extra step', '    if indent>0 { p=spaces_to(out,p,lvl*indent+indent); }',
     '    if indent>0 { p=spaces_to(out,p,lvl*indent); }'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG505_VALUEJSON')]
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
