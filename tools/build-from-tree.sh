#!/usr/bin/env bash
# Build the whole runtime from the consolidated tree, in a clean output directory, using nothing from the
# experiment directories or from whatever objects happen to lie in the old build dir.
#
# Usage: bash tools/build-from-tree.sh [out-dir]
set -euo pipefail
TREE=${TREE:-/home/end0/AI/LlamaServer/runtime-build-tree}
OUT=${1:-$TREE/build/out-$(date +%Y%m%d-%H%M%S)}
[ -e "$OUT" ] && { echo "refusing to overwrite $OUT"; exit 1; }
mkdir -p "$OUT/obj"

# Refuse to build from a tree whose inputs do not match its manifest: a build from silently different sources is
# the failure this whole exercise exists to prevent.
if [ -f "$TREE/SHA256SUMS" ]; then
    (
        cd "$TREE"
        python3 - <<'PY'
from pathlib import Path

manifest = {
    Path(line[66:])
    for line in Path("SHA256SUMS").read_text().splitlines()
}
roots = (
    "our-src", "modules", "module-src", "bridges", "m1bridges",
    "extra-objects", "libs", "upstream-include",
    "module-dirs.txt",
)
invalid = []
for name in roots:
    root = Path(name)
    for path in (root, *root.rglob("*")):
        if path.is_symlink() or (path.is_file() and path not in manifest):
            invalid.append(path)
if invalid:
    for path in sorted(invalid):
        print(f"unverified build input: {path}")
    raise SystemExit(1)
PY
    ) || { echo "input inventory verification failed: $TREE"; exit 1; }
    ( cd "$TREE" && sha256sum -c SHA256SUMS --strict --quiet ) || { echo "input verification failed: $TREE"; exit 1; }
    echo "inputs verified against $TREE/SHA256SUMS"
else
    echo "no SHA256SUMS in $TREE - refusing to build unverified inputs"; exit 1
fi

compile_jobs=()
wait_for_compilers() {
    local pid failed=0
    for pid in "${compile_jobs[@]}"; do
        wait "$pid" || failed=1
    done
    compile_jobs=()
    if (( failed )); then
        echo "compilation of our sources failed" >&2
        return 1
    fi
}

test -f "$TREE/our-src/models/models.h"
CORE_FLAGS=(-O3 -DNDEBUG -fPIC -std=c++17 -DGGML_BACKEND_SHARED -DGGML_SHARED
    -DGGML_USE_CPU -DGGML_USE_CUDA -DLLAMA_BUILD -DLLAMA_SHARED -Dllama_EXPORTS)
CORE_INC=(-I"$TREE/our-src" -I"$TREE/upstream-include" -I"$TREE/module-src" -I"$TREE/modules")
for src in "$TREE"/our-src/*.cpp "$TREE"/our-src/models/*.cpp; do
    rel=${src#"$TREE/our-src/"}
    g++-13 "${CORE_FLAGS[@]}" "${CORE_INC[@]}" -c "$src" -o "$OUT/obj/our-${rel//\//_}.o" &
    compile_jobs+=("$!")
    if (( ${#compile_jobs[@]} == 4 )); then
        wait_for_compilers
    fi
done
wait_for_compilers
gcc-13 -O2 -fPIC -std=c11 -c "$TREE/our-src/gates.c" -o "$OUT/obj/our-gates.o"
echo "our sources: $(ls "$OUT/obj"/our-*.o | wc -l)"

MODINC=""
MODINC="-I$TREE/module-src"
for d in "$TREE"/module-src/*/; do MODINC="$MODINC -I$d"; done
# Each module is compiled with ITS OWN experiment directory first. A shared pool of directories lets
# #include "cp-generated.c" resolve to another experiment's copy - fg243c failed with "too many arguments to
# function gg_function_1" for exactly that reason, and resolving to a compatible-looking wrong file would be worse
# than failing.
mod_own_dir() {
    local want=$1
    [ -f "$TREE/module-dirs.txt" ] || return 0
    awk -v w="$want" '$1 == w { print $2; exit }' "$TREE/module-dirs.txt"
}
for src in "$TREE"/modules/*.c; do
    [ -e "$src" ] || continue
    b=$(basename "$src")
    own=$(mod_own_dir "$b")
    gcc-13 -O2 -fPIC -I"$TREE/modules" ${own:+-I"$TREE/module-src/$own"} $MODINC \
        -c "$src" -o "$OUT/obj/mod-$(basename "$src" .c).o"
done
for src in "$TREE"/modules/*.cpp; do
    [ -e "$src" ] || continue
    g++-13 -O2 -fPIC -std=c++17 -I"$TREE/modules" $MODINC -c "$src" -o "$OUT/obj/mod-$(basename "$src" .cpp).o"
done
echo "modules: $(ls "$OUT/obj"/mod-*.o 2>/dev/null | wc -l)"

for d in "$TREE"/bridges/*/; do
    m=$(basename "$d")
    [ -f "$d/bridge.c" ] || continue
    gcc-13 -O3 -DNDEBUG -fPIC -std=c11 -I"$d" -c "$d/bridge.c" -o "$OUT/obj/br-$m.o"
done
echo "bridges: $(ls "$OUT/obj"/br-*.o 2>/dev/null | wc -l)"

# The composed Goose unit. Until 2026-11-20 this link never included it, and the omission was invisible:
# the build succeeded, the artefact loaded and answered, and it carried 11 unit doors where the accepted
# artefact carries 200. The unit object alone defines over five thousand entry points, so its absence looked
# like "path B produces a smaller runtime" rather than "path B forgot a file".
# `ls` with a pattern that matches nothing exits non-zero, pipefail propagates it and `set -e` ends the
# script with no message - the third time this shape has bitten in one day, so it is guarded here.
UNIT_OBJ=$(ls "$TREE"/m1/build/unit/main.o "$TREE"/unit/*.o 2>/dev/null | head -1 || true)
[ -n "$UNIT_OBJ" ] || {
    echo "no composed unit object under $TREE (looked for m1/build/unit/main.o and unit/*.o)."
    echo "Stage the tree with --with-unit; a runtime without the unit is not the runtime."
    exit 2
}
# The staged tree builds the unit for an executable, without -fPIC, so its object cannot go into a shared
# library. Compile a PIC copy from the same generated C instead of asking that build to change: the source is
# the unit, the flag is ours.
UNIT_SRC=${UNIT_SRC:-$(dirname "$UNIT_OBJ")/main.c}
[ -f "$UNIT_SRC" ] || { echo "no unit source next to $UNIT_OBJ"; exit 2; }
# The M1 host owns device table binding and the once guard for the composed unit.
gcc-13 -O1 -fPIC -std=c11 -w -Dgoose_init=fg_composed_goose_init -c "$UNIT_SRC" -o "$OUT/obj/unit-pic.o" \
    || { echo "unit PIC compile failed"; exit 1; }
cat > "$OUT/obj/unit-init-shim.c" <<'SHIM'
void fg_composed_goose_init(int argc, char ** argv);
void fg_goose_host_ensure_init(void);
__attribute__((constructor)) static void init_composed_unit(void) { fg_goose_host_ensure_init(); }
void goose_init(int argc, char ** argv) { fg_composed_goose_init(argc, argv); }
void fg950_goose_init(int argc, char ** argv) { fg_goose_host_ensure_init(); }
void fg_arch_registry_goose_init(int argc, char ** argv) { fg_goose_host_ensure_init(); }
void fg_q35_goose_init(int argc, char ** argv) { fg_goose_host_ensure_init(); }
void fg_qg_goose_init(int argc, char ** argv) { fg_goose_host_ensure_init(); }
SHIM
gcc-13 -O1 -fPIC -std=c11 -w -c "$OUT/obj/unit-init-shim.c" -o "$OUT/obj/unit-init-shim.o" \
    || { echo "unit init shim failed"; exit 1; }
UNIT_PIC=$OUT/obj/unit-pic.o
UNIT_OBJ="$UNIT_PIC $OUT/obj/unit-init-shim.o"
echo "unit object: $UNIT_PIC ($(nm --defined-only "$UNIT_PIC" | awk 'NF==3' | wc -l) defined symbols, PIC, from $UNIT_SRC)"

# The legacy SLRU ABI has its own unit and constructor.
SLRU_INIT=-Dgoose_init=fg_legacy_slru_goose_init
gcc-13 -O2 -fPIC -std=c11 "$SLRU_INIT" -c "$TREE/module-src/slru-policy.c" -o "$OUT/obj/mod-slru-policy.o"

# The hand-built extra objects existed because the unit was not linked. Now that it is, an object whose
# symbols the unit already defines is a duplicate, not an addition - the first rebuild after linking the unit
# failed with exactly that (q35_shape, q35_rope_multi, qg_split, goose_init defined twice). Skip such objects
# and say which, so the list can shrink deliberately rather than by accident.
EXTRA_OBJS=""
export LC_ALL=C
UNIT_SYMS=$(mktemp)
trap 'rm -f "$UNIT_SYMS"' EXIT
strong_symbols() {
    nm -g --defined-only -P "$1" | awk '$2 ~ /^[A-Z]$/ && $2 !~ /^[VW]$/ {print $1}' | sort -u
}
strong_symbols "$UNIT_PIC" > "$UNIT_SYMS"
for o in "$TREE"/extra-objects/*.o; do
    [ -f "$o" ] || continue
    case "${o##*/}" in gates.o|fg-plan-ffi.o|slru-policy.o|slru-shim.o) continue;; esac
    OV=$(strong_symbols "$o" | comm -12 - "$UNIT_SYMS")
    if [ -n "$OV" ]; then
        echo "  extra object skipped (the unit already defines its symbols): $(basename "$o")"
    else
        EXTRA_OBJS="$EXTRA_OBJS $o"
    fi
done
rm -f "$UNIT_SYMS"
trap - EXIT

# Use the staged M1 shims (including gguf-stack.h) for the ABI of the composed unit.
M1="$TREE/m1"
M1INC=(
    -I"$M1/goose-gen" -I"$TREE/m1bridges" -I"$TREE/module-src" -I"$TREE/modules"
    -I"$TREE/upstream-include" -I"$TREE/our-src" -I"$M1/runtime/core/include"
)
for src in "$TREE"/m1bridges/*.cpp; do
    [ -f "$src" ] || continue
    g++-13 -O2 -fPIC -std=c++17 -w "${M1INC[@]}" -c "$src" -o "$OUT/obj/m1-$(basename "$src" .cpp).o" \
        || { echo "m1 bridge failed: $src"; exit 1; }
done
for src in "$TREE"/m1bridges/*.c; do
    [ -f "$src" ] || continue
    flags=()
    if [ "${src##*/}" = slru-runtime-shim.c ]; then flags+=("$SLRU_INIT"); fi
    gcc-13 -O2 -fPIC -std=c11 -w "${M1INC[@]}" "${flags[@]}" -c "$src" -o "$OUT/obj/m1-$(basename "$src" .c).o" \
        || { echo "m1 host failed: $src"; exit 1; }
done
echo "m1 objects: $(ls "$OUT"/obj/m1-*.o 2>/dev/null | wc -l)"

# Keep the runtime search path relative to the installed library.
/usr/bin/c++ -fPIC -O3 -DNDEBUG -shared -Wl,-z,defs -Wl,-soname,libllama.so.0 \
    -o "$OUT/libllama.so.0" \
    "$OUT"/obj/our-*.o "$OUT"/obj/mod-*.o "$OUT"/obj/br-*.o "$OUT"/obj/m1-*.o $UNIT_OBJ \
    $EXTRA_OBJS \
    -Wl,-rpath,'$ORIGIN' \
    "$TREE/libs/libggml.so.0" "$TREE/libs/libggml-base.so.0" \
    "$TREE/libs/libggml-cpu.so.0" "$TREE/libs/libggml-cuda.so.0"

sha256sum "$OUT/libllama.so.0"
echo "built: $OUT/libllama.so.0 ($(stat -c%s "$OUT/libllama.so.0") bytes)"
