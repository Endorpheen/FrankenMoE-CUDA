#!/usr/bin/env bash
# Compare two runtime libraries the way the project has learned to need it.
#
# Written 2026-11-19, after a candidate build was rejected for carrying 87 unit doors against the
# release's 198 and missing four accepted components - a difference no one would have noticed from the
# build's own output, because the build succeeded.
#
# It answers these questions in one table, and refuses to answer the last one:
#   1. do both carry the same accepted components?
#   2. how many unit doors does each carry, and which differ?
#   3. does either have duplicate global symbols?
#   4. how do they compare in size and symbol count?
#   NOT answered: whether they behave the same. A component list is not a measurement.
#
# The set comparisons are done in Python. The first version used `comm` and spent three rounds on
# sortedness and locale before producing a number - the same class of harness error that made a gate
# report eight differences of its own earlier the same day.
#
# Usage: bash tools/compare-artefacts.sh <lib-a> <lib-b>
# Exit 0 when both pass the accepted-component check and neither has duplicates, 1 otherwise.
set -uo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
A=${1:?usage: compare-artefacts.sh <lib-a> <lib-b>}
B=${2:?usage: compare-artefacts.sh <lib-a> <lib-b>}

for f in "$A" "$B"; do
    [ -f "$f" ] || { echo "not a file: $f"; exit 2; }
done

python3 - "$ROOT" "$A" "$B" <<'PY'
import hashlib, pathlib, re, subprocess, sys

root = pathlib.Path(sys.argv[1])
a, b = pathlib.Path(sys.argv[2]), pathlib.Path(sys.argv[3])

def defined(p):
    out = subprocess.run(["nm", "-D", "--defined-only", str(p)], capture_output=True, text=True).stdout
    return [l.split()[-1] for l in out.splitlines() if len(l.split()) == 3]

def dupes(p):
    seen, dup = set(), set()
    for s in defined(p):
        (dup if s in seen else seen).add(s)
    return sorted(dup)

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()[:16]

doors = set()
for line in (root / "tools/build-main-runtime.sh").read_text().splitlines():
    line = line.split("#")[0].strip()
    if line.startswith("goose/") and (root / line).exists():
        doors |= set(re.findall(r"(?m)^export fn ([a-z0-9_]+)", (root / line).read_text()))

sa, sb = set(defined(a)), set(defined(b))
da, db = doors & sa, doors & sb
dpa, dpb = dupes(a), dupes(b)

print(f"{'':28} {'A':30} B")
print(f"{'path':28} {str(a.parent)[:30]:30} {b.parent}")
print(f"{'sha256':28} {sha(a):30} {sha(b)}")
print(f"{'bytes':28} {a.stat().st_size:<30} {b.stat().st_size}")
print(f"{'defined symbols':28} {len(sa):<30} {len(sb)}")
print(f"{'duplicate globals':28} {len(dpa):<30} {len(dpb)}")
print(f"{'unit doors carried':28} {len(da):<30} {len(db)}")
print(f"{'':28} only in A: {len(da - db):<17} only in B: {len(db - da)}")
for s in sorted(da - db)[:8]:
    print(f"    A only: {s}")
for s in sorted(db - da)[:8]:
    print(f"    B only: {s}")

print()
print("component lists:")
rc = 0
for label, d in (("A", a.parent), ("B", b.parent)):
    r = subprocess.run([sys.executable, str(root / "tools/check-accepted-components.py"), str(d)],
                       capture_output=True, text=True)
    print(f"  {label}: {str(d)[:52]:52} {'ok' if r.returncode == 0 else 'FAIL'}")
    if r.returncode != 0:
        rc = 1
if dpa or dpb:
    rc = 1

print()
print("Not answered here: whether they behave the same. Two libraries with identical component lists can")
print("still produce different output - that is a model run, not a symbol table.")
sys.exit(rc)
PY
