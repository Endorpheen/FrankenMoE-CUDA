#!/usr/bin/env python3
from pathlib import Path
import subprocess, sys
ROOT=Path('/home/end0/AI/FrankenMoE-CUDA/experiments/EXP-2026-11-14-569-p7-08-autoparser'); SRC=ROOT/'source/runtime.goose'; ORIG=SRC.read_text()
M=[
 ('hoist','pid offset shifted','    // pid column, preparatory: Until never uses mn, so it carries the native\n    // insertion-index candidate.\n    mn[id] = id;','    // pid column, preparatory: Until never uses mn, so it carries the native\n    // insertion-index candidate.\n    mn[id] = id + 1;'),
 ('hoist','state padding threshold changed','        p = gb_w(dst, p, "-");\n        if q < 10 {\n            p = gb_w(dst, p, "0");','        p = gb_w(dst, p, "-");\n        if q < 9 {\n            p = gb_w(dst, p, "0");'),
 ('hoist','completing class widened','bp = gb_class(body, bp, pat, q, 0 - 1, fail, 0);','bp = gb_class(body, bp, pat, q, 0, fail, 0);'),
 ('hoist','start-state transition dropped','    if pat.len > 0 && pat[0] == c {\n        return 1;\n    }','    if pat.len > 0 && pat[0] == c {\n        return 0;\n    }'),
 ('hoist','until prefix renamed','    if is_ac != 0 {\n        p = gb_w(dst, p, "ac-");\n    } else {\n        p = gb_w(dst, p, "until-");','    if is_ac != 0 {\n        p = gb_w(dst, p, "ac-");\n    } else {\n        p = gb_w(dst, p, "untill-");'),
 ('hoist','base state rule skipped','    var q: i64 = 0;\n\n    while q < pat.len {','    var q: i64 = 1;\n\n    while q < pat.len {'),
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
            script='build_gate8.sh' if half=='hoist' else 'build_gate6.sh'
            marker='FG_HOIST cases=4 failures=0' if half=='hoist' else 'FG_TOKENS cases=6 failures=0'
            try:
                r=subprocess.run(['bash',str(ROOT/script)],text=True,capture_output=True,timeout=240,errors='replace')
            except subprocess.TimeoutExpired:
                rows.append((i,half,name,'timeout')); caught+=1; continue
            ok=(marker in r.stdout and r.returncode==0)
            rows.append((i,half,name,'NOT CAUGHT' if ok else 'caught'))
            if not ok: caught+=1
    finally:
        SRC.write_text(ORIG)
    for r in rows: print(f'{r[0]:2d} {r[1]:6s} {r[2]:38s} {r[3]}')
    print(f'FG_MUT8 caught={caught}/{len(M)}')
    return 0 if caught==len(M) else 1
sys.exit(run())
