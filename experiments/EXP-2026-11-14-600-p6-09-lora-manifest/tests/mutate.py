#!/usr/bin/env python3
"""Mutation runner for the P6-09 S1 gate (EXP-2026-11-14-583).

Each mutation is applied to a fresh copy of source/lora-manifest.goose, then
tests/build.sh is invoked with FG_SRC pointing at the copy and a separate
FG_OUT. The mutated source must still pass `goose check` (a mutation that does
not compile proves nothing about the gate), and it must then fail the gate.

Usage:
    python3 tests/mutate.py              # both halves, all 12
    python3 tests/mutate.py --half A     # metadata half (6)
    python3 tests/mutate.py --half B     # pairing/shape half (6)
    python3 tests/mutate.py --only B4    # one mutation
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SPEC = os.path.join(HERE, "mutations.json")
BUILD = os.path.join(HERE, "build.sh")
GOOSE = os.environ.get("FG_GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--half", choices=("A", "B", "all"), default="all")
    ap.add_argument("--only", default=None)
    args = ap.parse_args()

    with open(SPEC, encoding="utf-8") as f:
        spec = json.load(f)

    source = os.path.join(ROOT, spec["source"])
    with open(source, encoding="utf-8") as f:
        base = f.read()

    chosen = [
        m for m in spec["mutations"]
        if (args.only is None or m["id"] == args.only)
        and (args.half == "all" or m.get("half") == args.half)
    ]
    if not chosen:
        print("FG583_MUT nothing selected", file=sys.stderr)
        return 2

    tmp = tempfile.mkdtemp(prefix="fg583-mut-")
    caught = 0
    missed = []
    uncompiled = []

    for m in chosen:
        old = m["old"]
        new = m["new"]
        count = base.count(old)
        if count != 1:
            print(f"FG583_MUT {m['id']} bad pattern occurrences={count}", file=sys.stderr)
            missed.append(m["id"])
            continue

        path = os.path.join(tmp, f"lora-{m['id']}.goose")
        with open(path, "w", encoding="utf-8") as f:
            f.write(base.replace(old, new, 1))

        check = subprocess.run(
            [GOOSE, "check", path],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if check.returncode != 0:
            print(f"FG583_MUT {m['id']} DOES NOT COMPILE half={m.get('half')}")
            print("\n".join(check.stdout.strip().splitlines()[-6:]))
            uncompiled.append(m["id"])
            continue

        env = dict(os.environ)
        env["FG_SRC"] = path
        env["FG_OUT"] = os.path.join(ROOT, "build-mut", m["id"])

        proc = subprocess.run(
            ["bash", BUILD],
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if proc.returncode != 0:
            caught += 1
            print(f"FG583_MUT {m['id']} caught half={m.get('half')} note={m.get('note')}")
        else:
            missed.append(m["id"])
            tail = "\n".join(proc.stdout.strip().splitlines()[-6:])
            print(f"FG583_MUT {m['id']} MISSED half={m.get('half')} note={m.get('note')}")
            print(tail)

    shutil.rmtree(tmp, ignore_errors=True)

    print(f"FG583_MUT half={args.half} caught={caught}/{len(chosen)}")
    if missed or uncompiled:
        print("FG583_MUT missed: " + " ".join(missed + uncompiled), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
