#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-504-p7-08-jinja-values'
SRC = E / 'source/values.goose'
ORIG = SRC.read_text()

M = [
    ('an int is always true', ' if k==V_INT { if vi(A,st,id)!=0 { return 1; } return 0; }',
     ' if k==V_INT { return 1; }'),
    ('a negative float is false', ' if k==V_FLOAT { if F[id]!=0.0 { return 1; } return 0; }',
     ' if k==V_FLOAT { if F[id]>0.0 { return 1; } return 0; }'),
    ('an empty string is true', ' if k==V_STRING { if vfl(A,st,id)>0 { return 1; } return 0; }',
     ' if k==V_STRING { return 1; }'),
    ('an empty array is true', ' if k==V_ARRAY || k==V_TUPLE { if vcc(A,st,id)>0 { return 1; } return 0; }',
     ' if k==V_ARRAY || k==V_TUPLE { return 1; }'),
    ('undefined is true', ' if k==V_OBJECT { if vvc(A,st,id)>0 { return 1; } return 0; }\n return 0;',
     ' if k==V_OBJECT { if vvc(A,st,id)>0 { return 1; } return 0; }\n return 1;'),
    ('a bool is not numeric', ' return k==V_INT || k==V_FLOAT || k==V_BOOL;',
     ' return k==V_INT || k==V_FLOAT;'),
    ('an array is immutable', ' return !(k==V_ARRAY || k==V_OBJECT);', ' return true;'),
    ('undefined is hashable', ' if k==V_UNDEF { return false; }\n return true;', ' return true;'),
    ('an object is always hashable', ''' if k==V_OBJECT {
  // the native check looks at the values only, not the keys
  var i:i64=0;
  while i<vvc(A,st,id) {
   let c=A[pool(A,st)+vvs(A,st,id)+i];
   if !is_immutable(A,st,c) || !is_hashable(A,st,c) { return false; }
   i=i+1;
  }
  return true;
 }''', ''' if k==V_OBJECT {
  return true;
 }'''),
    ('numeric equality ignores the float', '  return vi(A,st,a)==vi(A,st,b) && F[a]==F[b];',
     '  return vi(A,st,a)==vi(A,st,b);'),
    ('string equality ignores the bytes', '  return bytes_eq(vstr(B,A,st,a),vstr(B,A,st,b));', '  return true;'),
    ('array equality ignores the length', '  if vcc(A,st,a)!=vcc(A,st,b) { return false; }', '  if false { return false; }'),
    ('float strict inequality uses the int part', '  return !(vk(A,st,b)==V_FLOAT && F[a]==F[b]);',
     '  return !(vk(A,st,b)==V_FLOAT && vi(A,st,a)==vi(A,st,b));'),
    ('numeric compare uses the int part', '  return cmp_op(F[a],F[b],op);', '  return cmp_op(vi(A,st,a) as f64,vi(A,st,b) as f64,op);'),
    ('string compare is disabled', '  return bytes_cmp(sa[0..na],sb[0..nb],op);', '  return 0;'),
    ('bool ordering is allowed', '''  st[1]=1;
  let p=apps(E,0,"Unsupported comparison operator for bool type");
  st[2]=p;
  return -1;''', '''  return cmp_op((vi(A,st,a)) as f64,(vi(A,st,b)) as f64,op);'''),
    ('float as_string keeps trailing zeros', '''  var p:i64=at+n;
  while p>at && out[p-1]=='0' { p=p-1; }
  if p>at && out[p-1]=='.' { p=appc(out,p,'0'); }
  return p;''', '''  return at+n;'''),
    ('repr never JSON-encodes a quote', '  if has_quote { return json_string(out,at,s); }', '  if false { return json_string(out,at,s); }'),
    ('the integer string drops its sign', ''' if v<0 {
  p=appc(dst,p,'-');
  m=((0-(v+1)) as u64)+1;
 } else {''', ''' if false {
  p=appc(dst,p,'-');
  m=((0-(v+1)) as u64)+1;
 } else {'''),
    ('a single tuple loses its comma', '  if tuple && n==1 { p=appc(out,p,\',\'); }', '  if false { p=appc(out,p,\',\'); }'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG504_VALUES')]
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
