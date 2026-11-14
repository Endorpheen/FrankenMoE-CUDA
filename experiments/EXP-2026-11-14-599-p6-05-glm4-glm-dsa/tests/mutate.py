#!/usr/bin/env python3
"""Mutation table for the glm4/glm-dsa plan/shape slices (P6-05 arch 14 and 15).

Each mutation is a single semantic edit of one architecture file: the NextN
plan, the fused SWIGLU row, the MLP transposition, the post-norm width, the
attention bias set, the MLA latent widths, the shared-expert multiplier, the
indexer projection width and the dense/MoE boundary. A mutation is CAUGHT when
the structure gate stops reporting `failures=0`: either a case mismatches, or
the build/run aborts.

Half 1 (indices 0-5) mutates source/glm4.goose; half 2 (indices 6-11) mutates
source/glm-dsa.goose. Every mutation runs in its own temporary tree so parallel
jobs never share a generated C file or an object cache. The gate itself stays
at -O1; mutation builds run at -O0.

Usage:
    tests/mutate.py            # both halves, sequential
    tests/mutate.py 0 6        # half 1
    tests/mutate.py 6 12       # half 2
    tests/mutate.py --jobs 4
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
FILES = {
    'g4': Path('source') / 'glm4.goose',
    'dsa': Path('source') / 'glm-dsa.goose',
}
TAG = {'g4': 'FG-ARCH14', 'dsa': 'FG-ARCH15'}
ORIG = {k: (S / v).read_text() for k, v in FILES.items()}

# (label, family, anchor, replacement) -- anchors must occur exactly once.
HALF_1 = [
    ('g4 ffn_up loses the fused SWIGLU factor',
     'g4',
     '    return 2 * p[P_N_FF];',
     '    return p[P_N_FF];'),
    ('g4 fused qkv bias keeps only the query block',
     'g4',
     '        e_put3(out, qkv, 1, 1);',
     '        e_put3(out, q_width, 1, 1);'),
    ('g4 ffn_down is written transposed',
     'g4',
     '        e_put3(out, p[P_N_FF], n_embd, 1);',
     '        e_put3(out, n_embd, p[P_N_FF], 1);'),
    ('g4 plan stops at the trunk and never plans the NextN block',
     'g4',
     '    return t_plan(p[P_N_LAYER], g4_n_layer_all(p), kinds);',
     '    return t_plan(p[P_N_LAYER], p[P_N_LAYER], kinds);'),
    ('g4 trunk blocks claim the NextN projection',
     'g4',
     '''    if kind != K_NEXTN {
        return 0;
    }
    if tid == T_NEXTN_EH_PROJ {''',
     '''    if kind != K_NEXTN {
        e_put3(out, g4_nextn_eh_proj(p), n_embd, 1); return 1;
    }
    if tid == T_NEXTN_EH_PROJ {'''),
    ('g4 post-norm width uses the head width instead of n_embd',
     'g4',
     '''    if tid == T_ATTN_POST_NORM || tid == T_FFN_POST_NORM {
        // glm4.cpp:46,52: post-norms are per hidden state, not per head.
        e_put3(out, n_embd, 1, 1);''',
     '''    if tid == T_ATTN_POST_NORM || tid == T_FFN_POST_NORM {
        // glm4.cpp:46,52: post-norms are per hidden state, not per head.
        e_put3(out, p[P_HEAD_K], 1, 1);'''),
]

HALF_2 = [
    ('dsa dense ffn_up takes the fused GLM-4 width',
     'dsa',
     '''        if tid == T_FFN_GATE || tid == T_FFN_UP {
            e_put3(out, n_embd, n_ff, 1);''',
     '''        if tid == T_FFN_GATE || tid == T_FFN_UP {
            e_put3(out, n_embd, 2 * n_ff, 1);'''),
    ('dsa shared expert drops the n_expert_shared multiplier',
     'dsa',
     '    let n_ff_shexp = n_ff_exp_v * p[P_N_SHEXP];',
     '    let n_ff_shexp = n_ff_exp_v;'),
    ('dsa wkv_a_mqa drops the rope block',
     'dsa',
     '        e_put3(out, n_embd, kv_lora + p[P_N_ROT], 1);',
     '        e_put3(out, n_embd, kv_lora, 1);'),
    ('dsa wk_b rows use the kv rank instead of the nope width',
     'dsa',
     '        e_put3(out, nope, kv_lora, n_head);',
     '        e_put3(out, kv_lora, kv_lora, n_head);'),
    ('dsa indexer projection columns use the indexer head size',
     'dsa',
     '        e_put3(out, n_embd, p[P_IDX_NHEAD], 1);',
     '        e_put3(out, n_embd, p[P_IDX_HEAD], 1);'),
    ('dsa dense/MoE boundary is off by one layer',
     'dsa',
     '''    if il < p[P_DENSE_LEAD] {
        return F_DENSE;
    }''',
     '''    if il <= p[P_DENSE_LEAD] {
        return F_DENSE;
    }'''),
]

M = HALF_1 + HALF_2

G = os.environ.get('GOOSE_BIN', '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose')
CC = os.environ.get('CC', 'gcc-13')
CXX = os.environ.get('CXX', 'g++-13')


def build_and_run(family: str, mutated: str, root: Path) -> tuple:
    """Materialise a mutated tree, build it at -O0 and run both gates."""
    (root / 'source' / 'base').mkdir(parents=True)
    (root / 'tests').mkdir()
    for f, rel in FILES.items():
        (root / rel).write_text(mutated if f == family else ORIG[f])
    for extra in ('model_graph.goose', 'trunk.goose'):
        shutil.copy2(S / 'source' / 'base' / extra, root / 'source' / 'base' / extra)
    for extra in ('fixture_arch14.cpp', 'fixture_arch15.cpp', 'build.sh'):
        shutil.copy2(S / 'tests' / extra, root / 'tests' / extra)
    env = dict(os.environ, OPT='-O0', OUT_DIR=str(root / 'build'), GOOSE_BIN=G, CC=CC, CXX=CXX)
    try:
        r = subprocess.run(['bash', str(root / 'tests' / 'build.sh')],
                           capture_output=True, text=True, timeout=900,
                           errors='replace', env=env)
    except subprocess.TimeoutExpired:
        return True, 'timeout'
    out = (r.stdout or '') + (r.stderr or '')
    if r.returncode != 0:
        tag = TAG[family]
        m = re.search(tag + r' cases=(\d+) values=(\d+) failures=(\d+)', out)
        if m:
            return True, f'{tag} failures={m.group(3)}'
        tail = [x for x in out.strip().splitlines() if x.strip()]
        return True, 'abort' + (': ' + tail[-1][:100] if tail else '')
    return False, 'gate still failures=0'


def one(idx: int) -> dict:
    label, family, old, new = M[idx]
    if ORIG[family].count(old) != 1:
        return {'idx': idx, 'label': label, 'status': 'ANCHOR',
                'detail': f'anchor count {ORIG[family].count(old)}'}
    root = Path(tempfile.mkdtemp(prefix=f'mut{idx:02d}-', dir=str(S / 'build-mut')))
    try:
        caught, detail = build_and_run(family, ORIG[family].replace(old, new, 1), root)
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
