#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-474-p7-09-pattern'
SRC = E / 'source/pattern.goose'
ORIG = SRC.read_text()

M = [
    ('unanchored patterns are accepted',
     "if pattern.len<2 || pattern[0]!='^' || pattern[pattern.len-1]!='$' {",
     "if pattern.len<2 {"),
    ('merge drops the opening literal quote',
     "    p=append_c(tb,p,'\"');\n    have_item=true;",
     "    have_item=true;"),
    ('dot rule is named dots',
     '   let dotname: const u8[:] = "dot";',
     '   let dotname: const u8[:] = "dots";'),
    ('sub-rule numbering starts at the previous count',
     '        let idx=st[S_SUBS]+1;',
     '        let idx=st[S_SUBS];'),
    ('sub-rule separator is X',
     "        cp=append_c(nb,name.len,'-');",
     "        cp=append_c(nb,name.len,'X');"),
    ('every backslash escape becomes a literal',
     '      if escaped_in_literals(nx) {',
     '      if true {'),
    ('unsupported class escape is copied verbatim',
     '     if e==0 {\n      set_fail_at(st,reason,1,"unsupported escape in character class: ",p,st[S_I],2);',
     "     if e==0 {\n      q=append_c(tb,q,p[st[S_I]]);\n      st[S_I]=st[S_I]+1;"),
    ('repeat suffix is emitted as #',
     '    let q=append_c(tb,st[S_TX],c);',
     "    let q=append_c(tb,st[S_TX],'#');"),
    ('bounded repetition loses its upper bound',
     ' let has_max = mx!=INT_MAX;',
     ' let has_max = false;'),
    ('optional form becomes plus',
     " if mn==0 && mx==1 {\n  p=append_c(tb,p,'?');",
     " if mn==0 && mx==1 {\n  p=append_c(tb,p,'+');"),
    ('trailing backslash is accepted',
     '     if st[S_I]==st[S_END]-1 {\n      set_fail(st,reason,2,"trailing backslash");',
     '     if st[S_I]==-5 {\n      set_fail(st,reason,2,"trailing backslash");'),
    ('group close does not leave the depth',
     '    st[S_DEPTH]=st[S_DEPTH]-1;\n    return join_seq(st,tb,to,tl,ti,base);',
     '    return join_seq(st,tb,to,tl,ti,base);'),
    ('non-capturing group keeps its colon',
     '      st[S_I]=st[S_I]+2;\n     } else {\n      set_fail(st,reason,1,"unsupported group syntax");',
     '      st[S_I]=st[S_I]+1;\n     } else {\n      set_fail(st,reason,1,"unsupported group syntax");'),
    ('fallback rule text is one byte',
     '  z=0;\n  while z<prim.len { rule_out[z]=prim[z]; z=z+1; }\n  st[S_RULE]=prim.len;',
     '  rule_out[0]=prim[0];\n  st[S_RULE]=1;'),
    ('uuid format selects date-string',
     ' if format==1 { return 1; }',
     ' if format==1 { return 2; }'),
    ('length rule omits the space before the quote',
     " p=append_c(out,p,' ');\n p=append_c(out,p,'\"');\n p=append_c(out,p,'\\\\');",
     " p=append_c(out,p,'\"');\n p=append_c(out,p,'\\\\');"),
    ('group wrapper uses brackets',
     "      var q=append_c(tb,at,'(');",
     "      var q=append_c(tb,at,'[');"),
    ('literal quote is not escaped',
     "     q=append_c(tb,q,'\\\\');\n     q=append_c(tb,q,'\"');",
     "     q=append_c(tb,q,'\"');"),
    ('alternation is emitted as a literal',
     '   push_copy(st,tb,to,tl,ti,"|",0,1,0);',
     '   push_copy(st,tb,to,tl,ti,"|",0,1,1);'),
    ('format_literal also escapes a dash',
     "  } else if c=='\\\\' {\n   p=append_c(out,p,'\\\\');",
     "  } else if c=='-' {\n   p=append_c(out,p,'\\\\');\n   p=append_c(out,p,'-');\n  } else if c=='\\\\' {\n   p=append_c(out,p,'\\\\');"),
    ('format_literal leaves CR raw',
     "  if c==(13 as u8) {\n   p=append_c(out,p,'\\\\');\n   p=append_c(out,p,'r');",
     "  if c==(13 as u8) {\n   p=append_c(out,p,c);"),
    ('dotall dot class is wrong',
     ' let narrow: const u8[:] = "[^\\\\x0A\\\\x0D]";',
     ' let narrow: const u8[:] = "[^\\\\x0A\\\\x0C]";'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=120, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)' 
    line = [x for x in r.stdout.splitlines() if x.startswith('FG474_PATTERN')]
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
