#!/usr/bin/env python3
"""Assemble the matched-era candidate: current flint libraries + our merged libllama and server implementation."""
from pathlib import Path
import os, shutil, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin')
OUT = F / 'runtime/reconstructed/candidate-20261112-matched'
LLAMA = F / 'runtime/reconstructed/build/libllama-exp371.so.0.7.0'
IMPL = F / 'runtime/reconstructed/server-merge/build/libllama-server-impl.so'

for p in (LLAMA, IMPL):
    if not p.exists(): print('missing', p); sys.exit(1)
if OUT.exists(): shutil.rmtree(OUT)
OUT.mkdir(parents=True)

# every library comes from the current flint build; only libllama and the server impl are ours
for f in sorted(FL.glob('lib*.so*')) + [FL / 'llama-server']:
    if f.name.startswith('libllama.so'): continue
    dst = OUT / f.name
    if dst.exists() or dst.is_symlink(): dst.unlink()
    os.symlink(f, dst)

shutil.copy2(LLAMA, OUT / 'libllama.so.0.7.0')
os.symlink('libllama.so.0.7.0', OUT / 'libllama.so.0')
shutil.copy2(IMPL, OUT / 'libllama-server-impl.so')
shutil.copy2(FL / 'llama-server', OUT / 'llama-server')
(OUT / 'llama-server').chmod(0o755)
print('matched candidate at', OUT)
