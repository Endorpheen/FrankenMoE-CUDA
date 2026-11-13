#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-493-p7-09-json-parser'
SRC = E / 'source/jsonparse.goose'
ORIG = SRC.read_text()

M = [
    ('a leading zero is accepted', " if first=='0' {\n  p=p+1;", " if first=='0' && false {\n  p=p+1;"),
    ('an exponent needs no digit', "  if p>=text.len || !is_dig(text[p]) { return err_cons(text,st,E_PARSE,p+1); }\n  while p<text.len && is_dig(text[p]) { p=p+1; }\n  isflt=true;\n }\n st[4]=p;",
     "  isflt=true;\n }\n st[4]=p;"),
    ('a fraction needs no digit', "  if p>=text.len || !is_dig(text[p]) { return err_cons(text,st,E_PARSE,p+1); }\n  while p<text.len && is_dig(text[p]) { p=p+1; }\n  isflt=true;\n }\n if p<text.len && (text[p]=='e' || text[p]=='E') {",
     "  while p<text.len && is_dig(text[p]) { p=p+1; }\n  isflt=true;\n }\n if p<text.len && (text[p]=='e' || text[p]=='E') {"),
    ('raw control bytes are accepted', "  } else if c<0x20 {", "  } else if c<0x20 && false {"),
    ('an unknown escape is accepted', "   } else {\n    return err_cons(text,st,E_PARSE,p+1),0;\n   }", "   } else {\n    st[10]=appc(sbuf,st[10],e); p=p+1;\n   }"),
    ('a lone high surrogate is accepted', "     } else if p>=text.len {\n      return err_eof(text,st,E_PARSE),0;\n     } else {\n      return err_cons(text,st,E_PARSE,p+1),0;\n     }",
     "     } else if p>=text.len {\n      st[10]=put_utf8(sbuf,st[10],cp);\n     } else {\n      st[10]=put_utf8(sbuf,st[10],cp);\n     }"),
    ('a lone low surrogate is accepted', "    } else if cp>=0xDC00 && cp<=0xDFFF {\n     return err_cons(text,st,E_PARSE,p),0;",
     "    } else if cp>=0xDC00 && cp<=0xDFFF {\n     st[10]=put_utf8(sbuf,st[10],cp);"),
    ('the surrogate pair is combined wrongly', "      let full=65536+(cp-0xD800)*1024+(lo-0xDC00);", "      let full=65536+(cp-0xD800)*1024+lo;"),
    ('ill-formed UTF-8 is copied', "   if uk!=0 {\n    return err_cons(text,st,E_PARSE,p+ub),0;\n   }", "   if uk!=99 {\n    return err_cons(text,st,E_PARSE,p+ub),0;\n   }"),
    ('duplicate keys are appended', "  if e_klen[e]==key.len {", "  if false {"),
    ('a uint above the range becomes an integer', "   jk[id]=K_UINT; jv[id]=mag as i64;\n   return id;", "   jk[id]=K_INT; jv[id]=mag as i64;\n   return id;"),
    ('the unsigned boundary is one off', '   if mag<=9223372036854775807 {', '   if mag<9223372036854775807 {'),
    ('the sign of a negative number is dropped', "     jk[id]=K_INT; jv[id]=0-(mag as i64);", "     jk[id]=K_INT; jv[id]=mag as i64;"),
    ('trailing garbage is accepted', " if st[4]<text.len {\n  err_next(text,st,E_PARSE);\n  return -1;\n }", " if false {\n  err_next(text,st,E_PARSE);\n  return -1;\n }"),
    ('a literal is not validated', " if !lit_rest(text,st,\"rue\") { return -1; }", " if !true { return -1; }"),
    ('number overflow is stored as a float', " if is_inf(d) || is_nan(d) {\n  return err_cons(text,st,E_RANGE,st[4]);\n }", " if false {\n  return err_cons(text,st,E_RANGE,st[4]);\n }"),
    ('booleans are inverted', "  jk[id]=K_BOOL; jv[id]=1;\n  st[4]=p+1;", "  jk[id]=K_BOOL; jv[id]=0;\n  st[4]=p+1;"),
    ('vertical tab counts as whitespace', " if c==(32 as u8) || c==(9 as u8) || c==(10 as u8) || c==(13 as u8) { return true; }",
     " if c==(32 as u8) || c==(9 as u8) || c==(10 as u8) || c==(13 as u8) || c==(11 as u8) { return true; }"),
    ('the error column is one off', " st[3]=consumed-ls;", " st[3]=consumed-ls+1;"),
    ('the child list is not flipped back', '    let nx=e_next[e];\n    e_next[e]=prev;', '    let nx=e_next[e];'),
    ('the object separator error reports the current byte', "  else { skip_token(text,jk,jv,jd,js,jl,jf,jn,jh,e_val,e_koff,e_klen,e_next,sbuf,st); return err_cons(text,st,E_PARSE,st[4]); }",
     "  else { return err_next(text,st,E_PARSE); }"),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG493_JSON')]
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
