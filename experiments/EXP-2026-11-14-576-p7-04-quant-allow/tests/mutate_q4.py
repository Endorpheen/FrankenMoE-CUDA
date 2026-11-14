#!/usr/bin/env python3
# Mutation run for the quantization allow-cuts slice (P7-04 S2).
# Each mutation edits the Goose port, regenerates the gate driver, runs it with
# `goose run` and compares with the native oracle. Any diff, run failure or
# missing output counts as caught. Usage: mutate_q4.py [lo hi]
from pathlib import Path
import subprocess
import sys

S = Path(__file__).resolve().parent.parent
SRC = S / 'source' / 'quant-allow.goose'
ORIG = SRC.read_text()
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'

M = [
    # ---- half 1: allow-cuts ------------------------------------------------
    ('norm tensors become quantizable', 'if q_find(name, "_norm.weight") >= 0 {', 'if q_find(name, "_norm.weightx") >= 0 {'),
    ('the output gate is inverted', 'if quantize_output == 0 && q_eq(name, "output.weight") {',
     'if quantize_output != 0 && q_eq(name, "output.weight") {'),
    ('expert gating tensors become quantizable', 'if q_find(name, "ffn_gate_inp.weight") >= 0 {',
     'if q_find(name, "ffn_gate_inpx.weight") >= 0 {'),
    ('the routing table becomes quantizable', 'if q_find(name, "ffn_gate_tid2eid.weight") >= 0 {',
     'if q_find(name, "ffn_gate_tid2eid.weightx") >= 0 {'),
    ('the conv1d cut is dropped', 'if q_find(name, "ssm_conv1d") >= 0 {', 'if q_find(name, "ssm_conv1dx") >= 0 {'),
    ('position and token-type tensors are allowed',
     'if q_eq(name, pos_embd) || q_eq(name, token_types) {',
     'if q_eq(name, "position_embdx") || q_eq(name, token_types) {'),
    ('the GLM5_NEXT block is keyed on the wrong bit', 'if (arch_flags & 1) != 0 {', 'if (arch_flags & 4) != 0 {'),
    ('the decision head is allowed', 'if q_starts(name, "decision.") {', 'if q_starts(name, "decisionx.") {'),
    # ---- half 2: matcher and priority --------------------------------------
    ('dot matches a line terminator', 'return c != 10 && c != 13;', 'return true;'),
    ('class negation is ignored', '    if neg {\n        return !hit;\n    }', '    if neg {\n        return hit;\n    }'),
    ('the repetition minimum is ignored', '                mn = mnb[0];', '                mn = 0;'),
    ('plus allows zero repetitions', '            } else if q == 43 {\n                mn = 1;',
     '            } else if q == 43 {\n                mn = 0;'),
    ('the end anchor is ignored', '''        if p[i] == 36 {
            if si == s.len {
                return 1;
            }
            return 0;
        }''', '''        if p[i] == 36 {
            return 1;
        }'''),
    ('the optional quantifier becomes mandatory-zero', '''            } else if q == 63 {
                mn = 0;
                mx = 1;''', '''            } else if q == 63 {
                mn = 0;
                mx = 0;'''),
    ('the override reports its own index', '            out_type[0] = ty[i];\n            return i;',
     '            out_type[0] = ty[0];\n            return i;'),
    ('the token-embedding branch is inverted',
     '        if !named {\n            out_type[0] = token_embd_type;\n            return 1;\n        }',
     '        if named {\n            out_type[0] = token_embd_type;\n            return 1;\n        }'),
]


def run():
    subprocess.run(['python3', str(S / 'tests' / 'gen_q4.py')],
                   capture_output=True, text=True, timeout=120)
    try:
        r = subprocess.run([G, 'run', str(S / 'build' / 'gate_q4.goose')],
                           capture_output=True, text=True, timeout=280, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'run timeout'
    if r.returncode != 0:
        tail = (r.stderr or r.stdout).strip().splitlines()
        return 1, 'run failed: ' + (tail[-1][:120] if tail else 'no output')
    got = r.stdout
    want = (S / 'build' / 'native_q4.out').read_text()
    if got == want:
        return 0, 'identical'
    g = got.splitlines()
    w = want.splitlines()
    diff = sum(1 for a, b in zip(g, w) if a != b) + abs(len(g) - len(w))
    return 1, f'{diff} differing lines'


lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
caught = 0
missed = []
try:
    for idx, (label, old, new) in enumerate(M[lo:hi], start=lo):
        if ORIG.count(old) != 1:
            print(f'[skip] {idx} {label}: anchor count {ORIG.count(old)}')
            continue
        SRC.write_text(ORIG.replace(old, new, 1))
        rc, detail = run()
        ok = rc != 0
        caught += ok
        if not ok:
            missed.append(idx)
        print(f'[{"caught" if ok else "MISSED"}] {idx} {label}: {detail}')
        SRC.write_text(ORIG)
finally:
    SRC.write_text(ORIG)
    rc, detail = run()
    print(f'baseline after restore: rc={rc} {detail}')
print(f'mutations {caught}/{hi-lo} caught (range {lo}:{hi}) missed={missed}')
