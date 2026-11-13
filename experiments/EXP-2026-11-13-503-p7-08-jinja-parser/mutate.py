#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-503-p7-08-jinja-parser'
SRC = E / 'source/parser.goose'
ORIG = SRC.read_text()

M = [
    ('the ternary becomes a select', 'let id=nnew(A,F,st,K_TERNARY,tpos(A,st,pos0));',
     'let id=nnew(A,F,st,K_SELECT,tpos(A,st,pos0));'),
    ('a negated test loses its negation', '   negate=1;', '   negate=0;'),
    ('bare test arguments are ignored',
     '   if is(A,st,T_NUM) || is(A,st,T_STR) || is(A,st,T_OCB) || is(A,st,T_OSQ) { takes_arg=true; }',
     '   if false { takes_arg=true; }'),
    ('the computed member flag is inverted',
     'let computed:i64 = if ttype(A,st,op)==T_OSQ { 1 } else { 0 };',
     'let computed:i64 = if ttype(A,st,op)==T_OSQ { 0 } else { 1 };'),
    ('the slice start and stop swap', '''  let id=nnew(A,F,st,K_SLICE,tpos(A,st,start));
  set_c0(A,st,id,s0);
  set_c1(A,st,id,s1);''', '''  let id=nnew(A,F,st,K_SLICE,tpos(A,st,start));
  set_c0(A,st,id,s1);
  set_c1(A,st,id,s0);'''),
    ('keyword arguments become values', '''   arg=parse_expression(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   if is(A,st,T_EQ) {''', '''   arg=parse_expression(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   if false {'''),
    ('spread arguments are ignored',
     'if is(A,st,T_MUL) && tval_bytes_is(A,B,st,st[2],"*") {', 'if false {'),
    ('adjacent strings are not joined', ' while is(A,st,T_STR) {', ' while false {'),
    ('every number becomes a float', '   if B[vo+k]==46 { is_float=true; }', '   is_float=true;'),
    ('the literal sign is dropped', '   if neg { v=0-v; }', '   if false { v=0-v; }'),
    ('elif is appended to the body', '  l1_add(A,st,id,nested);', '  l0_add(A,st,id,nested);'),
    ('for else goes to the body', '''  while !stmt_n(A,B,st,"endfor","","") {
   let b=parse_any(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   l1_add(A,st,id,b);
  }''', '''  while !stmt_n(A,B,st,"endfor","","") {
   let b=parse_any(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   l0_add(A,st,id,b);
  }'''),
    ('the multiline set body is dropped', '''  while !stmt_n(A,B,st,"endset","","") {
   let b=parse_any(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   l0_add(A,st,id,b);
  }''', '''  while !stmt_n(A,B,st,"endset","","") {
   let b=parse_any(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
  }'''),
    ('caller arguments land in the body', '  l0_add(A,st,stmt,v);', '  l1_add(A,st,stmt,v);'),
    ('generation consumes no token', '''  let id=nnew(A,F,st,K_NOOP,tpos(A,st,start));
  st[2]=st[2]+1;''', '''  let id=nnew(A,F,st,K_NOOP,tpos(A,st,start));'''),
    ('the comment value is dropped', '''  let id=nnew(A,F,st,K_COMMENT,tpos(A,st,ti));
  let off,len=tok_copy(A,B,st,i);
  set_str(A,st,id,off,len);''', '''  let id=nnew(A,F,st,K_COMMENT,tpos(A,st,ti));
  let off,len=tok_copy(A,B,st,i);
  set_str(A,st,id,off,0);'''),
    ('the error text omits the token value', '  q=app(m,q,B,vo,vl);\n', ''),
    ('the error caret is one column off', 'var n:i64=pos-start+3;', 'var n:i64=pos-start+2;'),
    ('the test binds after filters', ' var operand=parse_unary(A,F,B,E,st,src);', ' var operand=parse_call_member(A,F,B,E,st,src);'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG503_PARSER')]
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
