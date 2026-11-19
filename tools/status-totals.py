#!/usr/bin/env python3
"""Recompute the status totals from the 152 records and write them where they are read.

The ledger keeps a summary beside the records, and the summary drifts: at the last check it said 115/23/14 while the records
said 121/21/10. A number that is typed is a number that goes stale, so it is derived here instead - the summary in tasks.json
and the header of STATUS.md both come from counting the records themselves.

usage: status-totals.py [--check]
  --check   exit 1 when anything is out of date, without writing
"""
import json
import pathlib
import re
import sys

LEDGER = pathlib.Path("docs/goose-migration/tasks.json")
STATUS = pathlib.Path("docs/goose-migration/STATUS.md")


def actual(d):
    counts = {"DONE": 0, "PARTIAL": 0, "TODO": 0}
    for t in d["tasks"]:
        counts[t["status"]] = counts.get(t["status"], 0) + 1
    return counts, len(d["tasks"])


def main() -> int:
    check = "--check" in sys.argv
    d = json.loads(LEDGER.read_text())
    counts, total = actual(d)
    stale = []

    if d.get("status_totals") != counts:
        stale.append(f"tasks.json status_totals: {d.get('status_totals')} -> {counts}")
        d["status_totals"] = counts

    text = STATUS.read_text()
    want = f"**DONE {counts['DONE']}**, **PARTIAL {counts['PARTIAL']}**, **TODO {counts['TODO']}** of"
    new_text, n = re.subn(r"\*\*DONE \d+\*\*, \*\*PARTIAL \d+\*\*, \*\*TODO \d+\*\* of", want, text)
    if n == 0:
        print("status-totals: the STATUS.md summary line was not found - it changed shape and this tool is blind to it")
        return 2
    if new_text != text:
        stale.append("STATUS.md summary line")

    if stale:
        for s in stale:
            print(f"status-totals: stale - {s}")
        if check:
            return 1
        LEDGER.write_text(json.dumps(d, indent=2, ensure_ascii=False) + "\n")
        STATUS.write_text(new_text)
        print(f"status-totals: written, {counts['DONE']} DONE / {counts['PARTIAL']} PARTIAL / {counts['TODO']} TODO of {total}")
        return 0

    print(f"status-totals: ok, {counts['DONE']} DONE / {counts['PARTIAL']} PARTIAL / {counts['TODO']} TODO of {total}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
