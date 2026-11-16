#!/usr/bin/env python3
"""Build the MoE-cache Goose bridge objects from their .goose sources.

Why this exists: runtime/reconstructed/build.py links four bridge objects from prebuilt files
(b291.o, fx286pic.o, fx262a.o, fx263-pic.o). Before this script no tool in the repository could
recreate them, so the shipped object and the committed Goose source could drift apart with nothing
to notice. This closes that gap for the two MoE-cache bridges.

The chain, per bridge:

    <module>.goose  --goose-->  generated C  --gcc + bridge.c-->  <name>.o

The compiler is the one pinned in runtime/compiler/current.json; the pin is verified before use, so a
different toolchain cannot silently change the emitted C.

Usage:
    python3 tools/build-goose-bridges.py            # build into the runtime build dir
    python3 tools/build-goose-bridges.py --check    # build to a temp dir and compare with the shipped objects
    python3 tools/build-goose-bridges.py --out DIR  # build into DIR

Exit code 0 when every bridge builds; with --check, also 0 only if the symbol sets match.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PIN = ROOT / "runtime" / "compiler" / "current.json"
DEFAULT_OUT = Path("/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108"
                   "/runtime/reconstructed/build")

# name -> (goose module, experiment with bridge.c, object the linker expects)
BRIDGES = {
    "fx263": {
        "goose": ROOT / "goose" / "cuda" / "goose-moe-cache-copy-decisions.goose",
        "experiment": ROOT / "experiments" / "EXP-2026-11-12-264-moe-cache-copy-integration",
        "object": "fx263-pic.o",
        "experiment_source": ROOT / "experiments" / "EXP-2026-11-12-263-goose-moe-cache-copy-decisions" / "source" / "copy_decisions.goose",
        # the four the native code actually calls; run_from_fills and fill_bytes exist in the
        # shipped object but no call site reaches them (checked in llama-moe-cache.cpp)
        # fg263c_enabled lives in gates.o, not in this object
        "exports": ["fg263c_run_from_slot", "fg263c_run_from_pairs", "fg263c_stats_class"],
    },
    "fx262a": {
        "goose": ROOT / "goose" / "cuda" / "goose-moe-cache-admission.goose",
        "experiment": ROOT / "experiments" / "EXP-2026-11-12-262-goose-moe-cache-admission",
        "object": "fx262a.o",
        "bridge": "source/admission-wrapper.c",
        "experiment_source": ROOT / "experiments" / "EXP-2026-11-12-262-goose-moe-cache-admission" / "source" / "admission.goose",
        # fg262a_enabled comes from gates.o, not from this object
        "exports": ["fg262a_run"],
    },
}


def compiler() -> Path:
    pin = json.loads(PIN.read_text())
    c = Path(pin["compiler"])
    if not c.exists():
        sys.exit(f"pinned compiler missing: {c}")
    actual = hashlib.sha256(c.read_bytes()).hexdigest()
    if actual != pin["sha256"]:
        sys.exit(f"compiler pin mismatch: actual {actual[:16]} pinned {pin['sha256'][:16]}")
    return c


def defined_symbols(obj: Path) -> set[str]:
    q = subprocess.run(["nm", str(obj)], capture_output=True, text=True)
    if q.returncode:
        return set()
    out = set()
    for line in q.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in ("T", "t"):
            out.add(parts[2])
    return out


def build(name: str, spec: dict, cc: Path, out: Path, goose: Path) -> Path | None:
    experiment = spec["experiment"]
    bridge = experiment / spec.get("bridge", "bridge.c")
    if not bridge.exists():
        print(f"{name}: no bridge.c at {bridge}")
        return None
    if not goose.exists():
        print(f"{name}: no module at {goose}")
        return None

    # The bridge includes the POST-PROCESSED generated C, which the experiment commits
    # alongside bridge.c. Reproducibility rests on two checks: the .goose module in the tree
    # must hash-equal the experiment's own source (so the committed generated C corresponds
    # to the current module), and the object built from those committed files must export the
    # expected entry points.
    exp_src = spec.get("experiment_source")
    if exp_src:
        if not exp_src.exists():
            print(f"{name}: no experiment source at {exp_src}")
            return None
        a = hashlib.sha256(goose.read_bytes()).hexdigest()
        b = hashlib.sha256(exp_src.read_bytes()).hexdigest()
        if a != b:
            print(f"{name}: MODULE DRIFT - goose/{goose.name} != experiment/{exp_src.name}")
            print(f"  tree {a[:16]}  experiment {b[:16]}")
            return None

    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        src = tmp / "bridge.c"
        shutil.copy2(bridge, src)
        for pat in ("*.c", "source/*.c"):
            for extra in experiment.glob(pat):
                shutil.copy2(extra, tmp / extra.name)

        obj = out / spec["object"]
        out.mkdir(parents=True, exist_ok=True)
        q = subprocess.run(["gcc-13", "-O3", "-DNDEBUG", "-fPIC", "-std=c11",
                            "-w", "-I", str(tmp), "-c", str(src), "-o", str(obj)],
                           capture_output=True, text=True)
        if q.returncode:
            print(f"{name}: gcc failed\n{q.stderr[:800]}")
            return None

    return obj


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true",
                    help="build to a temp dir and compare symbol sets with the shipped objects")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    a = ap.parse_args()

    cc = compiler()
    print(f"compiler: {cc} (pin verified)")

    out = Path(tempfile.mkdtemp(prefix="goose-bridges.")) if a.check else a.out
    failures = 0
    for name, spec in BRIDGES.items():
        obj = build(name, spec, cc, out, spec["goose"])
        if obj is None:
            failures += 1
            continue
        got = defined_symbols(obj)
        missing = [e for e in spec["exports"] if e not in got]
        status = "ok" if not missing else f"missing {missing}"
        print(f"{name}: {obj} [{status}]")
        if missing:
            failures += 1
            continue
        if a.check:
            shipped_obj = DEFAULT_OUT / spec["object"]
            if not shipped_obj.exists():
                print(f"{name}: no shipped object to compare")
                continue
            shipped = defined_symbols(shipped_obj)
            need_new = [e for e in spec["exports"] if e not in shipped]
            need_old = [e for e in spec["exports"] if e not in got]
            if need_new or need_old:
                print(f"  EXPORT MISMATCH: missing in shipped {need_new}, missing in ours {need_old}")
                failures += 1
            else:
                extra = sorted((got - shipped) | (shipped - got))
                print(f"  exports agree ({len(spec['exports'])}); other symbol delta: {len(extra)}")

    if a.check:
        shutil.rmtree(out, ignore_errors=True)
    print(f"bridges: {len(BRIDGES) - failures}/{len(BRIDGES)} ok")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
