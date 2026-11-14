#!/usr/bin/env python3
# Mutation run for the HF cache naming slice (S2).
# Each mutation edits the Goose port, regenerates the gate driver, runs it with
# `goose run` and compares with the native oracle. Any diff, run failure or
# missing output counts as caught. Usage: mutate_s2.py [lo hi]
from pathlib import Path
import subprocess
import sys

S = Path(__file__).resolve().parent.parent
SRC = S / "source" / "hf-cache-naming.goose"
ORIG = SRC.read_text()
G = "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"

M = [
    # ---- half 1: validators and the name mapping ---------------------------
    ("the repo length limit is 255",
     "if s.len == 0 || s.len > 256 {",
     "if s.len == 0 || s.len > 255 {"),
    ("a leading separator is accepted",
     "var special: bool = true;",
     "var special: bool = false;"),
    ("a repo id without a slash is accepted",
     "if slash != 1 {",
     "if slash > 1 {"),
    ("the token length floor is 36",
     "if t.len < 37 || t.len > 256 {",
     "if t.len < 36 || t.len > 256 {"),
    ("a 64-byte oid is rejected",
     "return h_hex_len(s, 64);",
     "return h_hex_len(s, 63);"),
    ("uppercase F is not hex",
     "(c >= 65 && c <= 70)",
     "(c >= 65 && c <= 69)"),
    # ---- half 2: paths, subpaths, refs, finalize ---------------------------
    ("the folder separator is a dot and a dash",
     """            out[p] = 45;
            out[p + 1] = 45;""",
     """            out[p] = 45;
            out[p + 1] = 46;"""),
    ("an empty name after the prefix is rejected",
     "if folder.len < 8 {",
     "if folder.len < 9 {"),
    ("the reverse separator is a single dash",
     "if folder[i] == 45 && i + 1 < folder.len && folder[i + 1] == 45 {",
     "if folder[i] == 45 && i + 1 < folder.len && folder[i + 1] == 46 {"),
    ("an absolute subpath stays inside",
     """fn h_subpath_ok(base: const u8[:], sub: const u8[:]) -> i64 {
    if h_is_abs(sub) != 0 {
        return 0;
    }""",
     """fn h_subpath_ok(base: const u8[:], sub: const u8[:]) -> i64 {
    if h_is_abs(sub) != 0 {
        return 1;
    }"""),
    ("dot-dot does not truncate the path",
     """                if sp > 0 {
                    sp = sp - 1;
                    p = starts[sp];
                }""",
     """                if sp > 0 {
                    sp = sp - 1;
                }"""),
    ("the blob redirect ignores an existing snapshot",
     "if oid.len != 0 && final_exists == 0 {",
     "if oid.len != 0 {"),
]


def run():
    subprocess.run(["python3", str(S / "tests" / "gen_s2.py")],
                   capture_output=True, text=True, timeout=120)
    try:
        r = subprocess.run([G, "run", str(S / "build" / "gate_s2.goose")],
                           capture_output=True, text=True, timeout=280, errors="replace")
    except subprocess.TimeoutExpired:
        return 1, "run timeout"
    if r.returncode != 0:
        tail = (r.stderr or r.stdout).strip().splitlines()
        return 1, "run failed: " + (tail[-1][:120] if tail else "no output")
    got = r.stdout
    want = (S / "build" / "native_s2.out").read_text()
    if got == want:
        return 0, "identical"
    diff = sum(1 for a, b in zip(got.splitlines(), want.splitlines()) if a != b)
    diff += abs(len(got.splitlines()) - len(want.splitlines()))
    return 1, f"{diff} differing lines"


lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
caught = 0
missed = []
try:
    for idx, (label, old, new) in enumerate(M[lo:hi], start=lo):
        if ORIG.count(old) != 1:
            print(f"[skip] {idx} {label}: anchor count {ORIG.count(old)}")
            continue
        SRC.write_text(ORIG.replace(old, new, 1))
        rc, detail = run()
        ok = rc != 0
        caught += ok
        if not ok:
            missed.append(idx)
        print(f'[{"caught" if ok else "MISSED"}] {idx} {label}: {detail}')
        SRC.write_text(ORIG)
finally:
    SRC.write_text(ORIG)
    rc, detail = run()
    print(f"baseline after restore: rc={rc} {detail}")
print(f"mutations {caught}/{hi-lo} caught (range {lo}:{hi}) missed={missed}")
