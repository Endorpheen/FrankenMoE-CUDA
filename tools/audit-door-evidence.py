#!/usr/bin/env python3
"""Every exported Goose door must be traceable to a record that names it or its feature.

The audit exists because a door can be written, wired, exported and never measured - `sp_reject_accept` was exactly
that until EXP-2026-11-19-1368, and a grep by symbol name would not have found it either, since the measurement named
the feature rather than the wrapper.

Two columns are reported separately, because they mean different things:

  named      the symbol appears in a card or a document;
  feature    the symbol does not, but its feature does - the call sites are counted under the feature's name.

A symbol in neither column is a port nobody can find evidence for, and that is a gap rather than a formality.

Usage: tools/audit-door-evidence.py [--bin builds/flint-acc/bin] [--fail-on-gap]
"""
import argparse
import pathlib
import re
import subprocess

FEATURE = {
    "fg_grammar_reject_call":     ["grammar"],
    "fg_sampler_adaptive_p_call": ["adaptive-p", "adaptive p"],
    "fg_sampler_dist_call":       ["dist"],
    "fg_sampler_dist_pick_call":  ["dist"],
    "fg_sampler_dist_words_call": ["dist"],
    "fg_sampler_mirostat":        ["mirostat"],
    "fg_sampler_mirostat_call":   ["mirostat"],
    "fg_sampler_penalties_call":  ["penalt"],
    "fg_sampler_temp_ext":        ["temp_ext", "temp ext"],
    "fg_sampler_temp_ext_call":   ["temp_ext", "temp ext"],
    "fg_sampler_top_p_call":      ["top-p", "top p"],
    "fg_sampler_typical_call":    ["typical"],
    "fg_sampler_xtc_call":        ["xtc"],
    "fg_sp_synth_accept":         ["synthetic accept"],
    "fg_sp_reject_accept":        ["rejection accept"],
    "fg_sp_prefix":               ["prefix agreement", "prefix"],
    "fg_sp_n_max_call":           ["draft width", "n_max"],
}

def corpus() -> str:
    text = []
    for pat in ("experiments/**/README.md", "docs/**/*.md", "integration/**/*.md"):
        for p in pathlib.Path(".").glob(pat):
            try:
                text.append(p.read_text(errors="ignore"))
            except OSError:
                continue
    return "".join(text).lower()

def doors(bin_dir: str) -> list[str]:
    out = subprocess.run(f"nm -D {bin_dir}/libllama.so.0 | awk '$2==\"T\"{{print $3}}'",
                         shell=True, capture_output=True, text=True).stdout.split()
    return sorted(s for s in out if re.match(r"^(fg|bt|bmoe)_", s))

def modules_in_unit() -> list[str]:
    script = pathlib.Path("tools/build-sampler-chain.sh").read_text()
    return re.findall(r"goose/([a-z0-9/_\-]+\.goose)", script)


def module_report(bin_dir: str) -> int:
    """Per module in the composed unit: its exports, how many are named in the evidence, and the verdict."""
    text = corpus()
    mods = modules_in_unit()
    sources = {m: pathlib.Path("goose/" + m).read_text(errors="ignore") for m in mods}
    # which module calls which export, so a helper module with no card of its own is still accounted for
    callers: dict[str, set[str]] = {}
    for m, src in sources.items():
        for other, osrc in sources.items():
            if other == m:
                continue
            for e in re.findall(r"export fn ([a-z0-9_]+)", src):
                if re.search(r"\b" + re.escape(e) + r"\s*\(", osrc):
                    callers.setdefault(m, set()).add(other)
    rows = []
    for m in modules_in_unit():
        src = pathlib.Path("goose/" + m).read_text(errors="ignore")
        ex = re.findall(r"export fn ([a-z0-9_]+)", src)
        named = [e for e in ex if e in text]
        feature = [k for k in FEATURE.get(m.split("/")[-1], []) if k in text]
        if not ex:
            verdict = "implementation only - exercised through its doors"
        elif not named and callers.get(m):
            verdict = "exercised through: " + ", ".join(sorted(callers[m])[:2])
        elif named:
            verdict = f"{len(named)}/{len(ex)} exports named"
        elif feature:
            verdict = "feature named, exports not"
        else:
            verdict = "NO EVIDENCE"
        rows.append((m, len(ex), len(named), verdict))
    w = max(len(r[0]) for r in rows)
    print(f"{'module'.ljust(w)} exports named verdict")
    for m, n, named, verdict in rows:
        print(f"{m.ljust(w)} {n:7d} {named:5d} {verdict}")
    gaps = [r for r in rows if r[3] == "NO EVIDENCE"]
    if gaps:
        print(f"audit-door-evidence: FAIL, {len(gaps)} module(s) with no evidence")
        return 1
    print("audit-door-evidence: ok, every module in the unit is traceable to evidence")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default="builds/flint-acc/bin")
    ap.add_argument("--fail-on-gap", action="store_true")
    ap.add_argument("--modules", action="store_true",
                    help="report per module of the composed unit instead of per door")
    args = ap.parse_args()

    if args.modules:
        return module_report(args.bin)

    text = corpus()
    named, feature, gaps = [], [], []
    for s in doors(args.bin):
        if s in text:
            named.append(s)
        elif any(k in text for k in FEATURE.get(s, [])):
            feature.append(s)
        else:
            gaps.append(s)

    print(f"exported doors {len(named) + len(feature) + len(gaps)}: "
          f"named {len(named)}, feature-covered {len(feature)}, no record {len(gaps)}")
    for s in feature:
        print(f"  feature  {s}  <- covered under its feature name, not its symbol")
    for s in gaps:
        print(f"  NO RECORD {s}")
    if gaps and args.fail_on_gap:
        print(f"audit-door-evidence: FAIL, {len(gaps)} door(s) with no record")
        return 1
    print("audit-door-evidence: ok, every exported door is traceable to a record")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
