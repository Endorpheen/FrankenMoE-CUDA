#!/usr/bin/env python3
"""Differential gate for the SLRU policy: Goose against the native C++.

The native moe_cache_slru (llama-moe-cache.cpp:66-167) is the reference. This harness drives both
implementations with the same random requests and requires identical fills, identical hit counts,
identical refusal decisions and an identical final cache state.

Why all four: a policy can produce the right fills and still leave the lists inconsistent, which only
shows up on a later request. So the state is compared too, and the sequence is long enough that a
divergence surfaces.

Usage:
    python3 tests/gate.py [--cases N] [--seed S]
Exit code 0 only when every case matches.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import random
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PIN = ROOT / "runtime" / "compiler" / "current.json"
HARNESS = Path(__file__).resolve().parent / "harness.c"


class Slru:
    """The native implementation, transcribed to Python with the same semantics."""

    def __init__(self, n_layer, n_expert, n_slots):
        self.n_expert = n_expert
        self.n_slots = n_slots
        self.maps = [[-1] * n_expert for _ in range(n_layer)]
        self.key_of = [-1] * n_slots
        self.prev = [s - 1 for s in range(n_slots)]
        self.next = [s + 1 if s + 1 < n_slots else -1 for s in range(n_slots)]
        self.head = [0, -1]
        self.tail = [n_slots - 1, -1]
        self.prot = [0] * n_slots
        self.n_prot = 0
        self.seen = [0] * n_expert
        self.gen = 0

    def _move(self, s, p):
        frm = self.prot[s]
        if frm == p and s == self.tail[p]:
            return
        if self.prev[s] >= 0:
            self.next[self.prev[s]] = self.next[s]
        else:
            self.head[frm] = self.next[s]
        if self.next[s] >= 0:
            self.prev[self.next[s]] = self.prev[s]
        else:
            self.tail[frm] = self.prev[s]

        self.prev[s] = self.tail[p]
        self.next[s] = -1
        if self.tail[p] >= 0:
            self.next[self.tail[p]] = s
        else:
            self.head[p] = s
        self.tail[p] = s
        self.prot[s] = p
        self.n_prot += p - frm

    def _touch(self, s):
        self._move(s, 1)
        if self.n_prot > self.n_slots // 2:
            self._move(self.head[1], 0)

    def _victim(self, il):
        for seg in (0, 1):
            s = self.head[seg]
            while s >= 0:
                key = self.key_of[s]
                if key < 0 or key // self.n_expert != il or self.seen[key % self.n_expert] != self.gen:
                    return s
                s = self.next[s]
        return -1

    def plan(self, il, ids):
        self.gen += 1
        if self.gen == 1 << 32:
            self.seen = [0] * self.n_expert
            self.gen = 1
        uniq = []
        for e in ids:
            if self.seen[e] != self.gen:
                self.seen[e] = self.gen
                uniq.append(e)
        if len(uniq) > self.n_slots:
            return None, 0

        slots = self.maps[il]
        n_hit = 0
        for e in uniq:
            if slots[e] >= 0:
                self._touch(slots[e])
                n_hit += 1
        uniq.sort()
        fills = []
        for e in uniq:
            if slots[e] >= 0:
                continue
            s = self._victim(il)
            if s < 0:
                return None, 0
            key = self.key_of[s]
            if key >= 0:
                self.maps[key // self.n_expert][key % self.n_expert] = -1
            self.key_of[s] = il * self.n_expert + e
            slots[e] = s
            self._move(s, 0)
            fills.append((e, s))
        return fills, n_hit


def compile_harness(tmp: Path) -> Path:
    pin = json.loads(PIN.read_text())
    goose = Path(pin["compiler"])
    if hashlib.sha256(goose.read_bytes()).hexdigest() != pin["sha256"]:
        sys.exit("compiler pin mismatch")

    module = ROOT / "goose" / "cpu" / "goose-moe-cache-slru.goose"
    gen = tmp / "slru.c"
    hdr = tmp / "slru.h"
    q = subprocess.run([str(goose), "--standalone", "-o", str(gen), "--header", str(hdr), str(module)],
                       capture_output=True, text=True)
    if q.returncode:
        sys.exit(f"goose failed:\n{q.stderr[:1200]}")

    exe = tmp / "gate"
    q = subprocess.run(["gcc-13", "-O1", "-std=c11", "-w", "-I", str(tmp),
                        str(HARNESS), str(gen), "-o", str(exe)],
                       capture_output=True, text=True)
    if q.returncode:
        sys.exit(f"gcc failed:\n{q.stderr[:1200]}")
    return exe


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=int, default=200)
    ap.add_argument("--seed", type=int, default=1234)
    a = ap.parse_args()

    # The C harness prints one line per request so the two implementations can be compared
    # without marshalling structures; the Python side prints the same format.
    tmpdir = tempfile.mkdtemp(prefix="slru-gate.")
    exe = compile_harness(Path(tmpdir))
    print(f"harness: {exe}")

    rng = random.Random(a.seed)
    bad = 0
    for c in range(a.cases):
        n_layer = rng.randint(1, 4)
        n_expert = rng.randint(2, 24)
        n_slots = rng.randint(1, n_expert + 4)
        n_req = rng.randint(1, 30)
        requests = []
        for _ in range(n_req):
            il = rng.randrange(n_layer)
            k = rng.randint(1, min(n_expert, n_slots + 3))
            ids = [rng.randrange(n_expert) for _ in range(k)]
            requests.append((il, ids))

        ref = Slru(n_layer, n_expert, n_slots)
        ref_lines = []
        for il, ids in requests:
            fills, n_hit = ref.plan(il, ids)
            if fills is None:
                ref_lines.append("REFUSE")
            else:
                parts = " ".join(f"{e}:{s}" for e, s in fills)
                ref_lines.append(f"F {parts} H{n_hit}".replace("  ", " ").strip())
        ref_lines.append("STATE " + " ".join(
            f"{ref.key_of[s]},{ref.prev[s]},{ref.next[s]},{ref.prot[s]}" for s in range(n_slots))
            + f" HEAD {ref.head[0]},{ref.head[1]} TAIL {ref.tail[0]},{ref.tail[1]} NP {ref.n_prot}")

        case = {"n_layer": n_layer, "n_expert": n_expert, "n_slots": n_slots,
                "requests": [[il, ids] for il, ids in requests]}
        out = subprocess.run([str(exe)], input=json.dumps(case), capture_output=True, text=True)
        if out.returncode:
            print(f"case {c}: harness failed\n{out.stderr[:600]}")
            bad += 1
            continue
        got_lines = [" ".join(l.split()) for l in out.stdout.strip().splitlines() if l]
        if got_lines != ref_lines:
            bad += 1
            if bad <= 3:
                print(f"case {c} MISMATCH (layer={n_layer} expert={n_expert} slots={n_slots})")
                for i, (g, r) in enumerate(zip(got_lines, ref_lines)):
                    if g != r:
                        print(f"  line {i}\n   goose {g[:150]}\n   ref   {r[:150]}")
                        break
                if len(got_lines) != len(ref_lines):
                    print(f"  line count goose={len(got_lines)} ref={len(ref_lines)}")

    shutil.rmtree(tmpdir, ignore_errors=True)
    print(f"slru gate: {a.cases - bad}/{a.cases} cases match")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
