#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-479-p7-09-visit-dispatch'
SRC = E / 'source/visit.goose'
ORIG = SRC.read_text()

M = [
    ('reserved names lose the trailing dash',
     "  p=append_c(tb,p,'-');\n  return p-at;",
     '  return p-at;'),
    ('an empty name no longer becomes root',
     '  return append_s(tb,at,"root",0,4)-at;',
     '  return 0;'),
    ('sub-names always get a dash',
     " if name.len!=0 { p=append_c(tb,p,'-'); }",
     " p=append_c(tb,p,'-');"),
    ('ANY embeds the value content instead of the alias',
     '  return add_rule(mo,ml,vo,vl,mb,st,rule_name,mb[va..va+vb]);',
     '  return add_rule(mo,ml,vo,vl,mb,st,rule_name,strs[sdir[2*ptcontent[0]+0]..sdir[2*ptcontent[0]+0]+sdir[2*ptcontent[0]+1]]);'),
    ('empty object embeds the object content instead of the alias',
     '  return add_rule(mo,ml,vo,vl,mb,st,rule_name,mb[oa..oa+ob]);',
     '  return add_rule(mo,ml,vo,vl,mb,st,rule_name,strs[sdir[2*ptcontent[1]+0]..sdir[2*ptcontent[1]+0]+sdir[2*ptcontent[1]+1]]);'),
    ('primitive dependencies are not added',
     '  if dep>=0 {',
     '  if dep>=99 {'),
    ('the resolving guard is inverted',
     '  if !resolving {',
     '  if resolving {'),
    ('anyOf alternatives use dots',
     "    ap=append_s(tb,ap,\"alternative-\",0,12);",
     "    ap=append_s(tb,ap,\"alternativX-\",0,12);"),
    ('anyOf joins with a comma',
     '   if i>0 { p=append_s(tb,p," | ",0,3); }',
     '   if i>0 { p=append_s(tb,p," , ",0,3); }'),
    ('kv text loses its trailing space',
     '   kp=append_s(tb,kp," space \\":\\" space ",0,17);',
     '   kp=append_s(tb,kp," space \\":\\" space ",0,16);'),
    ('required chain comma loses a space',
     '   if have_req { p=append_s(tb,p," \\",\\" space ",0,11); }',
     '   if have_req { p=append_s(tb,p," \\",\\" space ",0,10); }'),
    ('rest rule uses the alternative index for the key',
     '    let kns=keyname[pi];',
     '    let kns=keyname[ii];'),
    ('optional chain becomes a star',
     '    cp=append_s(ob,cp," )?",0,3);',
     '    cp=append_s(ob,cp," )*",0,3);'),
    ('a colliding rule always creates a new entry',
     ' if found<0 {',
     ' if true {'),
    ('sanitization keeps invalid runs apart',
     '   if !run {',
     '   if true {'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG479_VISIT')]
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
