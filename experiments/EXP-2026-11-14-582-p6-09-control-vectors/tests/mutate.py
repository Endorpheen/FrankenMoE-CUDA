#!/usr/bin/env python3
"""Mutation runner for the P6-09 S3 control-vector gate (EXP578).

Each mutation is applied to a fresh copy of source/cvec.goose, then
tests/build.sh is invoked with FG578_SRC pointing at the copy and a separate
FG578_OUT. A mutation counts as caught when the build/run exits non-zero
(case failure, abort on an out-of-bounds access, or a compile error).

Usage:
    tests/mutate.py                 # both halves, all 16
    tests/mutate.py --half A        # state/validation half (8)
    tests/mutate.py --half B        # copy/arithmetic half (8)
    tests/mutate.py --only B4       # one mutation

This script was NOT executed in the preparation round (no heavy builds).
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
        print("FG578_MUT nothing selected", file=sys.stderr)
        return 2

    tmp = tempfile.mkdtemp(prefix="fg578-mut-")
    caught = 0
    missed = []

    for m in chosen:
        old = m["old"]
        new = m["new"]
        count = base.count(old)
        if count != 1:
            print(f"FG578_MUT {m['id']} bad pattern occurrences={count}", file=sys.stderr)
            missed.append(m["id"])
            continue

        path = os.path.join(tmp, f"cvec-{m['id']}.goose")
        with open(path, "w", encoding="utf-8") as f:
            f.write(base.replace(old, new, 1))

        env = dict(os.environ)
        env["FG578_SRC"] = path
        env["FG578_OUT"] = os.path.join(ROOT, "build-mut", m["id"])

        proc = subprocess.run(
            ["bash", BUILD],
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        ok = proc.returncode != 0
        if ok:
            caught += 1
        else:
            missed.append(m["id"])
            tail = "\n".join(proc.stdout.strip().splitlines()[-6:])
            print(f"FG578_MUT {m['id']} MISSED half={m.get('half')} note={m.get('note')}")
            print(tail)

    shutil.rmtree(tmp, ignore_errors=True)

    print(f"FG578_MUT half={args.half} caught={caught}/{len(chosen)} total={caught}/{len(chosen)}")
    if missed:
        print("FG578_MUT missed: " + " ".join(missed), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
