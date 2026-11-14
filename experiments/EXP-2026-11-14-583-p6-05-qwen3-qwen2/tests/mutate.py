#!/usr/bin/env python3
"""Mutation table for the qwen3/qwen2 plan/shape slices (P6-05 arch 3 and 6).

Each mutation is a single semantic edit of one architecture file: the plan
length, the GQA key/value width, the query-block width, the qk-norm width or
presence, the FFN shapes, and the qwen2 bias widths. A mutation is CAUGHT when
the structure gate stops reporting `failures=0`: either a case mismatches, or
the build/run aborts.

Half 1 (indices 0-5) mutates source/qwen3.goose; half 2 (indices 6-11)
mutates source/qwen2.goose. Every mutation runs in its own temporary tree so
parallel jobs never share a generated C file or an object cache. The gate
itself stays at -O1; mutation builds run at -O0.

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
    'q3': Path('source') / 'qwen3.goose',
    'q2': Path('source') / 'qwen2.goose',
}
ORIG = {k: (S / v).read_text() for k, v in FILES.items()}

# (label, family, anchor, replacement) -- anchors must occur exactly once.
HALF_1 = [
    ('q3 fused QKV drops the query head count',
     'q3',
     '    return d_gqa_width(p[P_HEAD_K], p[P_N_HEAD]);',
     '    return d_gqa_width(p[P_HEAD_K], 1);'),
    ('q3 key/value width uses the query head count',
     'q3',
     '    return d_gqa_width(p[P_HEAD_V], p[P_N_HEAD_KV]);',
     '    return d_gqa_width(p[P_HEAD_V], p[P_N_HEAD]);'),
    ('q3 attn_out rows use n_embd instead of the query block',
     'q3',
     '        e_put3(out, q_width, n_embd, 1);',
     '        e_put3(out, n_embd, n_embd, 1);'),
    ('q3 qk-norm width uses n_embd instead of the head width',
     'q3',
     '        e_put3(out, p[P_HEAD_K], 1, 1);',
     '        e_put3(out, n_embd, 1, 1);'),
    ('q3 ffn_down is written transposed',
     'q3',
     '        e_put3(out, n_ff, n_embd, 1);',
     '        e_put3(out, n_embd, n_ff, 1);'),
    ('q3 plan drops its last layer',
     'q3',
     '    return d_plan(p[P_N_LAYER], kinds);',
     '    return d_plan(p[P_N_LAYER] - 1, kinds);'),
]

HALF_2 = [
    ('q2 key/value width divides n_embd by the kv-head count',
     'q2',
     '    return d_gqa_width(p[P_HEAD_V], p[P_N_HEAD_KV]);',
     '    return p[P_N_EMBD] / p[P_N_HEAD_KV];'),
    ('q2 fused QKV bias drops the key/value blocks',
     'q2',
     '        e_put3(out, qkv, 1, 1);',
     '        e_put3(out, q_width, 1, 1);'),
    ('q2 output bias width uses n_embd instead of n_vocab',
     'q2',
     '        e_put3(out, n_vocab, 1, 1);',
     '        e_put3(out, n_embd, 1, 1);'),
    ('q2 claims a qk-norm tensor that the loader never declares',
     'q2',
     '''    if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM {
        // no qk-norm tensors exist for this architecture.
        return 0;''',
     '''    if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM {
        // no qk-norm tensors exist for this architecture.
        e_put3(out, p[P_HEAD_K], 1, 1); return 1;'''),
    ('q2 ffn_up is written transposed',
     'q2',
     '''    if tid == T_FFN_GATE || tid == T_FFN_UP {
        e_put3(out, n_embd, n_ff, 1);''',
     '''    if tid == T_FFN_GATE || tid == T_FFN_UP {
        e_put3(out, n_ff, n_embd, 1);'''),
    ('q2 plan drops its last layer',
     'q2',
     '    return d_plan(p[P_N_LAYER], kinds);',
     '    return d_plan(p[P_N_LAYER] - 1, kinds);'),
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
    for extra in ('model_graph.goose', 'dense.goose'):
        shutil.copy2(S / 'source' / 'base' / extra, root / 'source' / 'base' / extra)
    for extra in ('fixture_arch3.cpp', 'fixture_arch6.cpp', 'build.sh'):
        shutil.copy2(S / 'tests' / extra, root / 'tests' / extra)
    env = dict(os.environ, OPT='-O0', OUT_DIR=str(root / 'build'), GOOSE_BIN=G, CC=CC, CXX=CXX)
    try:
        r = subprocess.run(['bash', str(root / 'tests' / 'build.sh')],
                           capture_output=True, text=True, timeout=600,
                           errors='replace', env=env)
    except subprocess.TimeoutExpired:
        return True, 'timeout'
    out = (r.stdout or '') + (r.stderr or '')
    if r.returncode != 0:
        tag = 'FG-ARCH3' if family == 'q3' else 'FG-ARCH6'
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
