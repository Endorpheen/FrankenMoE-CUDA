#!/usr/bin/env python3
"""Materialise a run directory for the stage DSO.

Every file of the accepted runtime is hard-linked into the stage directory, so the run has the exact
same binaries except libllama. The active runtime is never modified.
"""
from pathlib import Path
import os, shutil, sys

E = Path(__file__).resolve().parent
SRC = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp244/run')
DST = E / 'build/stage-run'
STAGE = E / 'build/libllama-exp347.so.0.3.0'

if not STAGE.exists():
    print('stage DSO missing:', STAGE); sys.exit(1)

DST.mkdir(parents=True, exist_ok=True)
made = 0
for p in SRC.iterdir():
    target = DST / p.name
    if target.exists() or target.is_symlink():
        if target.is_symlink() or target.is_file():
            target.unlink()
    real = p.resolve()
    if not real.exists():
        print('skip broken:', p.name); continue
    try:
        os.link(real, target)
    except OSError:
        shutil.copy2(real, target)
    made += 1

# the one substitution: libllama points at the stage
for name in ('libllama.so.0.7.0',):
    t = DST / name
    if t.exists() or t.is_symlink():
        t.unlink()
    os.link(STAGE, t)
print('stage run ready:', DST, 'files:', made)
print('libllama ->', os.path.realpath(DST/'libllama.so.0.7.0'))
