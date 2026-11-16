#!/bin/bash
# Package the composed Goose runtime core: build it once, then write a dist
# directory with the unit, the generated C/header, the object, a SHA256SUMS file
# and a JSON manifest (module list, compiler, sizes, symbol counts).
#
# This is the packaging half of P10-06. It does not link a host and runs no
# model; the extern entry points are still supplied by the runtime wiring.
#
# The document decisions — the dist file list and its order, the SHA256SUMS
# bytes, the manifest key order, the module count and the compiler line — are
# the P10-05 S1 slice, `goose/misc/release-plan.goose`, reached through
# `build/goose-hosts/pkg` (`bash tools/goose-hosts/build.sh`). Hashing, stat()
# and writing stay here. BMOE_GOOSE_PKG=0 restores the inline decisions.
#
# Usage: bash tools/package-main-runtime.sh [out-dir]
#   out-dir defaults to build/goose-main; the package lands in <out-dir>/dist.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${1:-$ROOT/build/goose-main}
DIST=$OUT/dist
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
# shellcheck source=goose-hosts/client.sh
. "$ROOT/tools/goose-hosts/client.sh"

bash "$ROOT/tools/build-main-runtime.sh" "$OUT" >/dev/null

rm -rf "$DIST"
mkdir -p "$DIST"

if goosehost_init "$ROOT" pkg BMOE_GOOSE_PKG; then
    # The dist list is the module's: six names in the order the script writes
    # them. The four build products are copied; the two documents are composed
    # below, from the same list.
    mapfile -t DIST_FILES < <(goosehost_raw "$(goosehost_ask "DIST")")
    for name in "${DIST_FILES[@]}"; do
        [ -n "$name" ] || continue
        [ -f "$OUT/$name" ] && cp "$OUT/$name" "$DIST/"
    done

    modules=$(goosehost_ask "COUNT $(goosehost_hex "$(cat "$ROOT/tools/build-main-runtime.sh")")")
    defined=$(nm -g --defined-only "$OUT/main.o" | wc -l)
    extern=$(nm -u "$OUT/main.o" | wc -l)
    ver_text=$("$GOOSE" version 2>/dev/null) || ver_text=unknown
    version=$(goosehost_raw "$(goosehost_ask "VERSION_LINE $(goosehost_hex "$ver_text")")")

    hashes=$(python3 - "$DIST" <<'PY'
import hashlib, sys
from pathlib import Path
dist = Path(sys.argv[1])
for name in ["main.goose", "main.c", "main.h", "main.o"]:
    print(hashlib.sha256((dist / name).read_bytes()).hexdigest(), name)
PY
)
    sums_args=()
    hash_args=()
    while read -r hash name; do
        [ -n "$hash" ] || continue
        sums_args+=("$(goosehost_hex "$hash")" "$(goosehost_hex "$name")")
        hash_args+=("$(goosehost_hex "$hash")")
    done <<<"$hashes"

    goosehost_raw "$(goosehost_ask "SUMS 4 ${sums_args[*]}")" > "$DIST/SHA256SUMS"

    goosehost_raw "$(goosehost_ask "MANIFEST $(goosehost_hex goose-main) $(goosehost_hex "$version") \
        "$modules" "$(stat -c %s "$OUT/main.c")" "$(stat -c %s "$OUT/main.o")" \
        "$defined" "$extern" ${hash_args[*]}")" > "$DIST/manifest.json"

    echo "package: $DIST"
    cat "$DIST/manifest.json"
    exit 0
fi

modules=$(grep -cE '^\s+goose/.*\.goose\s*$' "$ROOT/tools/build-main-runtime.sh")
defined=$(nm -g --defined-only "$OUT/main.o" | wc -l)
extern=$(nm -u "$OUT/main.o" | wc -l)
version=$("$GOOSE" version 2>/dev/null | tail -1 || echo unknown)
cp "$OUT/main.goose" "$OUT/main.c" "$OUT/main.h" "$OUT/main.o" "$DIST/"

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
