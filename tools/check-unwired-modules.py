#!/usr/bin/env python3
"""Find Goose modules that export decisions but are in no composed unit.

Two cycles in a row the same defect appeared: a module owned a decision, nothing
contained it, and a host had grown its own version of the rule instead
(EXP995 shutdown-policy, EXP996 limits/error_404). Both were found by accident.

This turns the accident into a list. It separates three cases:

  in unit        - composed, so the decision is reachable
  excluded       - the path says it is not a runtime decision (archive, a rejected
                   variant, a staged copy, a fixture, a test)
  unwired        - exports decisions and no rule excludes it

The baseline file records every unwired module with a status and, where I have
actually read it, a reason. The check fails only on a module that is not in the
baseline, so the list cannot grow unnoticed; it also reports how many entries are
still unclassified, so the backlog is a number rather than a feeling.

Usage:
  tools/check-unwired-modules.py            check against the baseline
  tools/check-unwired-modules.py --refresh  rewrite the baseline from the tree
  tools/check-unwired-modules.py --list     print every unwired module
Exit: 0 no new unwired module, 1 at least one
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "tools", "build-main-runtime.sh")
BASELINE = os.path.join(ROOT, "docs", "goose-migration", "UNWIRED-MODULES.json")
EXPORT_RE = re.compile(r"^\s*export\s+fn\s+", re.M)

# A path that names itself as not-a-runtime-decision. These are excluded by rule, not
# by judgement, so nobody has to maintain 300 entries.
EXCLUDE_RULES = [
    ("_archive", "archived tree; kept for the record, not built"),
    ("/rejected", "a rejected variant, kept with its experiment"),
    ("-rejected", "a rejected variant, kept with its experiment"),
    ("/prior-design", "superseded design, kept for comparison"),
    ("/results/work", "mutation and debug copies under an experiment's results"),
    ("/diagnostic", "diagnostic draft, not a decision"),
    ("/corpus", "parser corpus, a test input"),
    ("/tests/", "a test, not a runtime decision"),
    ("/fixtures/", "fixture data"),
    ("/s1-1-apex-staging", "staged extraction, not a decision set"),
    ("/s1-6-stream-staged", "staged copy of modules that are already in the unit"),
    ("/s1-5-", "staged copy"),
    ("/s2-1-", "staged copy"),
    ("/iq-qgrid/", "quantisation experiment workspace"),
]


def in_unit():
    with open(BUILD) as f:
        text = f.read()
    m = re.search(r"MODULES=\((.*?)\)", text, re.S)
    return {os.path.basename(x.strip()) for x in m.group(1).split() if x.strip()}


def exports(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return len(EXPORT_RE.findall(f.read()))
    except OSError:
        return 0


def excluded(path):
    rel = os.path.relpath(path, ROOT)
    for needle, why in EXCLUDE_RULES:
        if needle in rel:
            return why
    return None


def scan():
    unit = in_unit()
    found, excluded_n = [], 0
    for base, _, files in os.walk(os.path.join(ROOT, "goose")):
        for f in files:
            if not f.endswith(".goose"):
                continue
            path = os.path.join(base, f)
            if f in unit:
                continue
            n = exports(path)
            if n == 0:
                continue
            why = excluded(path)
            if why:
                excluded_n += 1
                continue
            found.append({"path": os.path.relpath(path, ROOT), "exports": n})
    return sorted(found, key=lambda e: e["path"]), excluded_n


def load_baseline():
    try:
        with open(BASELINE) as f:
            return json.load(f)
    except (OSError, ValueError):
        return {"schema": 1, "entries": []}


def main():
    refresh = "--refresh" in sys.argv
    listing = "--list" in sys.argv
    found, excluded_n = scan()
    base = load_baseline()
    known = {e["path"]: e for e in base.get("entries", [])}

    if refresh:
        entries = []
        for e in found:
            prev = known.get(e["path"], {})
            entries.append({
                "path": e["path"],
                "exports": e["exports"],
                "status": prev.get("status", "unclassified"),
                "reason": prev.get("reason", ""),
            })
        base["schema"] = 1
        base["entries"] = entries
        base["note"] = ("Every module that exports decisions and is in no composed unit. "
                        "status excluded_with_reason means the reason was read and "
                        "recorded; unclassified means it is counted but not yet judged. "
                        "The check fails only on a path absent from this file.")
        os.makedirs(os.path.dirname(BASELINE), exist_ok=True)
        with open(BASELINE, "w") as f:
            json.dump(base, f, indent=1)
            f.write("\n")
        print("baseline written: %d entries, %d excluded by path" % (len(entries), excluded_n))
        return 0

    new = [e for e in found if e["path"] not in known]
    gone = [p for p in known if p not in {e["path"] for e in found}]
    unclassified = sum(1 for e in base.get("entries", []) if e.get("status") != "excluded_with_reason")

    if listing:
        for e in found:
            st = known.get(e["path"], {}).get("status", "NEW")
            print("  %-64s %3d exports  %s" % (e["path"], e["exports"], st))

    print("unwired modules: %d | excluded by path: %d | unclassified in baseline: %d"
          % (len(found), excluded_n, unclassified))
    for e in new:
        print("FAIL %s exports %d decisions and is in no unit and no baseline"
              % (e["path"], e["exports"]))
    for p in gone:
        print("NOTE %s is in the baseline but no longer unwired; refresh the file" % p)
    if new:
        return 1
    print("PASS no unwired module outside the recorded baseline")
    return 0


if __name__ == "__main__":
    sys.exit(main())
