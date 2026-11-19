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
    ( cd "$TREE" && sha256sum -c SHA256SUMS --quiet ) || { echo "input verification failed: $TREE"; exit 1; }
    echo "inputs verified against $TREE/SHA256SUMS"
else
    echo "no SHA256SUMS in $TREE - refusing to build unverified inputs"; exit 1
fi

n=0
for src in "$TREE"/our-src/*.cpp; do
    b=$(basename "$src")
    # module-src carries headers the Goose compiler writes for a module (slru-policy.h and others); a
    # carrier includes them, so the include path must be here and not only on the module compiles below.
    g++-13 -O3 -DNDEBUG -fPIC -std=c++17 -I"$TREE/our-src" -I"$TREE/upstream-include" -I"$TREE/upstream-src" \
        -I"$TREE/module-src" -I"$TREE/modules" -c "$src" -o "$OUT/obj/our-$b.o" &
    n=$((n+1)); [ $((n % 4)) -eq 0 ] && wait
done
wait
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

flint=""
for o in "$TREE"/flint-objects/*.o; do
    is_replaced "$(basename "$o" .o)" || flint="$flint $o"
done
echo "upstream objects: $(echo $flint | wc -w) of $(ls "$TREE"/flint-objects/*.o | wc -l)"

/usr/bin/c++ -fPIC -O3 -DNDEBUG -shared -Wl,-soname,libllama.so.0 \
    -o "$OUT/libllama.so.0" \
    "$OUT"/obj/our-*.o "$OUT"/obj/mod-*.o "$OUT"/obj/br-*.o \
    $(ls "$TREE"/extra-objects/*.o 2>/dev/null) $flint \
    # $ORIGIN, not the tree path. An absolute rpath is what made the shipped artefact depend on
    # /home/end0/... builds: libggml-cpu.so carried two build directories, one of them an experiment tree,
    # and the checkpoint could not start on its own (BASELINE-DEPENDS-ON-UNTRACKED-ARTEFACT.md). A set that
    # resolves its own directory works from any path, and the libraries it needs must be carried with it -
    # which tests/regression/set-self-contained now checks.
    -Wl,-rpath,'$ORIGIN' \
    "$TREE/libs/libggml.so.0" "$TREE/libs/libggml-base.so.0" \
    "$TREE/libs/libggml-cpu.so.0" "$TREE/libs/libggml-cuda.so.0"

sha256sum "$OUT/libllama.so.0"
echo "built: $OUT/libllama.so.0 ($(stat -c%s "$OUT/libllama.so.0") bytes)"
