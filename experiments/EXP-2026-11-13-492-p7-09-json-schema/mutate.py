#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-492-p7-09-json-schema'
SRC = E / 'source/schema.goose'
ORIG = SRC.read_text()

M = [
    ('the root path is empty', 'let ro_o,ro_l=p_lit(pb,st,0,0,"#");', 'let ro_o,ro_l=p_lit(pb,st,0,0,"");'),
    ('a double counts as an integer', 'if jk[v]==J_INT { return jv[v]; }', 'if jk[v]==J_INT || jk[v]==J_DBL { return jv[v]; }'),
    ('a negative count is accepted', 'if jk[v]!=J_INT || jv[v]<0 {', 'if jk[v]!=J_INT {'),
    ('minimum rounds down', 'if round_up!=0 { return ceil(d) as i64; }', 'if round_up!=0 { return floor(d) as i64; }'),
    ('uuid5 is not a uuid', "if s[0]=='u' && s[1]=='u' && s[2]=='i' && s[3]=='d' && s[4]>='1' && s[4]<='5' { return 1; }",
     "if s[0]=='u' && s[1]=='u' && s[2]=='i' && s[3]=='d' && s[4]>='1' && s[4]<='4' { return 1; }"),
    ('oneOf is looked up as anyOf', 'if jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"oneOf") {', 'if jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"anyOf") {'),
    ('the type override is ignored', 'var typej:i64=tover;', 'var typej:i64=-1;'),
    ('the const key is read as enum', ' let cj=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"const");', ' let cj=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"enum");'),
    ('an empty enum is accepted', 'if jk[ej]!=J_ARR || jn[ej]==0 {', 'if jk[ej]!=J_ARR {'),
    ('additionalProperties does not imply a property set', '   if !(jk[ap]==J_BOOL && jv[ap]!=0) { hasp=true; }', '   if false { hasp=true; }'),
    ('required names are ignored', '   if s_eq(strs,sdir,jv[rn],nm) { isreq=1; r=req_n; } else { r=r+1; }', '   if false { isreq=1; r=req_n; } else { r=r+1; }'),
    ('additionalProperties true adds no node', '    nadd[id]=anyid;\n   }\n  } else if jk[ap]==J_OBJ {', '   }\n  } else if jk[ap]==J_OBJ {'),
    ('a bare object gets no implicit any', ' } else if !jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"properties") {', ' } else if false {'),
    ('prefixItems wins over items', ' let has_items=jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"items");', ' let has_items=!jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"prefixItems");'),
    ('array item bounds are swapped', '  nminl[id]=get_count(jk,jv,jf,jn,jkey,jchild,strs,sdir,ji,"minItems",pb,mb,err,st,poff,plen,0);', '  nminl[id]=get_count(jk,jv,jf,jn,jkey,jchild,strs,sdir,ji,"maxItems",pb,mb,err,st,poff,plen,0);'),
    ('the pattern is dropped', '  if sdir[2*jv[pj]+1]==0 { nstr[id]=-1; } else { nstr[id]=jv[pj]; }', '  nstr[id]=-1;'),
    ('the format is dropped', ' nfmt[id]=fmt;\n nminl[id]=get_count(jk,jv,jf,jn,jkey,jchild,strs,sdir,ji,"minLength"', ' nminl[id]=get_count(jk,jv,jf,jn,jkey,jchild,strs,sdir,ji,"minLength"'),
    ('exclusiveMinimum is not shifted', '  nmin[id]=get_bound(jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,ji,"exclusiveMinimum",0,pb,mb,err,st,poff,plen)+1;',
     '  nmin[id]=get_bound(jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,ji,"exclusiveMinimum",0,pb,mb,err,st,poff,plen);'),
    ('exclusiveMaximum is not shifted', '  nmax[id]=get_bound(jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,ji,"exclusiveMaximum",1,pb,mb,err,st,poff,plen)-1;',
     '  nmax[id]=get_bound(jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,ji,"exclusiveMaximum",1,pb,mb,err,st,poff,plen);'),
    ('refs are rebuilt every time', '  if refs_find(rk,st[2],refi)<0 {', '  if true {'),
    ('pending refs keep no target', '  if fi>=0 { ntarget[nid]=rv[fi]; }', '  if fi<-99 { ntarget[nid]=rv[fi]; }'),
    ('an empty type array is accepted', '   let m: const u8[:] = "type must not be empty";\n   return fail(pb,err,st,poff,plen,m);', '   let m: const u8[:] = "type must not be empty";'),
    ('an unknown type becomes any', ' if s_eq(strs,sdir,tsid,"null") { nkind[id]=K_NULL; return id; }', ' if s_eq(strs,sdir,tsid,"null") { nkind[id]=K_NULL; return id; }\n nkind[id]=K_ANY;\n return id;'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=120, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG492_SCHEMA')]
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
