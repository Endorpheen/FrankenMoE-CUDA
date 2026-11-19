#!/usr/bin/env python3
"""Rule 31: the cumulative baseline must carry every accepted Goose component.

Usage: check-cumulative-baseline.py <run-dir>
Exits non-zero when a previously accepted component is missing, so no
acceptance measurement can be taken on a build that silently lost one.
"""
import subprocess
import sys
from pathlib import Path

COMPONENTS = [
    # (library, symbol or string, description)
    ("libggml-cpu.so.0", "GOOSE_MMID_Q4K", "P1-12 Goose Q4_K tiled kernel dispatch"),
    ("libggml-cpu.so.0", "fg_cpu_simd_mmid_tiled_q4_k", "P1-12 Goose kernel reference"),
    ("libggml-base.so.0", "fg161_bulk", "P4-13 Goose allocator primitive"),
]

def symbols(path: Path) -> str:
    out = subprocess.run(["nm", "-D", str(path)], capture_output=True, text=True)
    return out.stdout

def strings(path: Path) -> str:
    out = subprocess.run(["strings", "-a", str(path)], capture_output=True, text=True)
    return out.stdout

def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    d = Path(sys.argv[1])
    if not d.is_dir():
        print(f"not a directory: {d}")
        return 2
    bad = 0
    for lib, needle, what in COMPONENTS:
        p = d / lib
        if not p.exists():
            print(f"MISSING {lib} in {d}")
            bad += 1
            continue
        blob = symbols(p) + strings(p)
        if needle not in blob:
            print(f"MISSING {what}: '{needle}' not in {lib}")
            bad += 1
        else:
            print(f"ok {what}")
    if not (d / "libmoe_mmid_tiled.so").exists():
        print("MISSING Goose kernel object libmoe_mmid_tiled.so")
        bad += 1
    else:
        print("ok Goose kernel object present")
    print("CUMULATIVE-BASELINE", "FAIL" if bad else "PASS")
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
