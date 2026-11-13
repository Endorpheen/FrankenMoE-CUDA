#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-506-p7-08-builtins-scalar'
SRC = E / 'source/builtins.goose'
ORIG = SRC.read_text()

M = [
    ('int abs keeps the sign', 'if v<0 { return mk_int(A,F,st,0-v); }', 'if v<0 { return mk_int(A,F,st,v); }'),
    ('float abs keeps the sign', 'if v<0.0 { return mk_float(A,F,st,0.0-v); }', 'if v<0.0 { return mk_float(A,F,st,v); }'),
    ('float to int rounds instead of truncating', 'return mk_int(A,F,st,F[roots[0]] as i64)',
     'return mk_int(A,F,st,(F[roots[0]]+0.5) as i64)'),
    ('float to int floors negative values', 'return mk_int(A,F,st,F[roots[0]] as i64)',
     'return mk_int(A,F,st,(F[roots[0]]-0.5) as i64)'),
    ('int to float returns an int', 'return mk_float(A,F,st,F[roots[0]])', 'return mk_int(A,F,st,vi(A,st,roots[0]))'),
    ('float string uses JSON output', 'let n=float_repr(sb,0,F[roots[0]])', 'let n=float_json(sb,0,F[roots[0]])'),
    ('default ignores the boolean kwarg', 'let check=v_bool(A,F,st,cv)!=0', 'let check:bool=false'),
    ('default always returns the fallback', 'if noval { return roots[1]; }\n return v0',
     'return roots[1];\n return v0'),
    ('tojson ignores indent', 'if vk(A,st,indent_v)==V_INT { indent=vi(A,st,indent_v); }',
     'if false { indent=vi(A,st,indent_v); }'),
    ('tojson loses the default item separator space', 'if indent<0 { isn=apps(isb,0,", "); }',
     'if indent<0 { isn=apps(isb,0,","); }'),
    ('tojson ignores sort_keys', 'if v_bool(A,F,st,sort_v)!=0 {', 'if false {'),
    ('the count error reports the minimum as maximum', 'p=apps(E,p," and ");\n p=appi(E,p,max);', 'p=apps(E,p," and ");\n p=appi(E,p,min);'),
    ('the type error omits the actual type', 'p=apps(E,p,", got ");\n p=type_name(A,B,st2,v,E[p..]) + p;',
     'p=apps(E,p,", got ");\n p=p;'),
    ('the name list loses safe', 'return apps(out,0,"abs default float int safe string tojson")',
     'return apps(out,0,"abs default float int string tojson")'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG506_BUILTINS')]
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
