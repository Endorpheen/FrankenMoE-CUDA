#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-507-p7-08-jinja-runtime-expr'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('and returns the left value when true', 'if v_bool(A,F,st,l)!=0 { return eval_node(ac1(A,st,id),ctxc,noff,nlen,cval,A,F,B,E,st); }',
     'if v_bool(A,F,st,l)!=0 { return l; }'),
    ('or returns the left value when true', 'if v_bool(A,F,st,l)!=0 { return l; }',
     'if v_bool(A,F,st,l)!=0 { return mk_bool(A,F,st,0); }'),
    ('equality uses the strict form', 'if bytes_eq(op,"==") { return mk_bool(A,F,st,if eqv(A,B,F,st,l,r) { 1 } else { 0 }); }',
     'if bytes_eq(op,"==") { return mk_bool(A,F,st,if nonequal(A,B,F,st,l,r) { 1 } else { 0 }); }'),
    ('float arithmetic returns an int', 'let isf:bool = lk==V_FLOAT || rk==V_FLOAT;\n   if isf { return mk_float(A,F,st,res); }',
     'let isf:bool = false;\n   if isf { return mk_float(A,F,st,res); }'),
    ('division returns an int', 'if bytes_eq(op,"/") { return mk_float(A,F,st,a/b); }',
     'if bytes_eq(op,"/") { return mk_int(A,F,st,(a/b) as i64); }'),
    ('modulo becomes subtraction', 'let rem:f64=a%b;', 'let rem:f64=a-b;'),
    ('less-than is inverted', 'if bytes_eq(op,"<") { return mk_bool(A,F,st,if a<b { 1 } else { 0 }); }',
     'if bytes_eq(op,"<") { return mk_bool(A,F,st,if a>b { 1 } else { 0 }); }'),
    ('plus stops concatenating strings', '(bytes_eq(op,"~") || bytes_eq(op,"+")) {',
     '(bytes_eq(op,"~")) {'),
    ('string repetition adds one copy', 'while i<rep {', 'while i<rep+1 {'),
    ('a missing identifier becomes none', 'return mk_undef_hint(A,F,B,st,name)', 'return mk_none(A,F,st)'),
    ('null concatenation workaround is disabled', 'let ln:bool = lk==V_UNDEF || lk==V_NONE;',
     'let ln:bool = false;'),
    ('the undefined error loses the operator', 'var p=apps(mb,0,"Cannot perform operation ");\n  p=app(mb,p,op,0,op.len);',
     'var p=apps(mb,0,"Cannot perform operation ");'),
    ('unary minus on a float returns an int', 'if kk==V_FLOAT { return mk_float(A,F,st,0.0-F[arg]); }',
     'if kk==V_FLOAT { return mk_int(A,F,st,0-(F[arg] as i64)); }'),
    ('the node wrapper drops the position', 'p=apps(m,p," at position ");\n p=appi(m,p,pos);',
     'p=apps(m,p," at position ");\n p=appi(m,p,0);'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG507_RUNTIME')]
    if line:
        return r.returncode, line[-1]
    if 'wrote ' not in r.stdout:
        return None, 'emission failed'
    last = r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'no stderr'
    return max(r.returncode, 1), 'build failure: ' + last[:140]


lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
caught = 0
try:
    for label, old, new in M[lo:hi]:
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
print(f'mutations {caught}/{hi-lo} caught (range {lo}:{hi})')
