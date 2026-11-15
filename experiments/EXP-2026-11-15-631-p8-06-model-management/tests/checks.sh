#!/usr/bin/env bash
# Compatibility checks that do not build the gate: typecheck, composition and
# the declaration-collision scan against the canonical Goose tree.
#
# Usage: bash tests/checks.sh            (repo default: /home/end0/AI/FrankenMoE-CUDA)
#        FG_REPO=/path/to/repo bash tests/checks.sh
set -uo pipefail
E="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
RC=0

echo "== goose version =="
"$G" version

echo "== goose check per module =="
for f in "$E"/source/*.goose; do
  printf '%s: ' "$(basename "$f")"
  if "$G" check "$f" >/dev/null 2>&1; then echo ok; else echo FAIL; RC=1; fi
done

echo "== composed unit =="
mkdir -p "$E/build"
( echo "import std;"; for f in "$E"/source/*.goose; do grep -v '^import std;$' "$f"; done ) > "$E/build/composed.goose"
"$G" check "$E/build/composed.goose" 2>&1 | tail -2
sha256sum "$E/build/composed.goose"

echo "== declaration collisions against $REPO/goose =="
python3 - "$REPO" "$E" <<'PY'
import pathlib
import re
import sys

repo = pathlib.Path(sys.argv[1])
exp = pathlib.Path(sys.argv[2])
mine = set()
for f in sorted((exp / "source").glob("*.goose")):
    mine |= set(re.findall(r"^(?:export )?fn (\w+)", f.read_text(), re.M))
theirs = set()
files = 0
for p in repo.glob("goose/**/*.goose"):
    files += 1
    theirs |= set(re.findall(r"^(?:export )?fn (\w+)", p.read_text(errors="ignore"), re.M))
bad = sorted(mine & theirs)
print(f"mine={len(mine)} canonical_files={files} canonical_decls={len(theirs)} collisions={len(bad)}")
if bad:
    print("COLLIDING:", ", ".join(bad))
print("prefixes:", ", ".join(sorted({n.split("_")[0] + "_" for n in mine})))
sys.exit(1 if bad else 0)
PY
[ $? -ne 0 ] && RC=1

echo "== result =="
if [ $RC -eq 0 ]; then echo "checks ok"; else echo "checks FAILED"; fi
exit $RC
