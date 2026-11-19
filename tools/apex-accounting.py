#!/usr/bin/env python3
"""The accounting P11-07 needs: how many APEX_COMMON tasks are done, and exactly what blocks the rest.

Generated from the registry so it cannot drift from it. Each open task is printed with its own remainder, which is the only
honest answer to "why is this not DONE yet".
"""
import json
import pathlib
import sys

reg = pathlib.Path("docs/goose-migration/tasks.json")
d = json.loads(reg.read_text())


def tags(t):
    out = []
    for k in ("scope", "tags", "groups", "applies_to", "workstream"):
        v = t.get(k)
        if isinstance(v, list):
            out += [str(x) for x in v]
        elif isinstance(v, str):
            out.append(v)
    return out


def short(v, n=220):
    if isinstance(v, list):
        text = "; ".join(str(x) for x in v if str(x).strip())
    else:
        text = str(v or "")
    text = " ".join(text.split())
    return text[:n] + ("..." if len(text) > n else "")


apex = [t for t in d["tasks"] if any("APEX" in x.upper() for x in tags(t))]
counts = {"DONE": 0, "PARTIAL": 0, "TODO": 0}
for t in apex:
    counts[t["status"]] = counts.get(t["status"], 0) + 1

print(f"APEX_COMMON tasks: {len(apex)}")
print(f"  DONE    {counts['DONE']}")
print(f"  PARTIAL {counts['PARTIAL']}")
print(f"  TODO    {counts['TODO']}")
print()
print("Open tasks and their own remainder:")
for t in apex:
    if t["status"] == "DONE":
        continue
    print(f"- {t['id']} [{t['status']}] {t.get('title','')}")
    print(f"    {short(t.get('remaining'))}")
print()
print("P11-07 needs both conditions:")
print(f"  all APEX_COMMON DONE: {'yes' if counts['DONE'] == len(apex) else 'no - ' + str(counts['PARTIAL'] + counts['TODO']) + ' open'}")
print("  zero native executable logic on Apex paths: depends on the removals listed under P11-04 and the PARTIAL items above")
sys.exit(0)
