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

# Our sources replace these upstream files, so the matching upstream objects must not also be linked.
REPLACED=" llama-batch.cpp llama-context.cpp llama-graph.cpp llama-hparams.cpp llama-kv-cache.cpp
           llama-memory-hybrid.cpp llama-memory-recurrent.cpp llama-model-loader.cpp llama-model.cpp
           llama-moe-cache.cpp llama-sampler.cpp llama-vocab.cpp unicode.cpp "
# The list above is written across lines, so it carries newlines; normalise it before matching, or a pattern of
# " name " never matches and the upstream object of a replaced file gets linked anyway - which is exactly what
# happened: libllama carried two definitions of llama_model_base::load_tensors.
REPLACED=" $(echo $REPLACED) "
is_replaced() { case "$REPLACED" in *" $1 "*) return 0;; *) return 1;; esac; }

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
    "our-src", "modules", "module-src", "bridges", "flint-objects",
    "extra-objects", "libs", "upstream-src", "upstream-include",
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

for src in "$TREE"/our-src/*.cpp; do
    b=$(basename "$src")
    # module-src carries headers the Goose compiler writes for a module (slru-policy.h and others); a
    # carrier includes them, so the include path must be here and not only on the module compiles below.
    g++-13 -O3 -DNDEBUG -fPIC -std=c++17 -I"$TREE/our-src" -I"$TREE/upstream-include" -I"$TREE/upstream-src" \
        -I"$TREE/module-src" -I"$TREE/modules" -c "$src" -o "$OUT/obj/our-$b.o" &
    compile_jobs+=("$!")
    if (( ${#compile_jobs[@]} == 4 )); then
        wait_for_compilers
    fi
done
wait_for_compilers
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
# -Dgoose_init=fg950_goose_init: the GGUF reader adapter calls the unit's init by that name, because
# build.py compiles the unit with this define. Without it the artefact loads, starts, and dies on
# `undefined symbol: fg950_goose_init` - the same shape as the missing translation unit before it.
gcc-13 -O1 -fPIC -std=c11 -w -Dgoose_init=fg950_goose_init -c "$UNIT_SRC" -o "$OUT/obj/unit-pic.o" \
    || { echo "unit PIC compile failed"; exit 1; }
# Two names for the same start-up: the adapter calls fg950_goose_init (build.py's define) and the rest of the
# tree calls goose_init. One shim, no duplicated state.
cat > "$OUT/obj/unit-init-shim.c" <<'SHIM'
void fg950_goose_init(int argc, char ** argv);
void goose_init(int argc, char ** argv) { fg950_goose_init(argc, argv); }
SHIM
gcc-13 -O1 -fPIC -std=c11 -w -c "$OUT/obj/unit-init-shim.c" -o "$OUT/obj/unit-init-shim.o" \
    || { echo "unit init shim failed"; exit 1; }
UNIT_PIC=$OUT/obj/unit-pic.o
UNIT_OBJ="$UNIT_PIC $OUT/obj/unit-init-shim.o"
echo "unit object: $UNIT_PIC ($(nm --defined-only "$UNIT_PIC" | awk 'NF==3' | wc -l) defined symbols, PIC, from $UNIT_SRC)"

# The hand-built extra objects existed because the unit was not linked. Now that it is, an object whose
# symbols the unit already defines is a duplicate, not an addition - the first rebuild after linking the unit
# failed with exactly that (q35_shape, q35_rope_multi, qg_split, goose_init defined twice). Skip such objects
# and say which, so the list can shrink deliberately rather than by accident.
EXTRA_OBJS=""
UNIT_SYMS=$(mktemp)
nm --defined-only "$UNIT_PIC" | awk 'NF==3 {print $3}' | sort -u > "$UNIT_SYMS"
for o in "$TREE"/extra-objects/*.o; do
    [ -f "$o" ] || continue
    # Captured, not piped into grep -q: grep leaves the pipe on its first match, comm dies of SIGPIPE and
    # pipefail turns "they overlap" into "they do not" - which is how slru-policy.o survived this filter once.
    OV=$(nm --defined-only "$o" | awk 'NF==3 {print $3}' | sort -u | LC_ALL=C comm -12 - "$UNIT_SYMS" || true)
    if [ -n "$OV" ]; then
        echo "  extra object skipped (the unit already defines its symbols): $(basename "$o")"
    else
        EXTRA_OBJS="$EXTRA_OBJS $o"
    fi
done
rm -f "$UNIT_SYMS"

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# The C++ m1 bridges: the entry points the composed unit calls and nothing else in this tree defined.
# goose_tok_enabled was the first undefined symbol; compiling the set brings the rest with it.
# runtime/core/include carries bmoe/config.h and its siblings, which the m1 bridges include. Recorded as a
# follow-up: the tree should carry a copy so this build does not read outside it.
M1INC="-I$TREE/m1bridges -I$TREE/module-src -I$TREE/modules -I$TREE/upstream-include -I$REPO_ROOT/runtime/core/include"
for src in "$TREE"/m1bridges/*.cpp; do
    [ -f "$src" ] || continue
    g++-13 -O2 -fPIC -std=c++17 -w $M1INC -c "$src" -o "$OUT/obj/m1-$(basename "$src" .cpp).o" \
        || { echo "m1 bridge failed: $src"; exit 1; }
done
for src in "$TREE"/m1bridges/*.c; do
    [ -f "$src" ] || continue
    gcc-13 -O2 -fPIC -std=c11 -w $M1INC -c "$src" -o "$OUT/obj/m1-$(basename "$src" .c).o" \
        || { echo "m1 host failed: $src"; exit 1; }
done
echo "m1 objects: $(ls "$OUT"/obj/m1-*.o 2>/dev/null | wc -l)"

flint=""
for o in "$TREE"/flint-objects/*.o; do
    is_replaced "$(basename "$o" .o)" || flint="$flint $o"
done
echo "upstream objects: $(echo $flint | wc -w) of $(ls "$TREE"/flint-objects/*.o | wc -l)"

# Keep the runtime search path relative to the installed library.
/usr/bin/c++ -fPIC -O3 -DNDEBUG -shared -Wl,-soname,libllama.so.0 \
    -o "$OUT/libllama.so.0" \
    "$OUT"/obj/our-*.o "$OUT"/obj/mod-*.o "$OUT"/obj/br-*.o "$OUT"/obj/m1-*.o $UNIT_OBJ \
    $EXTRA_OBJS $flint \
    -Wl,-rpath,'$ORIGIN' \
    "$TREE/libs/libggml.so.0" "$TREE/libs/libggml-base.so.0" \
    "$TREE/libs/libggml-cpu.so.0" "$TREE/libs/libggml-cuda.so.0"

sha256sum "$OUT/libllama.so.0"
echo "built: $OUT/libllama.so.0 ($(stat -c%s "$OUT/libllama.so.0") bytes)"
