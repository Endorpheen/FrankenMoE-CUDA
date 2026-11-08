#!/usr/bin/env python3
import re, sys

path = "repack-control/patches/repack-control.patch"
lines = open(path, "r", encoding="utf-8").read().splitlines()
i = 0
hunks = 0
errors = []
while i < len(lines):
    line = lines[i]
    m = re.match(r"^@@ -(\d+),(\d+) \+(\d+),(\d+) @@", line)
    if not m:
        i += 1
        continue
    old_n, new_n = int(m.group(2)), int(m.group(4))
    i += 1
    old = new = 0
    while i < len(lines) and not lines[i].startswith("@@"):
        if lines[i].startswith("---"):
            break
        c = lines[i][:1]
        if c == " ":
            old += 1; new += 1
        elif c == "-":
            old += 1
        elif c == "+":
            new += 1
        elif c == "":
            old += 1; new += 1
        i += 1
    hunks += 1
    if old != old_n or new != new_n:
        errors.append(f"hunk {hunks}: counts old={old}/{old_n} new={new}/{new_n}")

if hunks != 7:
    errors.append(f"expected 7 hunks, got {hunks}")
if errors:
    print("FG006_RC_PATCH FAILED")
    for e in errors:
        print("ERROR", e)
    sys.exit(1)
print(f"FG006_RC_PATCH PASS hunks={hunks}")
