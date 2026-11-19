#!/usr/bin/env bash
# Every hand-built object the runtime links has a named, verifiable producer.
#
# tools/consolidate-runtime-tree.sh copies 18 objects from an experiment tree instead of compiling them.
# On 2026-11-19 that directory was the last unknown in the reproduction of the accepted runtime, and it
# took a symbol-by-symbol pass to resolve: 17 come from tracked sources, one is the Goose compiler's own
# runtime. The registry under docs/goose-migration/EXTRA-OBJECTS.json records each one, and this test
# refuses to let it drift - a new object with no producer, or a producer whose file has been deleted,
# fails here rather than during a release.
#
# Tier: fast - reads JSON and stats files, no build.
#
# Usage: bash tests/regression/extra-objects-provenance/run.sh
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"

{
    python3 - "$ROOT" <<'PY'
import json, pathlib, sys
root = pathlib.Path(sys.argv[1])
reg = json.load(open(root / "docs/goose-migration/EXTRA-OBJECTS.json"))
objects = reg["objects"]
ok = bad = 0
for e in objects:
    kind, producer = e["kind"], e["producer"]
    if kind == "compiler-runtime":
        ver = root / "../LlamaServer/toolchain/goose-2026.3-string/RUNTIME_VERSION.txt"
        ver = (root.parent / "LlamaServer/toolchain/goose-2026.3-string/RUNTIME_VERSION.txt")
        if ver.is_file():
            print(f"  ok      {e['object']:20} compiler runtime {ver.read_text().strip()}")
            ok += 1
        else:
            print(f"  MISSING {e['object']:20} runtime version file not found"); bad += 1
        continue
    p = root / producer
    if p.exists():
        print(f"  ok      {e['object']:20} {kind}: {producer}")
        ok += 1
    else:
        print(f"  MISSING {e['object']:20} {kind}: {producer} does not exist"); bad += 1
print(f"\nextra-objects: {ok} with a producer, {bad} without, of {len(objects)}")
sys.exit(1 if bad else 0)
PY
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
