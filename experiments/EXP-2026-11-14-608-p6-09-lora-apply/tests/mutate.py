#!/usr/bin/env python3
"""Mutation runner for the P6-09 S2 gate (EXP-2026-11-14-605).

Each mutation is applied to a fresh copy of source/lora-apply.goose. The copy
must still pass `goose check` (a mutation that does not compile proves nothing
about the gate), the mutated runtime is then built at -O0 and the gate must
report failures. A run that does not reach the gate line at all is counted as
"not run", not as caught.

Usage:
    python3 tests/mutate.py              # both halves, all 12
    python3 tests/mutate.py --half A     # selection and scale (6)
    python3 tests/mutate.py --half B     # geometry, budget, cleanup (6)
    python3 tests/mutate.py --only B4    # one mutation
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SPEC = os.path.join(HERE, "mutations.json")
BUILD = os.path.join(HERE, "build.sh")
GOOSE = os.environ.get("FG_GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

GATE_LINE = re.compile(r"FG605_LORA cases=(\d+) values=(\d+) failures=(\d+)")


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
        print("FG605_MUT nothing selected", file=sys.stderr)
        return 2

    tmp = tempfile.mkdtemp(prefix="fg605-mut-")
    caught = 0
    missed = []
    not_run = []

    for m in chosen:
        old = m["old"]
        new = m["new"]
        count = base.count(old)
        if count != 1:
            print(f"FG605_MUT {m['id']} bad pattern occurrences={count}", file=sys.stderr)
            missed.append(m["id"])
            continue

        path = os.path.join(tmp, f"lora-apply-{m['id']}.goose")
        with open(path, "w", encoding="utf-8") as f:
            f.write(base.replace(old, new, 1))

        check = subprocess.run(
            [GOOSE, "check", path],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if check.returncode != 0:
            print(f"FG605_MUT {m['id']} DOES NOT COMPILE half={m.get('half')}")
            print("\n".join(check.stdout.strip().splitlines()[-6:]))
            not_run.append(m["id"])
            continue

        env = dict(os.environ)
        env["FG_SRC"] = path
        env["FG_OUT"] = os.path.join(ROOT, "build-mut", m["id"])

        proc = subprocess.run(
            [os.environ.get("FG_BASH", "bash"), BUILD, "mutate"],
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        hit = GATE_LINE.search(proc.stdout)
        if hit is not None and int(hit.group(3)) > 0:
            caught += 1
            print(f"FG605_MUT {m['id']} caught half={m.get('half')} "
                  f"failures={hit.group(3)} note={m.get('note')}")
        elif hit is not None:
            missed.append(m["id"])
            print(f"FG605_MUT {m['id']} MISSED half={m.get('half')} note={m.get('note')}")
        else:
            not_run.append(m["id"])
            tail = "\n".join(proc.stdout.strip().splitlines()[-6:])
            print(f"FG605_MUT {m['id']} DID NOT RUN half={m.get('half')} note={m.get('note')}")
            print(tail)

    total = len(chosen)
    print(f"FG605_MUT halo={args.half} caught={caught} total={total} missed={len(missed)} not_run={len(not_run)}")
    if missed:
        print("FG605_MUT missed: " + " ".join(missed), file=sys.stderr)
    if not_run:
        print("FG605_MUT not_run: " + " ".join(not_run), file=sys.stderr)
    return 0 if caught == total else 1


if __name__ == "__main__":
    sys.exit(main())
