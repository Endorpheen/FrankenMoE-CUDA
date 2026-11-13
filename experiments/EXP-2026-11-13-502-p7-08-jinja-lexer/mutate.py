#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-502-p7-08-jinja-lexer'
SRC = E / 'source/lexer.goose'
ORIG = SRC.read_text()

M = [
    ('crlf is not collapsed', 'if i+1<source.len && source[i+1]==10 { i=i+2; } else { i=i+1; }',
     'if false { i=i+2; } else { i=i+1; }'),
    ('the trailing newline is kept', 'if source.len>0 && source[source.len-1]==10 { n=n-1; }', 'if false { n=n-1; }'),
    ('depth never resets on an expression', 'if mt==T_OE { depth=0; }', 'if mt==T_OE { depth=depth; }'),
    ('close expression ignores the depth', 'if starts(s,p,"}}") && depth<=0 { return T_CE,2; }',
     'if starts(s,p,"}}") { return T_CE,2; }'),
    ('text stops at any brace', 'at(s,p)==123 && (at(s,p+1)==37 || at(s,p+1)==123 || at(s,p+1)==35)',
     'at(s,p)==123'),
    ('lstrip_blocks is off', 'if at(s,p)==123 && (at(s,p+1)==37 || at(s,p+1)==35 || at(s,p+1)==45) {',
     'if false {'),
    ('trim_blocks is off', 'if last_block_can_rm_newline && te>ts && at(s,ts)==10 { ts=ts+1; }',
     'if false { ts=ts+1; }'),
    ('rstrip of the previous block is reversed', 'while ts<te && is_space4(at(s,ts)) { ts=ts+1; }',
     'while te>ts && is_space4(at(s,te-1)) { te=te-1; }'),
    ('lstrip of the current block is reversed', 'while te>ts && is_space4(at(s,te-1)) { te=te-1; }',
     'while ts<te && is_space4(at(s,ts)) { ts=ts+1; }'),
    ('the explicit dash after a marker is kept', 'if at(s,p)==45 && (lt==T_OE || lt==T_OS) {', 'if false {'),
    ('a unary minus is always binary', 'var binary:bool=false;', 'var binary:bool=true;'),
    ('newline escape maps to the letter', "if e=='n' { v=10; }", "if e=='n' { v=110; }"),
    ('hex escapes read one digit', 'let h2=hexval(at(s,p+1))', 'let h2=hexval(at(s,p))'),
    ('an unknown escape is kept literally', 'if v<0 {', 'if false {'),
    ('a fraction needs no digit', 'if at(s,p)==46 && p+1<n && is_digit(at(s,p+1)) {', 'if at(s,p)==46 {'),
    ('identifiers reject underscores', "return c=='_';", 'return false;'),
    ('vertical tab is not whitespace', 'return c==32 || c==9 || c==10 || c==11 || c==12 || c==13;',
     'return c==32 || c==9 || c==10 || c==12 || c==13;'),
    ('the error caret is one column off', 'var n:i64=pos-start+3;', 'var n:i64=pos-start+2;'),
    ('the comment opener is skipped by one', '   p=p+2;\n   let off=st[7];', '   p=p+1;\n   let off=st[7];'),
    ('the text token ignores its start', 'push_tok(tt,voff,vlen,tpos,st,T_TEXT,off,te-ts,start_pos);',
     'push_tok(tt,voff,vlen,tpos,st,T_TEXT,off,te-ts,p);'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG502_LEXER')]
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
