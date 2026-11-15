#!/bin/bash
# Package the composed Goose runtime core: build it once, then write a dist
# directory with the unit, the generated C/header, the object, a SHA256SUMS file
# and a JSON manifest (module list, compiler, sizes, symbol counts).
#
# This is the packaging half of P10-06. It does not link a host and runs no
# model; the extern entry points are still supplied by the runtime wiring.
#
# Usage: bash tools/package-main-runtime.sh [out-dir]
#   out-dir defaults to build/goose-main; the package lands in <out-dir>/dist.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${1:-$ROOT/build/goose-main}
DIST=$OUT/dist
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}

bash "$ROOT/tools/build-main-runtime.sh" "$OUT" >/dev/null

rm -rf "$DIST"
mkdir -p "$DIST"
cp "$OUT/main.goose" "$OUT/main.c" "$OUT/main.h" "$OUT/main.o" "$DIST/"

modules=$(grep -cE '^\s+goose/.*\.goose\s*$' "$ROOT/tools/build-main-runtime.sh")
defined=$(nm -g --defined-only "$OUT/main.o" | wc -l)
extern=$(nm -u "$OUT/main.o" | wc -l)
version=$("$GOOSE" version 2>/dev/null | tail -1 || echo unknown)

python3 - "$DIST" "$modules" "$defined" "$extern" "$version" "$OUT" <<'PY'
import hashlib, json, os, sys
from pathlib import Path
dist, modules, defined, extern, version, out = sys.argv[1:7]
files = ["main.goose", "main.c", "main.h", "main.o"]
sums = []
for name in files:
    p = Path(dist) / name
    sums.append(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {name}")
Path(dist, "SHA256SUMS").write_text("\n".join(sums) + "\n")
manifest = {
    "unit": "goose-main",
    "compiler": version,
    "modules": int(modules),
    "generated_c_bytes": os.path.getsize(Path(out, "main.c")),
    "object_bytes": os.path.getsize(Path(out, "main.o")),
    "global_symbols": int(defined),
    "extern_symbols": int(extern),
    "files": {name: f"{hashlib.sha256((Path(dist)/name).read_bytes()).hexdigest()}" for name in files},
}
Path(dist, "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(f"package: {dist}")
print(json.dumps(manifest, indent=2))
PY
