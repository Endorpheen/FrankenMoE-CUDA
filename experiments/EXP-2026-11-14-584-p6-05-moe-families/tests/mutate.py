#!/usr/bin/env python3
"""Mutation table for the three MoE plan/shape slices (P6-05 architectures 4, 7, 13).

Each mutation is a single semantic edit of one family's source file:

  qwen3moe (0-3):   fused QKV width, routed-expert width, wo width, down_exps
                    orientation;
  qwen2moe (4-7):   fused QKV width, routed-expert width, shared-expert width,
                    wo width;
  glm4-moe (8-11):  dense-lead boundary, shared-expert multiplicity, nextn
                    eh_proj doubled width, routed-expert width fallback.

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
    'qwen3moe': ('source/qwen3moe.goose', 'fixture_qwen3moe.cpp', 'qwen3moe'),
    'qwen2moe': ('source/qwen2moe.goose', 'fixture_qwen2moe.cpp', 'qwen2moe'),
    'glm4-moe': ('source/glm4-moe.goose', 'fixture_glm4moe.cpp', 'glm4-moe'),
}

# (family, label, anchor, replacement) -- anchors must occur exactly once.
M = [
    # --- qwen3moe (architecture 4) ---
    ('qwen3moe',
     'fused attention QKV drops the value width',
     'let qkv_fused = q_width + n_embd_k_gqa + n_embd_v_gqa;',
     'let qkv_fused = q_width + n_embd_k_gqa;'),
    ('qwen3moe',
     'routed expert width ignores n_expert_used',
     'let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, used);',
     'let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, 1);'),
    ('qwen3moe',
     'attention output uses n_embd instead of the query width',
     'e_put3(out, head_k * n_head, n_embd, 1);',
     'e_put3(out, n_embd, n_embd, 1);'),
    ('qwen3moe',
     'routed down projection is written transposed',
     'e_put3(out, n_ff_exp_v, n_embd, n_expert);',
     'e_put3(out, n_embd, n_ff_exp_v, n_expert);'),

    # --- qwen2moe (architecture 7) ---
    ('qwen2moe',
     'fused attention QKV drops the value width',
     'let qkv_fused = q_width + n_embd_k_gqa + n_embd_v_gqa;',
     'let qkv_fused = q_width + n_embd_k_gqa;'),
    ('qwen2moe',
     'routed expert width ignores n_expert_used',
     'let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, used);',
     'let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, 1);'),
    ('qwen2moe',
     'shared-expert width collapses to the routed width',
     'let n_ff_shexp_v = n_ff_shexp(p[P_N_FF_SHEXP], n_ff);',
     'let n_ff_shexp_v = n_ff_exp_v;'),
    ('qwen2moe',
     'attention output uses the GQA width instead of n_embd',
     'e_put3(out, n_embd, n_embd, 1);',
     'e_put3(out, n_embd_k_gqa, n_embd, 1);'),

    # --- glm4-moe (architecture 13) ---
    ('glm4-moe',
     'dense lead boundary includes one extra layer',
     'if il < p[P_DENSE_LEAD] {',
     'if il <= p[P_DENSE_LEAD] {'),
    ('glm4-moe',
     'shared-expert width ignores n_expert_shared',
     'let n_ff_shexp_v = n_ff_exp_v * n_shared;',
     'let n_ff_shexp_v = n_ff_exp_v;'),
    ('glm4-moe',
     'nextn eh_proj loses the doubled hidden state',
     'e_put3(out, 2 * n_embd, n_embd, 1);',
     'e_put3(out, n_embd, n_embd, 1);'),
    ('glm4-moe',
     'routed expert width ignores n_expert_used',
     'let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, used);',
     'let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, 1);'),
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
