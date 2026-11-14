#!/usr/bin/env python3
# Six tools-grammar mutations (gate7) and six token-collector mutations (gate6).
from pathlib import Path
import subprocess, sys
ROOT=Path('/tmp/opencode/sub-569'); SRC=ROOT/'base/runtime.goose'; ORIG=SRC.read_text()
M=[
 ('grammar','literal loses opening quote','    var p = gb_w(dst, at, "\\\"");','    var p = at;'),
 ('grammar','literal loses closing quote','    return gb_w(dst, p, "\\\"");','    return p;'),
 ('grammar','sequence parentheses disabled','    if kind[id] == PEG_SEQ || kind[id] == PEG_CHOICE {\n        return 1;\n    }','    if kind[id] == PEG_SEQ || kind[id] == PEG_CHOICE {\n        return 0;\n    }'),
 ('grammar','optional becomes star','        return gb_w(dst, p, "?");','        return gb_w(dst, p, "*");'),
 ('grammar','root rule renamed','    let rnm: u8[4] = [114, 111, 111, 116];','    let rnm: u8[4] = [114, 111, 111, 120];'),
 ('grammar','space rule body changed','    let spbody = "| \\" \\" | \\"\\\\n\\"{1,2} [ \\\\t]{0,20}";','    let spbody = "| \\" \\" | \\"\\\\n\\"{1,3} [ \\\\t]{0,20}";'),
 ('tokens','trim disabled','    let t = ap_trim(tok);','    let t = tok;'),
 ('tokens','dedupe disabled','    while i < count {\n        if len[i] == t.len {','    while i < 0 {\n        if len[i] == t.len {'),
 ('tokens','empty tokens kept','    if t.len == 0 {\n        return count;\n    }','    if t.len < 0 {\n        return count;\n    }'),
 ('tokens','first two tokens swapped','    n = ap_add_token(rstart, out, off, len, n);\n    n = ap_add_token(rend, out, off, len, n);','    n = ap_add_token(rend, out, off, len, n);\n    n = ap_add_token(rstart, out, off, len, n);'),
 ('tokens','call-id suffix dropped','    n = ap_add_token(cis, out, off, len, n);','    n = ap_add_token(cstart, out, off, len, n);'),
 ('tokens','token offset shifted','    off[count] = at;','    off[count] = at + 1;'),
]
def run():
    caught=0; rows=[]
    try:
        for i,(half,name,old,new) in enumerate(M,1):
            if ORIG.count(old)!=1:
                rows.append((i,half,name,f'COUNT {ORIG.count(old)}')); continue
            SRC.write_text(ORIG.replace(old,new,1))
            script='build_gate7.sh' if half=='grammar' else 'build_gate6.sh'
            marker='FG_TOOLS cases=8 failures=0' if half=='grammar' else 'FG_TOKENS cases=6 failures=0'
            try:
                r=subprocess.run(['bash',str(ROOT/script)],text=True,capture_output=True,timeout=240,errors='replace')
            except subprocess.TimeoutExpired:
                rows.append((i,half,name,'timeout')); caught+=1; continue
            ok=(marker in r.stdout and r.returncode==0)
            rows.append((i,half,name,'NOT CAUGHT' if ok else 'caught'))
            if not ok: caught+=1
    finally:
        SRC.write_text(ORIG)
    for r in rows: print(f'{r[0]:2d} {r[1]:7s} {r[2]:38s} {r[3]}')
    print(f'FG_MUT7 caught={caught}/{len(M)}')
    return 0 if caught==len(M) else 1
sys.exit(run())
