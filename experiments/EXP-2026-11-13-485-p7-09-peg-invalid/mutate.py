#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-485-p7-09-peg-invalid'
SRC = E / 'source/peginv.goose'
ORIG = SRC.read_text()

M = [
    ('until does not record invalid runs',
     '    invp[st[4]*2+0]=pos;\n    invp[st[4]*2+1]=step;\n    st[4]=st[4]+1;',
     '    st[4]=st[4];'),
    ('until records a one-byte invalid run',
     '    invp[st[4]*2+1]=step;',
     '    invp[st[4]*2+1]=1;'),
    ('until succeeds on a lenient incomplete run',
     '   if uk==1 && lenient!=0 { return R_NEED,start,last,st[1],0,ib,st[4]-ib; }',
     '   if uk==1 && lenient!=0 { return R_OK,start,last,st[1],0,ib,st[4]-ib; }'),
    ('until ignores partial delimiter prefixes',
     '     if ok { hit=true; }',
     '     if ok && z==d_len { hit=true; }'),
    ('until reports the input end',
     '  return R_OK,start,last,st[1],0,ib,st[4]-ib;\n }\n if k==K_RULE',
     '  return R_OK,start,input.len,st[1],0,ib,st[4]-ib;\n }\n if k==K_RULE'),
    ('sequence drops invalid runs on success',
     '  return R_OK,start,pos,nb,st[1]-nb,ib,st[4]-ib;',
     '  return R_OK,start,pos,nb,st[1]-nb,st[4],0;'),
    ('sequence keeps invalid runs on failure',
     '   if t==R_FAIL { return R_FAIL,start,e,st[1],0,st[4],0; }',
     '   if t==R_FAIL { return R_FAIL,start,e,st[1],0,ib,st[4]-ib; }'),
    ('the node loses its invalid runs',
     '  a_ic[nid]=rn;',
     '  a_ic[nid]=0;'),
    ('the rule result loses its invalid runs',
     '  return t,s,e,st[1]-1,1,ri,rn;',
     '  return t,s,e,st[1]-1,1,st[4],0;'),
    ('node invalid runs index the result arena instead',
     '   aninv[st[5]*2+0]=invp[(ri+z2)*2+0];',
     '   aninv[st[5]*2+0]=invp[ri*2+0];'),
    ('the schema parser does not pass through',
     ' if k==K_SCHEMA {\n  return parse_node(children[cfirst[id]],start,lenient,input,kind,cfirst,ccount,children,sidx,strs,sdir,dstart,dcount,delims,a_rule,a_tag,a_start,a_end,a_toff,a_tlen,a_cs,a_cc,a_partial,a_is,a_ic,achild,nres,invp,aninv,st);\n }',
     ' if k==K_SCHEMA {\n  return R_FAIL,start,start,st[1],0,st[4],0;\n }'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG485_PEGINV')]
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
