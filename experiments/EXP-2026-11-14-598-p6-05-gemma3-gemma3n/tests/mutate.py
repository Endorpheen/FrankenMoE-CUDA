#!/usr/bin/env python3
"""Mutation table for the gemma3/gemma3n plan/shape slices (P6-05 arch 16, 17).

Each mutation is a single semantic edit of one architecture file: the SWA
detection and mask, the query and key/value widths, the 27B attention-scale
rule, the softcap decision, the KV prefix, altup widths, the per-layer table,
the laurel rank and activation sparsity. A mutation is CAUGHT when the
structure gate stops reporting `failures=0`: either a case mismatches, or the
build/run aborts.

Half 1 (indices 0-5) mutates source/gemma3.goose; half 2 (indices 6-11)
mutates source/gemma3n.goose. Every mutation runs in its own temporary tree so
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
    'g3': Path('source') / 'gemma3.goose',
    'g3n': Path('source') / 'gemma3n.goose',
}
ORIG = {k: (S / v).read_text() for k, v in FILES.items()}

# (label, family, anchor, replacement) -- anchors must occur exactly once.
HALF_1 = [
    ('g3 SWA detection accepts a zero window',
     'g3',
     '''    if p[P_N_SWA] > 0 {
        return 1;
    }''',
     '''    if p[P_N_SWA] >= 0 {
        return 1;
    }'''),
    ('g3 SWA mask shifts every layer by one',
     'g3',
     '    return g_swa_is(p[P_SWA_PERIOD], il);',
     '    return g_swa_is(p[P_SWA_PERIOD], il + 1);'),
    ('g3 query width divides n_embd by the head count',
     'g3',
     '    return g_gqa_width(p[P_HEAD_K], p[P_N_HEAD]);',
     '    return p[P_N_EMBD] / p[P_N_HEAD];'),
    ('g3 key/value width uses the query head count',
     'g3',
     '    return g_gqa_width(p[P_HEAD_V], p[P_N_HEAD_KV]);',
     '    return g_gqa_width(p[P_HEAD_V], p[P_N_HEAD]);'),
    ('g3 27B attention scale rule fires on the 4B size',
     'g3',
     '    if g3_model_type(p) == MT_27B {',
     '    if g3_model_type(p) == MT_4B {'),
    ('g3 drops the conditional logit softcap',
     'g3',
     '''    if p[P_SOFTCAP_BITS] != 0 {
        return 1;
    }
    return 0;''',
     '''    if p[P_SOFTCAP_BITS] != 0 {
        return 0;
    }
    return 0;'''),
]

HALF_2 = [
    ('g3n KV prefix includes one layer too many',
     'g3n',
     '        if il < n_kv_from_start {',
     '        if il <= n_kv_from_start {'),
    ('g3n altup projection keeps all states instead of n_altup - 1',
     'g3n',
     '    return p[P_N_ALTUP] - 1;',
     '    return p[P_N_ALTUP];'),
    ('g3n per-layer token table drops the layer axis',
     'g3n',
     '        e_put3(out, embd_altup * n_layer, n_vocab, 1);',
     '        e_put3(out, embd_altup, n_vocab, 1);'),
    ('g3n laurel_l uses the altup width instead of the laurel rank',
     'g3n',
     '        e_put3(out, n_embd, laurel, 1);',
     '        e_put3(out, n_embd, embd_altup, 1);'),
    ('g3n fixed attention scale becomes zero',
     'g3n',
     'const G3N_ATTN_SCALE_BITS: i64 = 1065353216; // 1.0f, gemma3n.cpp:10',
     'const G3N_ATTN_SCALE_BITS: i64 = 0; // 1.0f, gemma3n.cpp:10'),
    ('g3n activation sparsity covers one layer too many',
     'g3n',
     '    if il < G3N_SPARSITY_LAYERS {',
     '    if il <= G3N_SPARSITY_LAYERS {'),
]

M = HALF_1 + HALF_2

G = os.environ.get('GOOSE_BIN', '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose')
CC = os.environ.get('CC', 'gcc-13')
CXX = os.environ.get('CXX', 'g++-13')
TAGS = {'g3': 'FG-ARCH16', 'g3n': 'FG-ARCH17'}


def build_and_run(family: str, mutated: str, root: Path) -> tuple:
    """Materialise a mutated tree, build it at -O0 and run both gates."""
    (root / 'source' / 'base').mkdir(parents=True)
    (root / 'tests').mkdir()
    for f, rel in FILES.items():
        (root / rel).write_text(mutated if f == family else ORIG[f])
    for extra in ('model_graph.goose', 'gemma.goose'):
        shutil.copy2(S / 'source' / 'base' / extra, root / 'source' / 'base' / extra)
    for extra in ('fixture_arch16.cpp', 'fixture_arch17.cpp', 'build.sh'):
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
        tag = TAGS[family]
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
