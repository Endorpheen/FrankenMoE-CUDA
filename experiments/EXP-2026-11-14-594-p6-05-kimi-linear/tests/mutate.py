#!/usr/bin/env python3
"""Mutation table for the kimi-linear plan/shape slice (P6-05 architecture 11).

Each mutation is a single semantic edit of source/kimi-linear.goose: the
recurrent marking read from n_head_kv, the plan bounds, the no-RoPE decision,
the MLA head split and the combined wkv_b width, the KDA conv geometry and gate
kind, the shared-expert multiplier, the dense-FFN boundary and the required
output tensor. A mutation is CAUGHT when the structure gate stops reporting
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
SRC_REL = Path('source') / 'kimi-linear.goose'
ORIG = (S / SRC_REL).read_text()

# (label, anchor, replacement) -- anchors must occur exactly once.
HALF_1 = [
    ('the recurrent marking is inverted, KDA layers become MLA',
     '''        if kv[i] == 0 {
            flags[i] = 1;
        } else {
            flags[i] = 0;
        }''',
     '''        if kv[i] == 0 {
            flags[i] = 0;
        } else {
            flags[i] = 1;
        }'''),
    ('the graph ignores the recurrent flag and builds MLA on every layer',
     '        return K_KDA;',
     '        return K_MLA;'),
    ('the plan bounds check past the last layer is dropped',
     '''    if il >= flags.len {
        return -1;
    }''',
     '''    if il >= flags.len {
        return K_MLA;
    }'''),
    ('the no-RoPE decision is flipped and k_pe is rotated',
     '''export fn kl_rope_applied() -> i64 {
    return 0;
}''',
     '''export fn kl_rope_applied() -> i64 {
    return 1;
}'''),
    ('the MLA key head keeps the rotary prefix in the nope split',
     '    return p[P_EMBD_HEAD_K] - p[P_N_ROT];',
     '    return p[P_EMBD_HEAD_K];'),
    ('the combined wkv_b drops the decompressed value width',
     '    return p[P_N_HEAD] * (kl_nope(p) + p[P_EMBD_HEAD_V]);',
     '    return p[P_N_HEAD] * kl_nope(p);'),
]

HALF_2 = [
    ('the KDA conv row forgets the Q/K/V triplication',
     '    return 3 * kl_kda_conv_state(p);',
     '    return kl_kda_conv_state(p);'),
    ('the KDA conv window counts d_conv taps instead of d_conv-1',
     '    let taps = if p[P_D_CONV] > 0 { p[P_D_CONV] - 1 } else { 3 };',
     '    let taps = if p[P_D_CONV] > 0 { p[P_D_CONV] } else { 3 };'),
    ('the shared expert loses the n_expert_shared multiplier',
     '        return n_ff_exp * n_shexp;',
     '        return n_ff_exp;'),
    ('the dense FFN boundary takes one extra leading layer',
     '''    if il < p[P_DENSE_LEAD] {
        return F_DENSE;
    }''',
     '''    if il <= p[P_DENSE_LEAD] {
        return F_DENSE;
    }'''),
    ('the output tensor is treated as optional',
     '''export fn kl_output_required() -> i64 {
    return 1;
}''',
     '''export fn kl_output_required() -> i64 {
    return 0;
}'''),
    ('the KDA output gate uses SiLU instead of sigmoid',
     '''export fn kl_kda_out_gate() -> i64 {
    return GATE_SIGMOID;
}''',
     '''export fn kl_kda_out_gate() -> i64 {
    return GATE_SILU;
}'''),
]

M = HALF_1 + HALF_2

G = os.environ.get('GOOSE_BIN', '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose')
CC = os.environ.get('CC', 'gcc-13')
CXX = os.environ.get('CXX', 'g++-13')


def build_and_run(mutated: str, root: Path) -> tuple:
    """Materialise a mutated tree, build it at -O0 and run the gate."""
    (root / 'source' / 'base').mkdir(parents=True)
    (root / 'tests').mkdir()
    (root / 'source' / 'kimi-linear.goose').write_text(mutated)
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
