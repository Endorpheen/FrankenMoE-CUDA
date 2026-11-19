#!/usr/bin/env python3
"""Check that every module in the canonical unit has evidence behind it.

Why this exists: EXP962 and EXP963 were both the same finding, found twice by
hand. Three sampler modules and the HTTP response module were gated in their own
experiments and absent from the composed runtime; then, once added, the reverse
question had no answer at all - which of the 140 units in the unit is actually
covered by a gate, and which is only present?

That question is answerable from the repository without a build. A module is
"evidenced" when some other tracked file names it: an experiment gate, a
document, or a tool. It is "unevidenced" when nothing does, and those are the ones
worth looking at, because a module nobody gates is a claim nobody checks.

Usage:
    python3 tools/check-unit-evidence.py              # report
    python3 tools/check-unit-evidence.py --strict     # exit 1 if any is unevidenced
    python3 tools/check-unit-evidence.py --module X   # explain one module

The list of modules is read from tools/build-main-runtime.sh itself, never typed
here, so the report cannot drift from the build.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "build-main-runtime.sh"

# Where evidence can legitimately live. Kept narrow on purpose: a mention inside
# goose/ itself is not evidence, because the module naming itself proves nothing.
EVIDENCE_GLOBS = (
    "experiments/**/*.md",
    "experiments/**/*.py",
    "experiments/**/*.cpp",
    "experiments/**/*.json",
    "experiments/**/*.txt",
    "docs/**/*.md",
    "integration/**/*.c",
    "integration/**/*.cpp",
    "tools/*.py",
    "tools/*.sh",
)

# Files that list the unit by construction name every module and prove nothing.
SELF_LISTING = {
    "tools/build-main-runtime.sh",
    "tools/check-unit-evidence.py",
}

# Generated and archived trees are not evidence of anything current.
SKIP_PARTS = {".git", "node_modules", "_archive", "results", "builds", "vendor", "__pycache__"}


def unit_modules() -> list[str]:
    text = SCRIPT.read_text()
    block = text.split("MODULES=(", 1)[1].split("\n)", 1)[0]
    return re.findall(r"(goose/[A-Za-z0-9_/.-]+\.goose)", block)


def evidence_index() -> dict[str, list[str]]:
    """module path -> files that name it, by path, basename or export call."""
    modules = unit_modules()
    names = {m: [] for m in modules}
    basenames = {m.rsplit("/", 1)[-1]: m for m in modules}
    corpus: dict[str, str] = {}
    for pattern in EVIDENCE_GLOBS:
        for path in ROOT.glob(pattern):
            if not path.is_file():
                continue
            if SKIP_PARTS & set(path.parts):
                continue
            rel = str(path.relative_to(ROOT))
            if rel in SELF_LISTING:
                continue
            try:
                body = path.read_text(errors="replace")
            except OSError:
                continue
            corpus[rel] = body
            for module in modules:
                if module in body:
                    names[module].append(rel)
                    continue
                base = module.rsplit("/", 1)[-1]
                # A bare basename counts only when it is unambiguous.
                if body.count(base) and sum(1 for b in basenames if b == base) == 1:
                    names[module].append(rel)
    # A gate that includes the generated header calls the exports and never writes
    # the module path; that is the normal shape, not an exception.
    calls = call_index(corpus)
    for module in modules:
        for name in exports_of(module):
            for rel in calls.get(name, ()):
                if rel != module and rel not in names[module]:
                    names[module].append(rel)
    return names


EXPORT = re.compile(r"^export\s+(?:recursive\s+)?fn\s+([A-Za-z0-9_]+)\s*\(", re.MULTILINE)


def exports_of(module: str) -> list[str]:
    """The exported names of one module, for callers that never write its path.

    A gate commonly includes the generated header and calls `index_stats(...)`
    without ever naming goose/misc/edge0-pack-index.goose. Searching only for the
    path reports such a module as unverified, which is wrong - and the first version
    of this tool made exactly that mistake on three modules.
    """
    try:
        body = (ROOT / module).read_text(errors="replace")
    except OSError:
        return []
    return EXPORT.findall(body)


CALL = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")


def call_index(corpus: dict[str, str]) -> dict[str, list[str]]:
    """exported name -> files that call it. One pass over the corpus.

    The per-module search was 140 x 2663 regex scans and took minutes; this is one
    scan that builds the reverse index, and the module loop only looks names up.
    """
    index: dict[str, list[str]] = {}
    for rel, body in corpus.items():
        for name in set(CALL.findall(body)):
            index.setdefault(name, []).append(rel)
    return index


def gate_of(evidence_file: str) -> str | None:
    """The gate that belongs to an experiment README, when there is one.

    A module naming itself in an experiment README is evidence only if that
    experiment also carries an executable check. Otherwise the README is prose and
    the module is exactly as unverified as one with no file at all.
    """
    parts = Path(evidence_file).parts
    if not parts or parts[0] != "experiments" or len(parts) < 2:
        return None
    experiment = ROOT / parts[0] / parts[1]
    if not experiment.is_dir():
        return None
    for candidate in sorted(experiment.rglob("*")):
        if not candidate.is_file():
            continue
        if SKIP_PARTS & set(candidate.parts):
            continue
        if candidate.suffix in {".py", ".cpp", ".c", ".sh"} and (
                "tests" in candidate.parts or "gates" in candidate.parts
                or candidate.name in {"gate.py", "gate.cpp", "mutate.py"}):
            return str(candidate.relative_to(ROOT))
    return None


def classify(hits: list[str]) -> tuple[str, list[str]]:
    """strong when a test, gate or report names the module; weak otherwise."""
    for h in hits:
        if "/tests/" in h or "/gates/" in h or h.endswith(".py") or "REPORT" in h:
            return "strong", [h]
        gate = gate_of(h)
        if gate:
            return "strong", [h, gate]
    return "weak", hits[:2]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--strict", action="store_true", help="exit 1 when any module is unevidenced")
    ap.add_argument("--module", help="explain one module")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    modules = unit_modules()
    index = evidence_index()

    if args.module:
        want = args.module if args.module.startswith("goose/") else f"goose/{args.module}"
        if want not in index:
            print(f"not in the unit: {want}")
            return 2
        hits = index[want]
        kind, why = classify(hits) if hits else ("weak", [])
        print(f"{want}: {kind}, {len(hits)} evidence file(s)")
        for h in hits:
            print(f"  {h}")
        if kind == "strong" and len(why) == 2 and why[1].endswith((".py", ".cpp", ".c", ".sh")):
            print(f"  gate: {why[1]}")
        return 0 if kind == "strong" else 1

    called = {m: classify(h) for m, h in index.items()}
    unevidenced = sorted(m for m, (kind, _) in called.items() if kind == "weak")
    total = len(modules)
    if not args.quiet:
        print(f"canonical unit: {total} modules")
        print(f"with a gate:    {total - len(unevidenced)}")
        print(f"prose only:     {len(unevidenced)}")
        if unevidenced:
            print()
            print("Modules named only by prose - no test, gate or report:")
            for m in unevidenced:
                print(f"  {m}  <- {', '.join(index[m])}")
    if args.strict and unevidenced:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
