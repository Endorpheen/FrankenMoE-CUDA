#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-486-p7-09-peg-grammar'
SRC = E / 'source/peggram.goose'
ORIG = SRC.read_text()

M = [
    ('literals are not escaped',
     '  } else if c==\'"\' {\n   p=append_c(out,p,\'\\\\\');\n   p=append_c(out,p,\'"\');',
     '  } else if c==\'"\' {\n   p=append_c(out,p,\'"\');'),
    ('sequence parts lose the separator',
     "    if have { p=append_c(out,p,' '); }",
     '    if have { p=append_c(out,p,(0 as u8)); }'),
    ('sequence does not parenthesise groups',
     '   if is_seq_or_choice(children[base+i],kind,children,cfirst)!=0 {',
     '   if false {'),
    ('choice joins with a comma',
     '   if i>0 { p=append_s(out,p," | ",0,3); }',
     '   if i>0 { p=append_s(out,p,", ",0,2); }'),
    ('the optional suffix becomes a star',
     "  if lo==0 && hi==1 { p=append_c(out,p,'?'); }",
     "  if lo==0 && hi==1 { p=append_c(out,p,'*'); }"),
    ('repetition does not parenthesise groups',
     '  if is_seq_or_choice(children[cfirst[id]],kind,children,cfirst)!=0 {\n   p=append_c(out,p,\'(\');',
     '  if false {\n   p=append_c(out,p,\'(\');'),
    ('any becomes the word any',
     "  st[0]=append_c(out,at,'.');\n  return at,1;",
     '  let e=append_s(out,at,"any",0,3);\n  st[0]=e;\n  return at,e-at;'),
    ('space is upper case',
     '  let e=append_s(out,at,"space",0,5);',
     '  let e=append_s(out,at,"SPACE",0,5);'),
    ('rules emit their child text',
     ' if k==K_RULE || k==K_REF {\n  let si=sidx[id];',
     ' if k==K_RULE || k==K_REF {\n  let si=sidx[id];\n  if k==K_RULE { return to_gbnf(children[cfirst[id]],out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir); }'),
    ('reachability never marks rules',
     '     marks[i]=1;',
     '     marks[i]=marks[i];'),
    ('rule text uses the rule parser',
     '   let co,cl=to_gbnf(children[cfirst[rid[z]]],out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir);',
     '   let co,cl=to_gbnf(rid[z],out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir);'),
    ('empty variants emit a token',
     ' st[0]=at;\n return at,0;\n}',
     ' let e=append_s(out,at,"eps",0,3);\n st[0]=e;\n return at,e-at;\n}'),
    ('tag and atomic are not unwrapped',
     '  if k==K_TAG || k==K_ATOMIC {\n   id=children[cfirst[id]];\n  } else {\n   looping=false;\n  }',
     '  looping=false;'),
    ('bounded repetitions lose the comma',
     '   p=append_c(out,p,\',\');\n   p=append_dec(out,p,hi);',
     '   p=append_dec(out,p,hi);'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG486_PEGGRAM')]
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
