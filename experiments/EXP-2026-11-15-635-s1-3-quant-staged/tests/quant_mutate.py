#!/usr/bin/env python3
"""Mutation run for the staged S1.3 seam: twelve mutations of the Goose port.

The rules the repository uses for a port gate apply unchanged: the run is
manual, one mutation at a time, the mutated module is compiled at -O0
(`-DFG_GOOSE_QUANT_O0=ON`) while the rest of the tree stays at Release, and a
mutation counts as caught only if a gate goes red.

Each mutation is a one-site edit of a staged Goose module:

    base copy (canon + staged patches)
      -> apply one mutation
      -> compose-quant.py -> goose --standalone
      -> rebuild the ON tree with the Goose object at -O0
      -> run fg-goose-quant-smoke and fg-goose-quant-arms
      -> DETECTED if either gate fails

Two halves of six, each half covering distinct mechanisms. The last step
restores the base copy and proves both gates are green again.

Usage: python3 tests/quant_mutate.py
Writes: evidence/q-mutations.txt
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GOOSE = Path(os.environ.get(
    "GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"))
BUILD = ROOT / "build-mut"
BASE = ROOT / "goose" / "model-base"
STAGED = ROOT / "goose" / "model-staged"
EVID = ROOT / "evidence" / "q-mutations.txt"
J = os.environ.get("J", "32")

PATCHES = ["patches/quant-dequant-signed-zero.diff"]

# id, half, file, old, new, mechanism
MUTATIONS = [
    # ---- half A ----------------------------------------------------------
    ("A1", "A", "quant-rows.goose",
     "let d = mx / -8.0;",
     "let d = mx / -7.0;",
     "quantization level: q4_0 scale divisor -8 -> -7"),

    ("A2", "A", "quant-dequant.goose",
     """                if t == T_Q4_0 {
                    out[i * QK + j] = (x0 as f32) * d;
                    out[i * QK + j + QK / 2] = (x1 as f32) * d;
                } else {""",
     """                if t == T_Q4_0 {
                    out[i * QK + j] = (x0 as f32) * d + m;
                    out[i * QK + j + QK / 2] = (x1 as f32) * d + m;
                } else {""",
     "sign of zero: q4_0 dequant adds the (zero) min again"),

    ("A3", "A", "quant-dequant.goose",
     "        return -v;",
     "        return 0.0 - v;",
     "sign of zero: half decode returns 0.0 - v"),

    ("A4", "A", "quant-dequant.goose",
     """            var m: f32 = 0.0;
            if t == T_Q4_1 {
                m = dq_half(dq_u16(src, o + 2));
            }""",
     """            var m: f32 = 0.0;
            if t == T_Q4_1 {
                m = 0.5 * dq_half(dq_u16(src, o + 2));
            }""",
     "min offset: q4_1 dequant halves the min"),

    ("A5", "A", "quant-rows.goose",
     "st_u8(yb + 2 + (j as u64), qz_u8(qz_round_f2i32(x0)));",
     "st_u8(yb + 2 + (j as u64), qz_u8(qz_f2i32(x0)));",
     "tail rounding: q8_0 quantizer truncates instead of rounding"),

    ("A6", "A", "quant-rows.goose",
     "        let y = dst + (r * rs) as u64;",
     "        let y = dst + (r * n_per_row) as u64;",
     "block boundary: row stride uses n_per_row, not row_size"),

    # ---- half B ----------------------------------------------------------
    ("B1", "B", "quant-rows2.goose",
     "            let isc = -32.0 / max_scale;",
     "            let isc = -31.0 / max_scale;",
     "super-scale: q3_K scale encoding -32 -> -31"),

    ("B2", "B", "quant-weighted.goose",
     """            st_f32(wa + (j as u64) * 4, ld_f32(qw + (j as u64) * 4) * sqrt32(sigma2 + xv * xv));
            j += 1 as i64;
        }
        let d = make_qx(32, 8, xb, L, 1, wa);""",
     """            st_f32(wa + (j as u64) * 4, sqrt32(sigma2 + xv * xv));
            j += 1 as i64;
        }
        let d = make_qx(32, 8, xb, L, 1, wa);""",
     "imatrix: q4_0 weighted arm ignores quant_weights"),

    ("B3", "B", "quant-weighted.goose",
     """        let sigma2 = sumx2 / (QK_K as f32);
        j = 0 as i64;
        while j < 16 {
            let qw = qw0 + ((i * QK_K + 16 * j) as u64) * 4;""",
     """        let sigma2 = 2.0 * sumx2 / (QK_K as f32);
        j = 0 as i64;
        while j < 16 {
            let qw = qw0 + ((i * QK_K + 16 * j) as u64) * 4;""",
     "imatrix: q2_K sigma2 factor 1 -> 2"),

    ("B4", "B", "quant-rows2.goose",
     "        let d = iq4_nl_block(xb, L, wa, 7);",
     "        let d = iq4_nl_block(xb, L, wa, 6);",
     "IQ neighbour search: iq4_nl trial count 7 -> 6"),

    ("B5", "B", "quant-rows2.goose",
     "            let sc = make_qkx2(16, 3, xb + ((j * 16) as u64) * 4, wa, L + (j * 16) as u64, laux, mnbuf,",
     "            let sc = make_qkx2(16, 4, xb + ((j * 16) as u64) * 4, wa, L + (j * 16) as u64, laux, mnbuf,",
     "quantization level: q2_K level count 3 -> 4"),

    ("B6", "B", "quant-dequant2.goose",
     "let q1 = (((u8at(src, ql + l) & 15) | (((u8at(src, qh + l) >> 0) & 3) << 4)) - 32) as f32;",
     "let q1 = (((u8at(src, ql + l) & 15) | (((u8at(src, qh + l) >> 0) & 3) << 4)) - 31) as f32;",
     "dequant zero point: q6_K offset -32 -> -31"),
]


def run(cmd, cwd=ROOT, capture=True):
    res = subprocess.run(cmd, cwd=str(cwd), shell=isinstance(cmd, str),
                         stdout=subprocess.PIPE if capture else None,
                         stderr=subprocess.STDOUT if capture else None,
                         text=True)
    return res.returncode, (res.stdout or "")


def prepare_base():
    if BASE.exists():
        shutil.rmtree(BASE)
    shutil.copytree(ROOT / "goose" / "model", BASE)
    for p in PATCHES:
        rc, out = run("patch %s < %s" % (BASE / "quant-dequant.goose", p))
        if rc != 0:
            raise SystemExit("patch failed: %s\n%s" % (p, out))


def compose_and_build(label):
    """model-staged -> goose-gen -> C -> build; returns (ok, output)."""
    if STAGED.exists():
        shutil.rmtree(STAGED)
    shutil.copytree(BASE, STAGED)

    for m in MUTATIONS_BY_ID.get(label, []):
        path = STAGED / m["file"]
        text = path.read_text()
        if text.count(m["old"]) != 1:
            return False, "mutation site %s not unique in %s (%d)" % (
                m["id"], m["file"], text.count(m["old"]))
        path.write_text(text.replace(m["old"], m["new"]))

    rc, out = run([sys.executable, "scripts/compose-quant.py",
                   "goose/model-staged", "goose-gen/quant-stack.goose"])
    if rc != 0:
        return False, out
    rc, out = run([str(GOOSE), "check", "goose-gen/quant-stack.goose"])
    if rc != 0:
        return False, "goose check failed:\n" + out
    rc, out = run([str(GOOSE), "--standalone",
                   "-o", "goose-gen/quant-stack.c",
                   "--header", "goose-gen/quant-stack.h",
                   "goose-gen/quant-stack.goose"])
    if rc != 0:
        return False, "goose --standalone failed:\n" + out
    rc, out = run(["cmake", "--build", "build-mut", "-j", J,
                   "--target", "fg-goose-quant-smoke", "fg-goose-quant-arms"])
    if rc != 0:
        return False, "build failed:\n" + out[-4000:]
    return True, ""


def gate(name):
    """(failures, detail, exit code) for one gate binary."""
    exe = BUILD / name
    rc, out = run([str(exe)])
    fails = 0
    for line in out.splitlines():
        if "failures=" in line:
            part = line.split("failures=")[1].split()[0]
            try:
                fails = max(fails, int(part))
            except ValueError:
                pass
    return fails, out.strip().splitlines()[-1] if out.strip() else "(no output)", rc


MUTATIONS_BY_ID = {}
for mid, half, f, old, new, mech in MUTATIONS:
    MUTATIONS_BY_ID.setdefault(mid, []).append(
        dict(id=mid, half=half, file=f, old=old, new=new, mechanism=mech))


def main():
    prepare_base()

    # Configure and build the mutation tree once: ON, Goose object at -O0.
    rc, out = run(["cmake", "-S", ".", "-B", "build-mut",
                   "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_SHARED_LIBS=ON",
                   "-DFG_GOOSE_QUANT=ON", "-DFG_GOOSE_QUANT_O0=ON"])
    if rc != 0:
        raise SystemExit("configure failed:\n" + out)

    ok, err = compose_and_build("baseline")
    if not ok:
        raise SystemExit("baseline build failed:\n" + err)
    base_smoke = gate("fg-goose-quant-smoke")
    base_arms = gate("fg-goose-quant-arms")
    print("baseline: smoke %s | arms %s" % (base_smoke[1], base_arms[1]))
    if base_smoke[0] or base_arms[0] or base_smoke[2] or base_arms[2]:
        raise SystemExit("baseline gates are not green; mutations would be meaningless")

    lines = []
    lines.append("S1.3 mutation run — the Goose quant port inside a CPU-only copy")
    lines.append("module compiled at -O0 (FG_GOOSE_QUANT_O0=ON), tree at Release")
    lines.append("gates: fg-goose-quant-smoke (seam) and fg-goose-quant-arms (module arms)")
    lines.append("")
    lines.append("baseline      smoke: %s" % base_smoke[1])
    lines.append("baseline      arms : %s" % base_arms[1])
    lines.append("")
    header = "%-4s %-4s %-22s %-58s %s" % ("id", "half", "file", "mechanism", "result")
    lines.append(header)
    lines.append("-" * len(header))

    caught = {"A": 0, "B": 0}
    total = {"A": 0, "B": 0}

    for mid, half, f, old, new, mech in MUTATIONS:
        total[half] += 1
        ok, err = compose_and_build(mid)
        if not ok:
            lines.append("%-4s %-4s %-22s %-58s INVALID" % (mid, half, f, mech))
            print("INVALID %s: %s" % (mid, err))
            continue
        s_fails, s_last, s_rc = gate("fg-goose-quant-smoke")
        a_fails, a_last, a_rc = gate("fg-goose-quant-arms")
        detected = (s_fails > 0 or a_fails > 0 or s_rc != 0 or a_rc != 0)
        verdict = "DETECTED" if detected else "UNDETECTED"
        if detected:
            caught[half] += 1
        detail = "smoke rc=%d fails=%d; arms rc=%d fails=%d" % (
            s_rc, s_fails, a_rc, a_fails)
        lines.append("%-4s %-4s %-22s %-58s %-10s %s" % (
            mid, half, f, mech, verdict, detail))
        print("%s %s %s" % (mid, verdict, detail))
        for tag, txt in (("smoke", s_last), ("arms", a_last)):
            if detected:
                lines.append("     %-5s %s" % (tag, txt))

    lines.append("")
    lines.append("half A: caught %d/%d" % (caught["A"], total["A"]))
    lines.append("half B: caught %d/%d" % (caught["B"], total["B"]))
    print("half A: caught %d/%d" % (caught["A"], total["A"]))
    print("half B: caught %d/%d" % (caught["B"], total["B"]))

    # Restore: base copy, rebuild, both gates green again.
    ok, err = compose_and_build("baseline")
    if not ok:
        raise SystemExit("restore failed:\n" + err)
    r_smoke = gate("fg-goose-quant-smoke")
    r_arms = gate("fg-goose-quant-arms")
    lines.append("")
    lines.append("after restore smoke: %s" % r_smoke[1])
    lines.append("after restore arms : %s" % r_arms[1])
    print("after restore: smoke %s | arms %s" % (r_smoke[1], r_arms[1]))

    EVID.write_text("\n".join(lines) + "\n")
    print("wrote %s" % EVID)

    all_ok = (caught["A"] == total["A"] and caught["B"] == total["B"]
              and not r_smoke[0] and not r_arms[0]
              and not r_smoke[2] and not r_arms[2])
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
