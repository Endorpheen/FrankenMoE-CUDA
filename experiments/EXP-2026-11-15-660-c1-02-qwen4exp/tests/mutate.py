#!/usr/bin/env python3
"""Mutation table for the C1-02 qwen4exp metadata/tensor/expert slices.

Each mutation is one semantic edit of its slice source. The mutation is CAUGHT
when that slice's gate stops reporting `failures=0`: either a case mismatches
or the build/run aborts.

Usage:
    tests/mutate.py q4m            # all 12
    tests/mutate.py q4m 0 6        # half 1
    tests/mutate.py q4m 6 12       # half 2
    tests/mutate.py q4e 0 12 --jobs 6

Every mutation runs in its own temporary tree, so parallel jobs never share a
generated C file or an object cache. The gate itself stays at -O1; mutation
builds run at -O0.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

S = Path(__file__).resolve().parent.parent
SRC_REL = {
    'q4m': Path('source') / 'q4m_metadata.goose',
    'q4t': Path('source') / 'q4t_tensor_map.goose',
    'q4e': Path('source') / 'q4e_expert_layout.goose',
}
ORIG = {k: (S / v).read_text() for k, v in SRC_REL.items()}

# ---- slice M: metadata / hparams rules -------------------------------------

M_HALF_1 = [
    ('expert_feed_forward_length becomes a required key',
     '    if k == K_EXPERT_FF || k == K_EXPERT_FF_SHEXP || k == K_COMPRESS_RATIOS {',
     '    if k == K_EXPERT_FF_SHEXP || k == K_COMPRESS_RATIOS {'),
    ('the PLE group is required even when ple.layers is absent',
     '        return R_PLE_GROUP;',
     '        return R_ALWAYS;'),
    ('rope.dimension_sections loses its fixed four-element arity',
     '''    if k == K_ROPE_SECTIONS {
        return A_FOUR;
    }''',
     '''    if k == K_ROPE_SECTIONS {
        return A_SCALAR;
    }'''),
    ('the n-gram upper bound excludes the legal maximum',
     '        if ngram < 2 || ngram > LLAMA_MAX_PLE_NGRAM {',
     '        if ngram < 2 || ngram >= LLAMA_MAX_PLE_NGRAM {'),
    ('the head-count upper bound excludes the legal maximum',
     '        if heads <= 0 || heads > LLAMA_MAX_PLE_HEADS {',
     '        if heads <= 0 || heads >= LLAMA_MAX_PLE_HEADS {'),
    ('a PLE layer exactly at n_layer_all is accepted',
     '        if ple_layer < 0 || ple_layer >= n_layer_all {',
     '        if ple_layer < 0 || ple_layer > n_layer_all {'),
]

M_HALF_2 = [
    ('a zero head vocabulary size passes the int32 range check',
     '        if v == 0 || o > INT32_MAX',
     '        if v == 1 || o > INT32_MAX'),
    ('the head end may exceed the int32 row index by one',
     '|| o + v > INT32_MAX {',
     '|| o + v > 2147483648 {'),
    ('a head ending exactly at the table end is rejected',
     '        if off[h] + vocab[h] > rows {',
     '        if off[h] + vocab[h] >= rows {'),
    ('conv history rows count the kernel instead of kernel-1',
     '    return (conv_kernel - 1) * ngram * hc * n_embd;',
     '    return conv_kernel * ngram * hc * n_embd;'),
    ('the recurrent pattern ignores the one-based layer index',
     '        if (il + 1) % interval != 0 {',
     '        if il % interval != 0 {'),
    ('the A3B type matches 49 layers instead of 48',
     '    if n_layer == 48 {',
     '    if n_layer == 49 {'),
]

# ---- slice T: tensor map ---------------------------------------------------

T_HALF_1 = [
    ('attn_output is spelled attn_out',
     '    if t == 15 { return "blk.%d.attn_output"; }',
     '    if t == 15 { return "blk.%d.attn_out"; }'),
    ('the hyper-connection head norm is named like the plain head norm',
     '    if t == 1  { return "output_hc_norm"; }',
     '    if t == 1  { return "hc_head_norm"; }'),
    ('the per-layer embedding table is treated as a layer tensor',
     '    if t <= 5 {\n        return 0;\n    }',
     '    if t <= 4 {\n        return 0;\n    }'),
    ('the per-layer embedding table moves to the output bucket',
     '    if t == 0 || t == 5 {\n        return B_INPUT;\n    }',
     '    if t == 0 {\n        return B_INPUT;\n    }'),
    ('ssm_dt is declared with a weight suffix instead of a bias',
     '    if t == 25 {\n        return SUF_BIAS;\n    }',
     '    if t == 25 {\n        return SUF_WEIGHT;\n    }'),
    ('the fused gate_up expert tensor is declared required',
     '    if t == 39 {\n        return F_NOT_REQUIRED;   // gate_up is optional: gate + up are the fallback\n    }',
     '    if t == 39 {\n        return F_REQUIRED;\n    }'),
]

T_HALF_2 = [
    ('the PLE table loses its lazy read flag',
     '    if t == 5 {\n        return F_READ_LAZY;      // its row count is only known after the read\n    }',
     '    if t == 5 {\n        return F_REQUIRED;\n    }'),
    ('the recurrent fused qkv tensor is not marked skip-if-virtual',
     '    if t == 14 || t == 22 {',
     '    if t == 14 {'),
    ('the MTP layer loses its attention and indexer tensors',
     '''        if kind == 1 {
            return 0; // recurrent layers own no attention and no indexer''',
     '''        if kind == 2 {
            return 0; // recurrent layers own no attention and no indexer'''),
    ('the PLE layer tensors are attached to attention layers',
     '    if t >= 31 && t <= 36 {\n        if kind == 1 {',
     '    if t >= 31 && t <= 36 {\n        if kind == 0 {'),
    ('the per-layer embedding table becomes a layer-kind tensor',
     '        if t == 5 {\n            return 0;\n        }',
     '        if t == 5 {\n            return 1;\n        }'),
    ('ssm_a gains a weight suffix the native call never passes',
     '    if t == 26 {\n        return SUF_NONE;\n    }',
     '    if t == 26 {\n        return SUF_WEIGHT;\n    }'),
]

# ---- slice E: expert and token-mixer layout --------------------------------

E_HALF_1 = [
    ('the routed expert width stops dividing by n_expert_used',
     '    return n_ff_exp(declared, n_ff, n_expert_used);',
     '    return n_ff_exp(declared, n_ff, 1);'),
    ('the shared expert width ignores its declared key',
     '    return n_ff_shexp(declared, n_ff);',
     '    return n_ff;'),
    ('the routed down tensor is declared square',
     '''    out[0] = n_ff_exp;
    out[1] = n_embd;
    out[2] = n_expert;''',
     '''    out[0] = n_ff_exp;
    out[1] = n_ff_exp;
    out[2] = n_expert;'''),
    ('the fused gate_up tensor loses the doubled row',
     '    out[1] = n_ff_exp * 2 as i64;',
     '    out[1] = n_ff_exp;'),
    ('the split gate tensor loses the expert dimension',
     '''    out[0] = n_embd;
    out[1] = n_ff_exp;
    out[2] = n_expert;''',
     '''    out[0] = n_embd;
    out[1] = n_ff_exp;
    out[2] = 1;'''),
    ('the shared down projection is not transposed',
     '''    } else if slot == 3 {
        out[0] = n_ff_shexp;
        out[1] = n_embd;''',
     '''    } else if slot == 3 {
        out[0] = n_embd;
        out[1] = n_ff_shexp;'''),
]

E_HALF_2 = [
    ('the query width drops the interleaved gate half',
     '    let q: i64 = q_full_width(head_k) * n_head;',
     '    let q: i64 = head_k * n_head;'),
    ('the key width uses the query head count',
     '    let k: i64 = head_k * n_head_kv;',
     '    let k: i64 = head_k * n_head;'),
    ('the value head width is reported as zero',
     '    out[Q4E_HEAD_V] = head_k;',
     '    out[Q4E_HEAD_V] = 0;'),
    ('the fused qkv row width drops the value block',
     '    return n_embd_q + n_embd_k + n_embd_v;',
     '    return n_embd_q + n_embd_k;'),
    ('the split routed layout reports two tensors',
     '    return 3; // gate + up + down',
     '    return 2; // gate + up + down'),
    ('the head-width assertion is inverted',
     '    if n_embd_head_v == n_embd_head_k {',
     '    if n_embd_head_v != n_embd_head_k {'),
]

MUT = {
    'q4m': M_HALF_1 + M_HALF_2,
    'q4t': T_HALF_1 + T_HALF_2,
    'q4e': E_HALF_1 + E_HALF_2,
}

G = os.environ.get('GOOSE_BIN', '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose')
CC = os.environ.get('CC', 'gcc-13')
CXX = os.environ.get('CXX', 'g++-13')


def build_and_run(slice_name: str, mutated: str, root: Path) -> tuple:
    """Materialise a mutated tree, build at -O0 and run that slice's gate."""
    (root / 'source' / 'base').mkdir(parents=True)
    (root / 'tests').mkdir()
    (root / SRC_REL[slice_name]).write_text(mutated)
    shutil.copy2(S / 'source' / 'base' / 'model_graph.goose',
                 root / 'source' / 'base' / 'model_graph.goose')
    fixture = 'fixture_%s.cpp' % slice_name
    shutil.copy2(S / 'tests' / fixture, root / 'tests' / fixture)
    shutil.copy2(S / 'tests' / 'build.sh', root / 'tests' / 'build.sh')
    env = dict(os.environ, OUT_DIR=str(root / 'build'), GOOSE_BIN=G, CC=CC, CXX=CXX)
    try:
        r = subprocess.run(['bash', str(root / 'tests' / 'build.sh'), slice_name, 'mutate'],
                           capture_output=True, text=True, timeout=600,
                           errors='replace', env=env)
    except subprocess.TimeoutExpired:
        return True, 'timeout'
    out = (r.stdout or '') + (r.stderr or '')
    if r.returncode != 0:
        m = re.search(r'failures=(\d+)', out)
        if m:
            return True, f'gate failures={m.group(1)}'
        tail = [x for x in out.strip().splitlines() if x.strip()]
        return True, 'abort' + (': ' + tail[-1][:100] if tail else '')
    return False, 'gate still failures=0'


def one(args) -> dict:
    slice_name, idx = args
    label, old, new = MUT[slice_name][idx]
    orig = ORIG[slice_name]
    if orig.count(old) != 1:
        return {'slice': slice_name, 'idx': idx, 'label': label,
                'status': 'ANCHOR', 'detail': f'anchor count {orig.count(old)}'}
    root = Path(tempfile.mkdtemp(prefix=f'mut-{slice_name}{idx:02d}-', dir=str(S / 'build-mut')))
    try:
        caught, detail = build_and_run(slice_name, orig.replace(old, new, 1), root)
    finally:
        shutil.rmtree(root, ignore_errors=True)
    return {'slice': slice_name, 'idx': idx, 'label': label,
            'status': 'CAUGHT' if caught else 'MISSED', 'detail': detail}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('slice', choices=sorted(MUT))
    ap.add_argument('lo', nargs='?', type=int, default=0)
    ap.add_argument('hi', nargs='?', type=int, default=None)
    ap.add_argument('--jobs', type=int, default=1)
    a = ap.parse_args()
    table = MUT[a.slice]
    hi = len(table) if a.hi is None else a.hi
    lo = a.lo
    (S / 'build-mut').mkdir(exist_ok=True)
    jobs = [(a.slice, i) for i in range(lo, hi)]
    rows = []
    if a.jobs > 1:
        with ThreadPoolExecutor(max_workers=a.jobs) as ex:
            rows = list(ex.map(one, jobs))
        for r in rows:
            print(f'[{r["status"]}] {r["idx"]} {r["label"]}: {r["detail"]}')
    else:
        for j in jobs:
            r = one(j)
            print(f'[{r["status"]}] {r["idx"]} {r["label"]}: {r["detail"]}', flush=True)
            rows.append(r)
    missed = [r['idx'] for r in rows if r['status'] != 'CAUGHT']
    caught = sum(1 for r in rows if r['status'] == 'CAUGHT')
    print(f'{a.slice} mutations {caught}/{hi - lo} caught (range {lo}:{hi}) missed={missed}')
    return 1 if missed else 0


if __name__ == '__main__':
    sys.exit(main())
