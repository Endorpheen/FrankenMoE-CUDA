#!/usr/bin/env python3
"""Verify the qualified runtime is the one the manifests describe, and that every module
in the active chain actually executed in a given Apex run.

Two independent failure modes are covered:

1. **Library drift** -- the active `libggml-cuda.so.0` symlink must resolve to the file
   whose hash the stage manifest recorded. Without this check a rebuild can silently pair
   new binaries with old claims, which already happened once after the EXP180 switch fix.

2. **Silent module loss** -- a module can stay linked but stop being reached (an
   eligibility branch flips, a selector changes). The witness line is the only evidence
   that a module ran, so absence is treated as failure rather than assumed benign.

Usage:  python3 verify_runtime.py [<apex server.log>]
Exit:   0 when the runtime matches and all witnesses are present.
"""
from pathlib import Path
import json, re, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
STAGE = 'exp185'
RUN = F / f'runtime/{STAGE}/run'
MANIFEST = F / f'experiments/EXP-2026-11-08-185-goose-cuda-mmvq-q4k/runtime-manifest.json'

# Every module the active chain is expected to execute, with the witness it must emit.
EXPECTED = {
    'EXP166 activations':  'FG166_CUDA_ACTIVATIONS',
    'EXP168 norm':         'FG168_CUDA_NORM',
    'EXP169 topk':         'FG169T_CUDA_TOPK',
    'EXP170 ssm':          'FG170S_CUDA_SSM',
    'EXP171 rope':         'FG171R_CUDA_ROPE',
    'EXP174 gdn (mma)':    'FG174M_CUDA_GDN',
    'EXP174 gdn (scalar)': 'FG174S_CUDA_GDN',
    'EXP175 concat':       'FG175C_CUDA_CONCAT',
    'EXP173 flash attn':   'FG175FA_CUDA',
    'EXP176 get_rows':     'FG176G_CUDA_GET_ROWS',
    'EXP176 set_rows':     'FG176R_CUDA_SET_ROWS',
    'EXP177 cpy':          'FG177C_CUDA_CPY',
    'EXP178 q8_1':         'FG178Q_CUDA_Q8_1',
    'EXP179 moe weighted': 'FG179W_CUDA_MOE_WEIGHTED',
    'EXP181 mm_ids':       'FG181M_CUDA_MM_IDS',
    'EXP183 fwht':         'FG183F_CUDA_FWHT',
    'EXP184 mmvf':         'FG184M_CUDA_MMVF',
    'EXP185 mmvq q4_k':    'FG185Q_CUDA_MMVQ_Q4K',
}

# Modules that must NOT appear: rejected candidates are opt-in and off unless asked for.
FORBIDDEN = {'EXP180 mmq q8_1 (rejected)': 'FG180Q_CUDA_MMQ_Q8_1',
             'EXP186 mmvq q6_k (rejected)': 'FG186Q_CUDA_MMVQ_Q6K'}

failures = []

# 1. library identity
link = RUN / 'libggml-cuda.so.0'
if not link.exists():
    failures.append(f'missing {link}')
else:
    actual = link.resolve()
    want = json.loads(MANIFEST.read_text())['sha256']
    import hashlib
    got = hashlib.sha256(actual.read_bytes()).hexdigest()
    if got != want:
        failures.append(f'library drift: active {got[:16]} != manifest {want[:16]}')
    else:
        print(f'library  OK  {actual.name}  sha256={got[:16]}')

    stray = [p.name for p in RUN.glob('libggml-cuda-*.so.0.27.0')
             if not p.is_symlink() and p.resolve() != actual]
    if stray:
        failures.append(f'stale libraries in run dir: {stray}')
    else:
        print(f'library  OK  no stale copies in {RUN.name}')

# 2. witnesses
log_path = Path(sys.argv[1]) if len(sys.argv) > 1 else None
if log_path is None:
    cands = sorted((F / f'runtime/{STAGE}/results').glob('*-goose/server.log'), key=lambda p: p.stat().st_mtime)
    log_path = cands[-1] if cands else None
if not log_path or not log_path.exists():
    failures.append('no Apex server log to check witnesses against')
else:
    log = log_path.read_text(errors='ignore')
    try:
        shown = log_path.relative_to(F)
    except ValueError:
        shown = log_path
    print(f'log      {shown}')
    missing = [f'{name} ({w})' for name, w in EXPECTED.items() if w not in log]
    if missing:
        failures.extend(f'module did not execute: {m}' for m in missing)
    else:
        print(f'witness  OK  all {len(EXPECTED)} expected modules executed')
    present = [f'{name} ({w})' for name, w in FORBIDDEN.items() if w in log]
    if present:
        failures.extend(f'rejected module is running: {p}' for p in present)
    else:
        print(f'witness  OK  no rejected module executed')

if failures:
    print('\nFAILURES:')
    for f in failures:
        print(' ', f)
    raise SystemExit(1)
print('\nRUNTIME_VERIFY_PASS')
