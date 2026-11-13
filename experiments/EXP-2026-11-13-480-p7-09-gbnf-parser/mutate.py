#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-480-p7-09-gbnf-parser'
SRC = E / 'source/gbnf.goose'
ORIG = SRC.read_text()

M = [
    ('unknown escapes are accepted',
     "  if c=='\\\\' || c=='\"' || c=='[' || c==']' || c=='-' { return c,i0+2; }\n  fail_at(g,err,st,\"unknown escape\",i0);\n  return 0,i0;",
     "  return c,i0+2;"),
    ('u escapes read three hex chars',
     "  if c=='u' { return parse_hex(g,i0+2,4,err,st); }",
     "  if c=='u' { return parse_hex(g,i0+2,3,err,st); }"),
    ('short hex runs are accepted',
     ' if bad || k<n {',
     ' if false {'),
    ('a literal may end at EOF',
     "   while st[0]<g.len && g[st[0]]!='\"' {",
     "   while false {"),
    ('class negation is ignored',
     "   if st[0]<g.len && g[st[0]]=='^' {\n    st[0]=st[0]+1;\n    stype=E_NOT;\n   }",
     "   if st[0]<g.len && g[st[0]]=='^' {\n    st[0]=st[0]+1;\n   }"),
    ('class alternates use the start type',
     "     if st[3]>last_sym_start { etype=E_ALTCHAR; }",
     "     if st[3]>last_sym_start { etype=stype; }"),
    ('ranges never extend to the next char',
     "     if ni<g.len && g[ni]=='-' && (ni+1>=g.len || g[ni+1]!=']') {",
     "     if false {"),
    ('rule refs drop a character',
     "   let id=sym_get(g,st[0],ne-st[0],sbuf,soff,slen,st);",
     "   let id=sym_get(g,st[0],ne-st[0]-1,sbuf,soff,slen,st);"),
    ('generated ids use a dash',
     " end=append_c(sbuf,end,'_');",
     " end=append_c(sbuf,end,'-');"),
    ('alternation markers are missing',
     "  wtype[st[3]]=E_ALT;\n  wval[st[3]]=0;\n  st[3]=st[3]+1;",
     "  st[3]=st[3]+0;"),
    ('rule references become chars',
     "   wtype[st[3]]=E_REF;\n   wval[st[3]]=id;",
     "   wtype[st[3]]=E_CHAR;\n   wval[st[3]]=id;"),
    ('dot becomes a char element',
     "   wtype[st[3]]=E_ANY;",
     "   wtype[st[3]]=E_CHAR;"),
    ('zero-min repetitions keep the repeated element',
     " if mn==0 {\n  st[3]=last_sym_start;\n } else {",
     " if mn==0 {\n  st[3]=st[3];\n } else {"),
    ('star does not self-reference',
     "   if no_max { wval[e]=rec_id; } else { wval[e]=last_rec; }",
     "   if no_max { wval[e]=last_rec; } else { wval[e]=last_rec; }"),
    ('optional count is the maximum',
     ' if no_max { n_opt=1; } else { n_opt=mx-mn; }',
     ' if no_max { n_opt=1; } else { n_opt=mx; }'),
    ('undefined refs are not validated',
     '      if v>=st[2] || rlen[v]==0 {',
     '      if false {'),
    ('the ::= check accepts ::',
     " if st[0]+2>=g.len || g[st[0]]!=':' || g[st[0]+1]!=':' || g[st[0]+2]!='=' {\n  fail_at(g,err,st,\"expecting ::=\",st[0]);",
     " if st[0]+1>=g.len || g[st[0]]!=':' || g[st[0]+1]!=':' {\n  fail_at(g,err,st,\"expecting ::=\",st[0]);"),
    ('trailing text after a rule is accepted',
     " } else if st[0]<g.len {\n  fail_at(g,err,st,\"expecting newline or end\",st[0]);",
     " } else if st[0]<-5 {\n  fail_at(g,err,st,\"expecting newline or end\",st[0]);"),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG480_GBNF')]
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
