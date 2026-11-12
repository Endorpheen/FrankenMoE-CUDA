#!/usr/bin/env python3
"""Assemble the merged candidate: the EXP340 stage with the merged libllama and the merged server libraries."""
from pathlib import Path
import shutil, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
BASE = F / 'experiments/EXP-2026-11-12-340-p6-03-request-trace/stage'
OUT = F / 'runtime/reconstructed/candidate-20261112'
LLAMA = F / 'runtime/reconstructed/build/libllama-exp371.so.0.7.0'
IMPL = F / 'runtime/reconstructed/server-merge/build/libllama-server-impl.so'
SERVER = F / 'runtime/reconstructed/server-merge/build/llama-server'

for p in (LLAMA, IMPL, SERVER):
    if not p.exists():
        print('missing artifact:', p); sys.exit(1)

if OUT.exists():
    shutil.rmtree(OUT)
shutil.copytree(BASE, OUT, symlinks=True)
shutil.copy2(LLAMA, OUT / 'libllama.so.0.7.0')
shutil.copy2(IMPL, OUT / 'libllama-server-impl.so')
shutil.copy2(SERVER, OUT / 'llama-server')
(OUT / 'llama-server').chmod(0o755)

def defined(path, prefix):
    out = subprocess.run(['nm', str(path)], capture_output=True, text=True).stdout
    return sum(1 for l in out.splitlines() if l.endswith(' T ' + prefix) or (' T ' + prefix) in l)

print('candidate at', OUT)
for sym in ('fg262a', 'fg263c', 'fg286m', 'fg291b'):
    print(' ', sym, defined(OUT / 'libllama.so.0.7.0', sym))
for sym in ('fg268p', 'fg270q', 'fg272s', 'fg274b', 'fg276t'):
    print(' ', sym, defined(OUT / 'libllama-server-impl.so', sym))
