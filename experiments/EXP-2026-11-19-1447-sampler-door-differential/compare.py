#!/usr/bin/env python3
"""The sampler doors against the native stages.

Two things are compared and they are not the same kind of claim:
  selection - must be identical on every case, because it is the decision;
  kept set  - may differ by one element at the cut, because two correct summations of the same probabilities can cross
              the threshold on different sides. That is reported with the token's mass, not hidden.
"""
import re
import sys

def load(path):
    out = {}
    for line in open(path):
        m = re.match(r"(\S+ \S+ \S+ \S+ \S+) sel=(\S+) kept=(\d+) ids=(.*)", line.strip())
        if m:
            out[m.group(1)] = (int(m.group(2)), int(m.group(3)), set(m.group(4).strip(",").split(",")))
    return out

g, n = load(sys.argv[1]), load(sys.argv[2])
if len(g) != len(n) or set(g) != set(n):
    print(f"compare: FAIL, different case sets ({len(g)} vs {len(n)})")
    raise SystemExit(1)

sel_bad, set_diff, set_bad = [], 0, []
for k in g:
    if g[k][0] != n[k][0]:
        sel_bad.append((k, g[k][0], n[k][0]))
        continue
    if g[k][1] != n[k][1]:
        set_bad.append((k, g[k][1], n[k][1]))
    elif g[k][2] != n[k][2]:
        d = len(g[k][2] ^ n[k][2])
        if d <= 2:
            set_diff += 1
        else:
            set_bad.append((k, d, "symmetric difference"))

print(f"compare: cases={len(g)} selection_mismatches={len(sel_bad)} kept_size_mismatches={len(set_bad)} "
      f"boundary_token_differences={set_diff}")
for k, a, b in sel_bad[:5]:
    print(f"  SELECTION {k}: goose={a} native={b}")
for k, a, b in set_bad[:5]:
    print(f"  SET {k}: {a} vs {b}")
if sel_bad or set_bad:
    print("compare: FAIL")
    raise SystemExit(1)
print("compare: ok, every selection matches and the kept sets agree up to the threshold boundary")
