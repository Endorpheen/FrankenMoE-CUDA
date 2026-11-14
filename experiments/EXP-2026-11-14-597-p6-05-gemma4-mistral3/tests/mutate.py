#!/usr/bin/env python3
"""Mutation table for the three Gemma 4 / Mistral 3 plan/shape slices
(P6-05 architectures 18, 19, 20).

Each mutation is a single semantic edit of one family's source file:

  gemma4 (0-3):            SWA head width, declared expert width, shared-KV
                           boundary, rope_freqs layer set;
  gemma4-assistant (4-7):  nextn_proj_pre width, out_scale requirement,
                           draft SWA head width, nextn_proj_pre layer bound;
  mistral3 (8-11):         routed expert width, fused QKV width, shared-expert
                           width, longrope tensor group.

A mutation is CAUGHT when its family's structure gate stops reporting
`failures=0`: either a case mismatches, or the build/run aborts.

Usage:
    tests/mutate.py            # both halves, sequential
    tests/mutate.py 0 6        # half 1
    tests/mutate.py 6 12       # half 2
    tests/mutate.py --jobs 4

Every mutation runs in its own temporary tree and builds only the affected
family, so parallel jobs never share a generated C file or an object cache.
The gate itself stays at -O1; mutation builds run at -O0.
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
FAMILIES = {
    'gemma4': ('source/gemma4.goose', 'fixture_gemma4.cpp', 'gemma4'),
    'gemma4-assistant': ('source/gemma4-assistant.goose', 'fixture_g4assistant.cpp', 'gemma4-assistant'),
    'mistral3': ('source/mistral3.goose', 'fixture_mistral3.cpp', 'mistral3'),
}

# (family, label, anchor, replacement) -- anchors must occur exactly once.
M = [
    # --- gemma4 (architecture 18) ---
    ('gemma4',
     'SWA layers keep the full-attention head width',
     '        return p[P_HEAD_K_SWA];',
     '        return p[P_HEAD_K];'),
    ('gemma4',
     'expert width derived from n_ff/n_expert_used instead of the declared value',
     '    let n_ff_exp_v = p[P_N_FF_EXP];',
     '    let n_ff_exp_v = n_ff / p[P_N_EXPERT_USED];'),
    ('gemma4',
     'shared-KV boundary includes one extra layer',
     '    if il < n_from_start {',
     '    if il <= n_from_start {'),
    ('gemma4',
     'rope_freqs placed on the SWA layers instead of the full ones',
     '        if g4_is_swa(p, il, swa_base) != 0 {',
     '        if g4_is_swa(p, il, swa_base) == 0 {'),

    # --- gemma4-assistant (architecture 19) ---
    ('gemma4-assistant',
     'nextn_proj_pre loses the concatenated target width',
     '        e_put3(out, 2 * n_backbone, n_embd, 1);',
     '        e_put3(out, n_backbone, n_embd, 1);'),
    ('gemma4-assistant',
     'out_scale reported optional although the loader requires it',
     'export fn g4a_out_scale_required() -> i64 {\n    return 1;',
     'export fn g4a_out_scale_required() -> i64 {\n    return 0;'),
    ('gemma4-assistant',
     'draft SWA layers keep the full-attention head width',
     '        return p[P_HEAD_K_SWA];',
     '        return p[P_HEAD_K];'),
    ('gemma4-assistant',
     'nextn_proj_pre declared on every draft layer',
     '        if il != 0 {',
     '        if il < 0 {'),

    # --- mistral3 (architecture 20) ---
    ('mistral3',
     'routed expert width divided by n_expert_used',
     '    if tid == T_FFN_GATE_EXPS || tid == T_FFN_UP_EXPS {\n'
     '        e_put3(out, n_embd, n_ff, n_expert);',
     '    if tid == T_FFN_GATE_EXPS || tid == T_FFN_UP_EXPS {\n'
     '        e_put3(out, n_embd, n_ff / p[P_N_EXPERT_USED], n_expert);'),
    ('mistral3',
     'fused attention QKV drops the value width',
     '    let qkv_fused = q_width + n_embd_k_gqa + n_embd_v_gqa;',
     '    let qkv_fused = q_width + n_embd_k_gqa;'),
    ('mistral3',
     'shared-expert width collapses to the routed width',
     '            e_put3(out, n_embd, n_shexp, 1);',
     '            e_put3(out, n_embd, n_ff, 1);'),
    ('mistral3',
     'longrope files still declare the single rope_freqs tensor',
     '        if m3_rope_tensor_kind(p) == 0 {',
     '        if m3_rope_tensor_kind(p) >= 0 {'),
]

G = os.environ.get('GOOSE_BIN', '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose')
CC = os.environ.get('CC', 'gcc-13')
CXX = os.environ.get('CXX', 'g++-13')


def build_and_run(mutated: str, root: Path, family: str) -> tuple:
    """Materialise a mutated tree, build one family at -O0 and run its gate."""
    src_rel, fixture, arg = FAMILIES[family]
    (root / 'source' / 'base').mkdir(parents=True)
    (root / 'tests').mkdir()
    (root / src_rel).write_text(mutated)
    shutil.copy2(S / 'source' / 'base' / 'model_graph.goose', root / 'source' / 'base' / 'model_graph.goose')
    shutil.copy2(S / 'tests' / fixture, root / 'tests' / fixture)
    shutil.copy2(S / 'tests' / 'gate_common.h', root / 'tests' / 'gate_common.h')
    shutil.copy2(S / 'tests' / 'build.sh', root / 'tests' / 'build.sh')
    env = dict(os.environ, OPT='-O0', OUT_DIR=str(root / 'build'), GOOSE_BIN=G, CC=CC, CXX=CXX)
    try:
        r = subprocess.run(['bash', str(root / 'tests' / 'build.sh'), arg],
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


def one(idx: int) -> dict:
    family, label, old, new = M[idx]
    src_rel, _, _ = FAMILIES[family]
    orig = (S / src_rel).read_text()
    if orig.count(old) != 1:
        return {'idx': idx, 'label': label, 'status': 'ANCHOR',
                'detail': f'anchor count {orig.count(old)}'}
    root = Path(tempfile.mkdtemp(prefix=f'mut{idx:02d}-', dir=str(S / 'build-mut')))
    try:
        caught, detail = build_and_run(orig.replace(old, new, 1), root, family)
    finally:
        shutil.rmtree(root, ignore_errors=True)
    return {'idx': idx, 'label': label, 'status': 'CAUGHT' if caught else 'MISSED',
            'family': family, 'detail': detail}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('lo', nargs='?', type=int, default=0)
    ap.add_argument('hi', nargs='?', type=int, default=len(M))
    ap.add_argument('--jobs', type=int, default=1)
    a = ap.parse_args()
    lo, hi = a.lo, a.hi
    (S / 'build-mut').mkdir(exist_ok=True)
    rows = []
    if a.jobs > 1:
        with ThreadPoolExecutor(max_workers=a.jobs) as ex:
            rows = list(ex.map(one, range(lo, hi)))
        for r in rows:
            print(f'[{r["status"]}] {r["idx"]} {r["family"]} {r["label"]}: {r["detail"]}')
    else:
        for i in range(lo, hi):
            r = one(i)
            print(f'[{r["status"]}] {r["idx"]} {r["family"]} {r["label"]}: {r["detail"]}', flush=True)
            rows.append(r)
    missed = [r['idx'] for r in rows if r['status'] != 'CAUGHT']
    caught = sum(1 for r in rows if r['status'] == 'CAUGHT')
    print(f'mutations {caught}/{hi - lo} caught (range {lo}:{hi}) missed={missed}')
    return 1 if missed else 0


if __name__ == '__main__':
    sys.exit(main())
