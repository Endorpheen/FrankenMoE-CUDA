#!/usr/bin/env python3
"""S2.1 mutation gate: 12 mutants of the config validator, two halves of six.

Each mutant is one textual change to exactly one of the three artefacts that
make up the switch:

    goose/cli/config-validate.goose   the decision chain (48 rules)
    common/fg-goose-config-bridge.cpp the seam (flat layout, precision, source map)
    runtime/core/src/config.cpp       the carrier (rule number -> English text)

The workflow per mutant is the whole point, so it is not shortcut:

    restore pristine -> apply one mutation -> rebuild at -O0 -> run the gate
    -> DETECTED / UNDETECTED -> restore pristine

A mutant is DETECTED when the gate exits non-zero, i.e. when at least one of its
four checks (accept/reject, text, rule number, pin) disagrees. Half A mutates
the shape of the decision chain (boundaries, rule order, NaN); half B mutates
the seam and the limits (source mapping, precision, text, number, constants).

After the last mutant the tree is restored, rebuilt and gated again: the
baseline must be 72/72 (66 cases + 6 order pins), failures=0.

The generated C is never committed and never kept mutated: it is regenerated
from the pristine module on every restore, and its SHA-256 is compared with the
value recorded at start.

Usage: python3 tests/config_mutate.py [--keep]
No model, no llama.cpp target, no GPU.
"""
from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GOOSE = os.environ.get("GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
BUILD = ROOT / "build-mut"
PRISTINE = BUILD / "pristine"
EVIDENCE = ROOT / "evidence" / "mutations.txt"
J = os.environ.get("J", "32")

MODULE = "goose/cli/config-validate.goose"
BRIDGE = "common/fg-goose-config-bridge.cpp"
CARRIER = "runtime/core/src/config.cpp"
SOURCES = (MODULE, BRIDGE, CARRIER)

UNIT = ROOT / "goose-gen" / "config-slice.goose"
GEN_C = ROOT / "goose-gen" / "config-slice.c"
GEN_H = ROOT / "goose-gen" / "config-slice.h"
COMPOSED = (
    "goose/model/arch-registry.goose",
    "goose/cli/config-validate.goose",
    "goose/cli/config-resolve.goose",
)

# --- the mutants -------------------------------------------------------------
#
# old/new are exact substrings; the script refuses a mutant whose anchor does
# not occur exactly once, so a shifted source cannot silently mutate the wrong
# line. "expect" names the case the failure is supposed to come from.

MUTATIONS = [
    # ---- half A: the shape of the decision chain -----------------------------
    dict(
        id="A1", half="A", kind="module", target=MODULE,
        what="cache_min_mb 1500 -> 1400 (pathological band widened)",
        old="const CFG_CACHE_MIN_MB: i64 = 1500;",
        new="const CFG_CACHE_MIN_MB: i64 = 1400;",
        expect="case 'cache in pathological band' (cache_mb=1499) and fuzz 1498",
    ),
    dict(
        id="A2", half="A", kind="module", target=MODULE,
        what="n_ubatch > n_ctx -> n_ubatch >= n_ctx (equality rejected)",
        old="    if i[3] > i[2] {",
        new="    if i[3] >= i[2] {",
        expect="case 'ubatch equal ctx is valid' (2048/2048)",
    ),
    dict(
        id="A3", half="A", kind="module", target=MODULE,
        what="n_predict <= 0 -> n_predict < 0 (zero accepted)",
        old="    if i[0] <= 0 {",
        new="    if i[0] < 0 {",
        expect="case 'n_predict zero'",
    ),
    dict(
        id="A4", half="A", kind="module", target=MODULE,
        what="rule order: CFG_R_CACHE_AUTO_BOTH moved before CFG_R_CACHE_BAND",
        old="""        // The pathological band: a cache below one token's working set thrashes.
        if i[18] > 0 && i[18] < CFG_CACHE_MIN_MB && i[31] == 0 {
            return CFG_R_CACHE_BAND;
        }
        if i[22] < 0 || i[22] > CFG_PREFETCH_MAX {
            return CFG_R_PREFETCH_RANGE;
        }
        if i[19] != 0 && i[18] > 0 {
            return CFG_R_CACHE_AUTO_BOTH;
        }""",
        new="""        if i[19] != 0 && i[18] > 0 {
            return CFG_R_CACHE_AUTO_BOTH;
        }
        // The pathological band: a cache below one token's working set thrashes.
        if i[18] > 0 && i[18] < CFG_CACHE_MIN_MB && i[31] == 0 {
            return CFG_R_CACHE_BAND;
        }
        if i[22] < 0 || i[22] > CFG_PREFETCH_MAX {
            return CFG_R_PREFETCH_RANGE;
        }""",
        expect="order pin 'cache band before auto conflict' (cache 1000 + cache_auto)",
    ),
    dict(
        id="A5", half="A", kind="module", target=MODULE,
        what="rule order: CFG_R_DROP_RANGE moved before CFG_R_DROP_CACHE",
        old="""        if f[3] > 0.0 && !(i[18] > 0 || i[19] != 0) {
            return CFG_R_DROP_CACHE;
        }
        // Negated inclusive range, so NaN (every comparison false) is rejected.
        if !(f[3] >= 0.0 && f[3] <= 1.0) {
            return CFG_R_DROP_RANGE;
        }""",
        new="""        // Negated inclusive range, so NaN (every comparison false) is rejected.
        if !(f[3] >= 0.0 && f[3] <= 1.0) {
            return CFG_R_DROP_RANGE;
        }
        if f[3] > 0.0 && !(i[18] > 0 || i[19] != 0) {
            return CFG_R_DROP_CACHE;
        }""",
        expect="order pin 'drop cache before drop range' (drop 5.0, no cache)",
    ),
    dict(
        id="A6", half="A", kind="module", target=MODULE,
        what="drop range: negated inclusive range -> positive range (NaN passes)",
        old="        if !(f[3] >= 0.0 && f[3] <= 1.0) {",
        new="        if f[3] < 0.0 || f[3] > 1.0 {",
        expect="case 'drop NaN' (NaN slips through every comparison)",
    ),
    # ---- half B: the seam, the texts and the limits --------------------------
    dict(
        id="B1", half="B", kind="bridge", target=BRIDGE,
        what="DraftSource::ngram mapped to 1 (MTP) instead of 2 (NGRAM)",
        old="    case DraftSource::ngram: return 2; // CFG_SRC_NGRAM",
        new="    case DraftSource::ngram: return 1; // CFG_SRC_NGRAM",
        expect="cases 'ngram rejects p_min', 'ngram min zero' (rule 19/21 become 18/…)",
    ),
    dict(
        id="B2", half="B", kind="bridge", target=BRIDGE,
        what="drop_cold_frac not narrowed: single-precision value plus a double epsilon",
        old="    fv[3] = narrow(cfg.moe.drop_cold_frac);",
        new="    fv[3] = static_cast<double>(cfg.moe.drop_cold_frac) + 1e-9;",
        expect="case 'drop at one' (1.0 becomes 1.000000001 > 1.0 -> rule 46)",
    ),
    dict(
        id="B3", half="B", kind="carrier", target=CARRIER,
        what="carrier text for rule 4 changed",
        old='    case 4:\n        return fail("n_ctx must be positive");',
        new='    case 4:\n        return fail("n_ctx must be >= 1");',
        expect="case 'n_ctx zero': text differs, message maps to 0 rules",
    ),
    dict(
        id="B4", half="B", kind="module", target=MODULE,
        what="rule number: expert_hot_slots returns CFG_R_VRAM_NEG (11) instead of CFG_R_SLOTS_NEG (12)",
        old="    if i[6] < 0 {\n        return CFG_R_SLOTS_NEG;\n    }",
        new="    if i[6] < 0 {\n        return CFG_R_VRAM_NEG;\n    }",
        expect="case 'slots negative': rule 11 instead of 12, and the vram text",
    ),
    dict(
        id="B5", half="B", kind="module", target=MODULE,
        what="io_threads_max 8 -> 7",
        old="const CFG_IO_THREADS_MAX: i64 = 8;",
        new="const CFG_IO_THREADS_MAX: i64 = 7;",
        expect="fuzz with io_threads=8 (accepted natively, rejected by the module)",
    ),
    dict(
        id="B6", half="B", kind="module", target=MODULE,
        what="route_ahead_max 8 -> 9",
        old="const CFG_ROUTE_AHEAD_MAX: i64 = 8;",
        new="const CFG_ROUTE_AHEAD_MAX: i64 = 9;",
        expect="case 'route_ahead range' (9 accepted by the module, rejected natively)",
    ),
]

# Not counted in the two halves: the third member of the limit group, kept as a
# separate probe so the 12-slot budget stays 6/6 while prefetch is still proven.
EXTRA = [
    dict(
        id="X1", half="extra", kind="module", target=MODULE,
        what="prefetch_layers_max 8 -> 7",
        old="const CFG_PREFETCH_MAX: i64 = 8;",
        new="const CFG_PREFETCH_MAX: i64 = 7;",
        expect="fuzz with prefetch_layers=8 (accepted natively, rejected by the module)",
    ),
]


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    h.update(path.read_bytes())
    return h.hexdigest()


def run(cmd, cwd=ROOT, capture=True):
    return subprocess.run(cmd, cwd=str(cwd), capture_output=capture, text=True)


def fail(msg: str) -> "NoReturn":  # type: ignore[name-defined]
    print(f"[SETUP-FAIL] {msg}")
    sys.exit(2)


def restore_sources():
    # copy2 keeps the pristine mtime, which is older than the object built from
    # the mutant — make would then keep the mutant object and every later mutant
    # (and the final baseline) would be measured against the wrong binary. The
    # timestamp is bumped so a restore always rebuilds. This bit the first run of
    # this script and is the same class of bug as a stale generated C.
    now = time.time()
    for rel in SOURCES:
        dst = ROOT / rel
        shutil.copy2(PRISTINE / rel, dst)
        os.utime(dst, (now, now))


def regenerate() -> str:
    """Compose + standalone from the current module. Returns the C hash."""
    r = run([sys.executable, "scripts/compose-goose.py", str(UNIT.relative_to(ROOT)),
             *COMPOSED])
    if r.returncode != 0:
        fail(f"compose failed: {r.stdout}{r.stderr}")
    r = run([GOOSE, "--standalone", "-o", str(GEN_C.relative_to(ROOT)),
             "--header", str(GEN_H.relative_to(ROOT)), str(UNIT.relative_to(ROOT))])
    if r.returncode != 0:
        fail(f"standalone failed: {r.stdout}{r.stderr}")
    # Regeneration is deterministic; the timestamp is not, and a stale object
    # must never let a mutant survive as "up to date".
    now = time.time()
    os.utime(GEN_C, (now, now))
    os.utime(GEN_H, (now, now))
    return sha256(GEN_C)


def build() -> tuple[bool, str]:
    r = run(["cmake", "--build", str(BUILD), "-j", J, "--target", "fg-config-smoke"])
    return r.returncode == 0, r.stdout + r.stderr


def gate() -> tuple[int, list[str]]:
    r = run([str(BUILD / "fg-config-smoke")])
    lines = (r.stdout + r.stderr).splitlines()
    return r.returncode, lines


def first_failure(lines: list[str]) -> str:
    for line in lines:
        if line.startswith("[FAIL]"):
            return line
    return lines[-1] if lines else "<no output>"


def summary_line(lines: list[str]) -> str:
    for line in reversed(lines):
        if line.startswith("FG632_SMOKE"):
            return line
    return "<no FG632_SMOKE line>"


def apply_mutation(m: dict) -> None:
    path = ROOT / m["target"]
    text = path.read_text(encoding="utf-8")
    n = text.count(m["old"])
    if n != 1:
        fail(f"{m['id']}: anchor occurs {n} times in {m['target']} (must be exactly 1)")
    path.write_text(text.replace(m["old"], m["new"], 1), encoding="utf-8")


def run_mutant(m: dict, pristine_c_sha: str) -> dict:
    restore_sources()
    regenerate()  # from the pristine module
    apply_mutation(m)
    if m["kind"] == "module":
        if regenerate() == pristine_c_sha:
            fail(f"{m['id']}: the mutation did not change the generated C")
    ok, log = build()
    if not ok:
        fail(f"{m['id']}: build failed\n{log[-2000:]}")
    rc, lines = gate()
    detected = rc != 0
    return {
        "id": m["id"],
        "half": m["half"],
        "kind": m["kind"],
        "target": m["target"],
        "what": m["what"],
        "expect": m["expect"],
        "detected": detected,
        "first": first_failure(lines),
        "summary": summary_line(lines),
    }


def main() -> int:
    keep = "--keep" in sys.argv

    BUILD.mkdir(parents=True, exist_ok=True)
    (PRISTINE / "goose/cli").mkdir(parents=True, exist_ok=True)
    (PRISTINE / "common").mkdir(parents=True, exist_ok=True)
    (PRISTINE / "runtime/core/src").mkdir(parents=True, exist_ok=True)
    for rel in SOURCES:
        shutil.copy2(ROOT / rel, PRISTINE / rel)

    cfg_log = run(["cmake", "-S", ".", "-B", str(BUILD), "-DCMAKE_BUILD_TYPE=Release",
                   "-DBUILD_SHARED_LIBS=ON", "-DFG_GOOSE_CONFIG=ON", "-DFG_GATE_OPT=-O0"])
    if cfg_log.returncode != 0:
        fail(f"configure failed\n{cfg_log.stdout}{cfg_log.stderr}")

    restore_sources()
    pristine_c_sha = regenerate()
    ok, log = build()
    if not ok:
        fail(f"pristine baseline build failed\n{log[-2000:]}")
    rc, lines = gate()
    if rc != 0:
        fail(f"pristine baseline gate is not green: {summary_line(lines)}")
    baseline = summary_line(lines)

    out: list[str] = []
    out.append("S2.1 mutation gate — Goose config validator (cfg_validate)")
    out.append(f"work tree   : {ROOT}")
    out.append(f"build       : {BUILD.name} (FG_GOOSE_CONFIG=ON, FG_GATE_OPT=-O0)")
    out.append(f"pristine    : goose-gen/config-slice.c sha256 {pristine_c_sha}")
    out.append(f"baseline    : {baseline}")
    out.append("")
    out.append("A mutant is DETECTED when the gate exits non-zero: accept/reject, error text,")
    out.append("rule number and the pin are all checked against bmoe::validate_native().")
    out.append("")

    results = []
    for group, name in ((MUTATIONS, "mutants"), (EXTRA, "extra probe")):
        out.append(f"=== {name} ===")
        for m in group:
            res = run_mutant(m, pristine_c_sha)
            results.append(res)
            verdict = "DETECTED  " if res["detected"] else "UNDETECTED"
            out.append(f"[{res['id']}] {res['half']:<5} {res['kind']:<7} {res['what']}")
            out.append(f"      expect   : {res['expect']}")
            out.append(f"      verdict  : {verdict}")
            out.append(f"      caught by: {res['first']}")
            out.append(f"      gate     : {res['summary']}")
            print(f"[{res['id']}] {verdict} {res['what']}")
        out.append("")

    # Restore, and prove the baseline is back: sources byte-identical, generated
    # C regenerated to the recorded hash, gate green again.
    restore_sources()
    restored_c_sha = regenerate()
    ok, log = build()
    if not ok:
        fail(f"restore build failed\n{log[-2000:]}")
    rc, lines = gate()
    restored = summary_line(lines)
    out.append("=== restore ===")
    out.append(f"generated C sha256 {restored_c_sha} {'MATCHES pristine' if restored_c_sha == pristine_c_sha else 'DIFFERS from pristine'}")
    out.append(f"baseline after restore: {restored}")
    out.append(f"72/72 (66 cases + 6 order pins): {'PASS' if rc == 0 else 'FAIL'}")

    counted = [r for r in results if r["half"] in ("A", "B")]
    a = [r for r in counted if r["half"] == "A"]
    b = [r for r in counted if r["half"] == "B"]
    extra = [r for r in results if r["half"] == "extra"]
    out.append("")
    out.append(f"half A: caught {sum(r['detected'] for r in a)}/{len(a)}")
    out.append(f"half B: caught {sum(r['detected'] for r in b)}/{len(b)}")
    out.append(f"counted: caught {sum(r['detected'] for r in counted)}/{len(counted)}")
    for r in extra:
        out.append(f"extra {r['id']}: {'DETECTED' if r['detected'] else 'UNDETECTED'} ({r['what']})")
    out.append("")
    out.append("| id | half | target | mutation | expected catch | caught by |")
    out.append("|---|---|---|---|---|---|")
    for r in results:
        first = r["first"].replace("|", "\\|")
        if len(first) > 150:
            first = first[:147] + "..."
        out.append(f"| {r['id']} | {r['half']} | {Path(r['target']).name} | {r['what']} | {r['expect']} | {first} |")

    text = "\n".join(out) + "\n"
    EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
    EVIDENCE.write_text(text, encoding="utf-8")
    print()
    print("\n".join(out[-8:]))
    print(f"evidence: {EVIDENCE.relative_to(ROOT)}")
    print(f"sha256   : {sha256(EVIDENCE)}")

    if not keep:
        pass  # the build directory stays, it is an artefact like the others
    return 0 if all(r["detected"] for r in counted) and rc == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
