#!/usr/bin/env python3
# Mutation run for the tiny-MoE fixture plan (P10-09 S2).
#
# Each mutation edits one plan decision in source/fx-fixture-plan.goose, rebuilds
# the driver and re-compares with the native capture (build/native_fx.tsv, from
# the real generator). Any row mismatch, build failure or crash is a catch.
#
#   mutate_fx.py 0 6     # half 1: shapes, counts, seeds
#   mutate_fx.py 6 12    # half 2: metadata, tokenizer, guards, banner
import subprocess
import sys
from pathlib import Path

S = Path(__file__).resolve().parent.parent
SRC = S / 'source' / 'fx-fixture-plan.goose'
ORIG = SRC.read_text()

M = [
    # ---- half 1: tensor plan ----------------------------------------------
    ('the layer count changes',
     'const FX_N_LAYER: i64 = 4;',
     'const FX_N_LAYER: i64 = 5;'),
    ('the expert count changes',
     'const FX_N_EXPERT: i64 = 8;',
     'const FX_N_EXPERT: i64 = 4;'),
    ('the fused gate_up width is halved',
     '"ffn_gate_up_exps.weight", FX_N_EXPERT, 2 * FX_N_FF_EXP, FX_N_EMBD, s + 18',
     '"ffn_gate_up_exps.weight", FX_N_EXPERT, FX_N_FF_EXP, FX_N_EMBD, s + 18'),
    ('the grouped key/value weights are transposed',
     '"attn_k.weight", FX_N_EMBD_GQA, FX_N_EMBD, 0, s + 2',
     '"attn_k.weight", FX_N_EMBD, FX_N_EMBD_GQA, 0, s + 2'),
    ('the per-layer seed stride shrinks',
     'let s: i64 = 100 + 100 * layer;',
     'let s: i64 = 100 + 50 * layer;'),
    ('the attention width doubles',
     'const FX_N_EMBD_HEAD: i64 = 32;  // N_EMBD // N_HEAD',
     'const FX_N_EMBD_HEAD: i64 = 64;  // N_EMBD // N_HEAD'),
]
M += [
    # ---- half 2: metadata, tokenizer, guards, banner -----------------------
    ('the vocabulary hex becomes lower case',
     '        out[p] = (55 + hi) as u8;',
     '        out[p] = (87 + hi) as u8;'),
    ('the vocabulary loses a byte token',
     'const FX_VOCAB: i64 = 259;       // 3 specials + 256 byte tokens',
     'const FX_VOCAB: i64 = 258;       // 3 specials + 256 byte tokens'),
    ('the sliding-window pattern inverts',
     'p = fx_put_bool(out, p, if l != FX_FULL_ATTN_LAYER { 1 } else { 0 });',
     'p = fx_put_bool(out, p, if l == FX_FULL_ATTN_LAYER { 1 } else { 0 });'),
    ('the split writer stops asking for a small first shard',
     'export fn fx_small_first(split_max_tensors: i64) -> i64 {\n    if split_max_tensors != 0 {\n        return 1;\n    }',
     'export fn fx_small_first(split_max_tensors: i64) -> i64 {\n    if split_max_tensors != 0 {\n        return 0;\n    }'),
    ('the split guard accepts a gemma4 split',
     'if arch == FX_GEMMA && split_max_tensors != 0 {',
     'if arch < 0 && split_max_tensors != 0 {'),
    ('the banner miscounts the MoE layers',
     'p = fx_put_int(out, p, FX_N_LAYER - 1);',
     'p = fx_put_int(out, p, FX_N_LAYER);'),
    ('the byte tokens get the control type',
     'export fn fx_vocab_type(i: i64) -> i64 {\n    if i == 0 {\n        return 2;\n    }\n    if i < 3 {\n        return 3;\n    }\n    return 6;',
     'export fn fx_vocab_type(i: i64) -> i64 {\n    if i == 0 {\n        return 2;\n    }\n    if i < 3 {\n        return 3;\n    }\n    return 3;'),
]


def main():
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    caught = 0
    for idx in range(lo, min(hi, len(M))):
        name, find, repl = M[idx]
        if ORIG.count(find) != 1:
            print('%-3d SETUP  %-52s (pattern occurs %d times)' % (idx, name, ORIG.count(find)))
            continue
        SRC.write_text(ORIG.replace(find, repl))
        try:
            r = subprocess.run(['python3', str(S / 'tests' / 'gen_fx.py')],
                               capture_output=True, text=True, timeout=300)
            ok = r.returncode != 0
        finally:
            SRC.write_text(ORIG)
        if ok:
            caught += 1
            first = [l for l in (r.stdout + r.stderr).splitlines() if l.startswith('FAIL')]
            print('%-3d caught %-52s %s' % (idx, name, first[0].strip()[:60] if first else ''))
        else:
            print('%-3d MISSED %-52s' % (idx, name))
    total = min(hi, len(M)) - lo
    print('caught %d/%d' % (caught, total))
    return 0 if caught == total else 1


if __name__ == '__main__':
    sys.exit(main())
