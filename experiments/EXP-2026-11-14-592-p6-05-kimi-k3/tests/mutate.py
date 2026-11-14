#!/usr/bin/env python3
"""Mutation table for the kimi-k3 plan/shape slice (P6-05 architecture 10).

Each mutation is a single semantic edit of source/kimi-k3.goose: the dense-lead
boundary, the MTP refusal, the KDA/MLA marker, the marker sanity rule, the
residual checkpoint cadence and count, the latent width, the MLA head split,
the joint latent projection width, the routed-expert input width, the MLA query
width and the shared-expert width. A mutation is CAUGHT when the structure gate
stops reporting `failures=0`: either a case mismatches, or the build/run aborts.

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
SRC_REL = Path('source') / 'kimi-k3.goose'
ORIG = (S / SRC_REL).read_text()

# (label, anchor, replacement) -- anchors must occur exactly once.
HALF_1 = [
    ('the dense-lead boundary includes one routed layer',
     '''    if il < p[P_DENSE_LEAD] {
        return K_DENSE;''',
     '''    if il <= p[P_DENSE_LEAD] {
        return K_DENSE;'''),
    ('MTP is declared supported, so a declared nextn block is accepted',
     '''export fn mk_mtp_supported() -> i64 {
    return 0;
}''',
     '''export fn mk_mtp_supported() -> i64 {
    return 1;
}'''),
    ('a zero n_head_kv is planned as MLA instead of KDA',
     '''export fn mk_attn_kind(n_head_kv: i64) -> i64 {
    if n_head_kv == 0 {
        return A_KDA;
    }''',
     '''export fn mk_attn_kind(n_head_kv: i64) -> i64 {
    if n_head_kv == 0 {
        return A_MLA;
    }'''),
    ('the marker sanity check drops the divisibility rule',
     '    if n_head % n_head_kv == 0 {',
     '    if n_head >= n_head_kv {'),
    ('the residual checkpoint is banked on every non-checkpoint layer',
     '    if il % p[P_RES_BLOCK] == 0 {',
     '    if il % p[P_RES_BLOCK] != 0 {'),
    ('the leading checkpoint is not counted',
     '    return (n_layer - 1) / res_bs + 1;',
     '    return (n_layer - 1) / res_bs;'),
]

HALF_2 = [
    ('the latent width is ignored and experts run at the full hidden size',
     '''    if n_latent > 0 {
        return n_latent;
    }''',
     '''    if n_latent < 0 {
        return n_latent;
    }'''),
    ('the nope share is reported as the whole MLA head',
     '''    e_ensure(head_k >= n_rot, 26);
    return head_k - n_rot;''',
     '''    e_ensure(head_k >= n_rot, 26);
    return head_k;'''),
    ('the joint latent projection loses its rope part',
     '''    e_ensure(kv_lora >= 0 && n_rot >= 0, 28);
    return kv_lora + n_rot;''',
     '''    e_ensure(kv_lora >= 0 && n_rot >= 0, 28);
    return kv_lora;'''),
    ('routed experts read the full hidden width instead of the latent one',
     'e_put3(out, embd_latent, n_ff_exp_v, n_expert);',
     'e_put3(out, n_embd, n_ff_exp_v, n_expert);'),
    ('the MLA query projection uses the rotary width instead of the head width',
     'e_put3(out, q_lora, n_head * head_k, 1);',
     'e_put3(out, q_lora, n_head * n_rot, 1);'),
    ('the shared-expert width ignores the declared expert count',
     '        return n_ff_exp_v * n_shared;',
     '        return n_ff_exp_v;'),
]

M = HALF_1 + HALF_2

G = os.environ.get('GOOSE_BIN', '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose')
CC = os.environ.get('CC', 'gcc-13')
CXX = os.environ.get('CXX', 'g++-13')


def build_and_run(mutated: str, root: Path) -> tuple:
    """Materialise a mutated tree, build it at -O0 and run the gate."""
    (root / 'source' / 'base').mkdir(parents=True)
    (root / 'tests').mkdir()
    (root / 'source' / 'kimi-k3.goose').write_text(mutated)
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
