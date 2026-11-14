#!/usr/bin/env python3
"""Mutation table for the glm5-next plan/shape slice (P6-05 architecture 12).

Each mutation is a single semantic edit of source/glm5-next.goose: the plan
kind, the MTP boundary, the dense-lead boundary, the KDA inner width, the
hyper-connection mixer width, the shared-expert width, the two MLA output
widths, the k-pool selection width and top-pool cap, and the MTP input
projection. A mutation is CAUGHT when the structure gate stops reporting
`failures=0`: either a case mismatches, or the build/run aborts.

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
SRC_REL = Path('source') / 'glm5-next.goose'
ORIG = (S / SRC_REL).read_text()

# (label, anchor, replacement) -- anchors must occur exactly once.
HALF_1 = [
    ('the kv-head rule is inverted, so KDA layers become MLA and back',
     '''    if kv_heads[il] == 0 {
        return K_RECR;
    }
    return K_MLA;''',
     '''    if kv_heads[il] == 0 {
        return K_MLA;
    }
    return K_RECR;'''),
    ('the MTP boundary is exclusive, so the MTP layer is planned as a trunk layer',
     '''    if il >= p[P_N_LAYER] {
        return K_MTP;
    }''',
     '''    if il > p[P_N_LAYER] {
        return K_MTP;
    }'''),
    ('the dense-lead boundary includes one routed layer',
     '''    if il < p[P_DENSE_LEAD] {
        return F_DENSE;
    }''',
     '''    if il <= p[P_DENSE_LEAD] {
        return F_DENSE;
    }'''),
    ('the KDA inner width keeps only half of the query heads',
     '    return head_dim * n_head;',
     '    return head_dim * (n_head / 2);'),
    ('the hyper-connection mixer drops one affine row',
     '    return (2 + hc) * hc;',
     '    return (1 + hc) * hc;'),
    ('the shared expert ignores the shared-expert count',
     '    return ff_exp * n_shexp;',
     '    return ff_exp;'),
]

HALF_2 = [
    ('the MLA query projection uses the value head width instead of the key width',
     '            e_put3(out, q_lora, n_head * head_k, 1);',
     '            e_put3(out, q_lora, n_head * head_v, 1);'),
    ('the MLA output projection uses the key head width instead of the value width',
     '            e_put3(out, n_head * head_v, n_embd, 1);',
     '            e_put3(out, n_head * head_k, n_embd, 1);'),
    ('wk_b swaps the nope and latent dimensions',
     '            e_put3(out, qk_nope, kv_lora, n_head);',
     '            e_put3(out, kv_lora, qk_nope, n_head);'),
    ('the k-pool tail is appended only when the model did not ask for it',
     '''    if tail != 0 {
        n = n + (kpool - 1);
    }''',
     '''    if tail == 0 {
        n = n + (kpool - 1);
    }'''),
    ('the top-pool count is not divided by kpool',
     '    let want = top_k / kpool;',
     '    let want = top_k;'),
    ('the MTP input projection loses the doubled hidden state',
     '    return 2 * n_embd;',
     '    return n_embd;'),
]

M = HALF_1 + HALF_2

G = os.environ.get('GOOSE_BIN', '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose')
CC = os.environ.get('CC', 'gcc-13')
CXX = os.environ.get('CXX', 'g++-13')


def build_and_run(mutated: str, root: Path) -> tuple:
    """Materialise a mutated tree, build it at -O0 and run the gate."""
    (root / 'source' / 'base').mkdir(parents=True)
    (root / 'tests').mkdir()
    (root / SRC_REL).write_text(mutated)
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
