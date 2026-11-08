#!/usr/bin/env python3
"""Create an include shim so the copied llama-batch TU sees the patched
llama-batch.h everywhere, while all other headers stay the native snapshots.
Idempotent; only writes inside batch-allocr/shim."""
from pathlib import Path
import os

here = Path(__file__).resolve().parent
shim = here / "shim"
shim.mkdir(exist_ok=True)
src = Path("/inputs/native/src")
for p in src.iterdir():
    if not p.is_file() or not p.name.endswith(".h"):
        continue
    target = p if p.name != "llama-batch.h" else (here / "llama-batch-copy.h")
    link = shim / p.name
    if link.is_symlink() or link.exists():
        link.unlink()
    os.symlink(target, link)
print("shim ready:", shim)
