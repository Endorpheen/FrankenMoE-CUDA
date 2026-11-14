#!/usr/bin/env python3
"""Mutation table for the deepseek32 plan/shape slice (P6-05 architecture 8).

Each mutation is a single semantic edit of source/deepseek32.goose: the
dense-lead/MTP layer plan, the MLA compression shapes, the indexer decisions,
the expert widths and the nextn head. A mutation is CAUGHT when the structure
gate stops reporting `failures=0`: either a case mismatches, or the build/run
aborts.

Usage:
    tests/mutate.py            # both halves, sequential
    tests/mutate.py 0 6        # half 1
    tests/mutate.py 6 12       # half 2
    tests/mutate.py --jobs 4

Every mutation runs in its own temporary tree so parallel jobs never share a
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
SRC_REL = Path('source') / 'deepseek32.goose'
ORIG = (S / SRC_REL).read_text()

# (label, anchor, replacement) -- anchors must occur exactly once.
HALF_1 = [
    ('dense-lead boundary includes one MoE layer',
     '''    if il < p[P_DENSE_LEAD] {
        ffn_dense = 1;
    }''',
     '''    if il <= p[P_DENSE_LEAD] {
        ffn_dense = 1;
    }'''),
    ('MTP layer is no longer planned past n_layer',
     '    if il < p[P_N_LAYER_ALL] {',
     '    if il < p[P_N_LAYER] {'),
    ('wq_b drops the head count',
     '''    if tid == T_WQ_B {
        e_put3(out, q_lora, n_head * head_k, 1);''',
     '''    if tid == T_WQ_B {
        e_put3(out, q_lora, head_k, 1);'''),
    ('wkv_a_mqa loses the rope part of the compressed key',
     '        e_put3(out, n_embd, kv_lora + rope, 1);',
     '        e_put3(out, n_embd, kv_lora, 1);'),
    ('wk_b swaps the nope and compressed-key axes',
     '        e_put3(out, nope, kv_lora, n_head);',
     '        e_put3(out, kv_lora, nope, n_head);'),
    ('indexer q projection ignores its head count',
     '        e_put3(out, q_lora, idx_nhead * idx_head, 1);',
     '        e_put3(out, q_lora, idx_head, 1);'),
]

HALF_2 = [
    ('shared expert width ignores n_expert_shared',
     '    let shexp_w = n_ff_exp_v * p[P_N_EXPERT_SHARED];',
     '    let shexp_w = n_ff_exp_v;'),
    ('routed expert width falls back to n_ff',
     '    let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, used);',
     '    let n_ff_exp_v = n_ff;'),
    ('MTP eh_proj loses the doubled hidden state',
     '            e_put3(out, n_embd * 2, n_embd, 1);',
     '            e_put3(out, n_embd, n_embd, 1);'),
    ('token embedding is written transposed',
     '''    if tid == T_TOKEN_EMBD {
        e_put3(out, n_embd, n_vocab, 1);''',
     '''    if tid == T_TOKEN_EMBD {
        e_put3(out, n_vocab, n_embd, 1);'''),
    ('the indexer is reported as used by the MTP draft graph',
     '''    if kind == K_MTP_DENSE || kind == K_MTP_MOE {
        return 0;
    }''',
     '''    if kind == K_MTP_DENSE || kind == K_MTP_MOE {
        return 1;
    }'''),
    ('indexer top-k clamp keeps the requested cap instead of the cache size',
     '    if n_cached < top_k {',
     '    if n_cached > top_k {'),
]

M = HALF_1 + HALF_2

G = os.environ.get('GOOSE_BIN', '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose')
CC = os.environ.get('CC', 'gcc-13')
CXX = os.environ.get('CXX', 'g++-13')


def build_and_run(mutated: str, root: Path) -> tuple:
    """Materialise a mutated tree, build it at -O0 and run the gate."""
    (root / 'source' / 'base').mkdir(parents=True)
    (root / 'tests').mkdir()
    (root / 'source' / 'deepseek32.goose').write_text(mutated)
    shutil.copy2(S / 'source' / 'base' / 'model_graph.goose', root / 'source' / 'base' / 'model_graph.goose')
    shutil.copy2(S / 'tests' / 'fixture.cpp', root / 'tests' / 'fixture.cpp')
    shutil.copy2(S / 'tests' / 'build.sh', root / 'tests' / 'build.sh')
    env = dict(os.environ, OPT='-O0', OUT_DIR=str(root / 'build'), GOOSE_BIN=G, CC=CC, CXX=CXX)
    try:
        r = subprocess.run(['bash', str(root / 'tests' / 'build.sh')],
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
    label, old, new = M[idx]
    if ORIG.count(old) != 1:
        return {'idx': idx, 'label': label, 'status': 'ANCHOR', 'detail': f'anchor count {ORIG.count(old)}'}
    root = Path(tempfile.mkdtemp(prefix=f'mut{idx:02d}-', dir=str(S / 'build-mut')))
    try:
        caught, detail = build_and_run(ORIG.replace(old, new, 1), root)
    finally:
        shutil.rmtree(root, ignore_errors=True)
    return {'idx': idx, 'label': label, 'status': 'CAUGHT' if caught else 'MISSED', 'detail': detail}


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
    else:
        for i in range(lo, hi):
            r = one(i)
            print(f'[{r["status"]}] {r["idx"]} {r["label"]}: {r["detail"]}', flush=True)
            rows.append(r)
    if a.jobs > 1:
        for r in rows:
            print(f'[{r["status"]}] {r["idx"]} {r["label"]}: {r["detail"]}')
    missed = [r['idx'] for r in rows if r['status'] != 'CAUGHT']
    caught = sum(1 for r in rows if r['status'] == 'CAUGHT')
    print(f'mutations {caught}/{hi - lo} caught (range {lo}:{hi}) missed={missed}')
    return 1 if missed else 0


if __name__ == '__main__':
    sys.exit(main())
