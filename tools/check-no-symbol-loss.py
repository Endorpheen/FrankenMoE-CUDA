#!/usr/bin/env python3
"""Compare the shipped library with an earlier checkpoint and report anything that disappeared.

Rule 33 says every accepted component must stay in one growing build. A list of known components is a
promise someone maintains; a symbol diff is a fact about two artefacts. This compares the exported
symbols and the diagnostic strings of the previous accepted checkpoint with the current build and
prints what is present in one and absent in the other.

Exit 1 when an exported symbol was lost. Strings are reported but do not fail: an optimised-away log
line is not necessarily a lost capability, whereas a missing export is.
"""
import subprocess
import sys

def newest_checkpoint():
    """The most recent accepted checkpoint, not an arbitrary older one.

    The previous default pointed at a checkpoint older than the vendoring change of 2026-11-16, so two internal C++
    constructors whose signature that change altered were reported as "lost" for weeks. A symbol diff is only
    meaningful against the build the project last accepted; against anything older it describes history.
    """
    import glob, os
    cands = glob.glob("builds/checkpoint-*/libllama.so.0.7.0")
    # by modification time, not by name: "chain" sorts after "EXP1412" in ASCII and is older
    return max(cands, key=os.path.getmtime) if cands else "builds/checkpoint-20261119-chain/libllama.so.0.7.0"

OLD = sys.argv[1] if len(sys.argv) > 1 else newest_checkpoint()
NEW = sys.argv[2] if len(sys.argv) > 2 else "builds/flint-acc/bin/libllama.so.0.7.0"

def syms(path):
    out = subprocess.run(f"nm -D --defined-only {path}", shell=True, capture_output=True, text=True).stdout
    names = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[1] in ("T", "W", "D", "B", "R"):
            names.add(parts[2])
    return names

def strs(path):
    out = subprocess.run(f"strings {path}", shell=True, capture_output=True, text=True).stdout
    return {s for s in out.splitlines() if s.startswith(("FG171_", "FG_", "ggml_goose", "GGML_GOOSE"))}

old_s, new_s = syms(OLD), syms(NEW)
old_t, new_t = strs(OLD), strs(NEW)
lost_syms = sorted(old_s - new_s)
lost_strs = sorted(old_t - new_t)

print(f"exported symbols: {len(old_s)} before, {len(new_s)} after, {len(lost_syms)} lost")
for s in lost_syms[:15]:
    print(f"  LOST {s}")
print(f"diagnostic strings: {len(old_t)} before, {len(new_t)} after, {len(lost_strs)} lost")
for s in lost_strs[:10]:
    print(f"  absent {s[:70]}")

if lost_syms:
    print("check-no-symbol-loss: FAIL, an exported symbol disappeared")
    sys.exit(1)
print("check-no-symbol-loss: ok, no exported symbol disappeared")
sys.exit(0)
