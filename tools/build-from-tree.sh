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

n=0
for src in "$TREE"/our-src/*.cpp; do
    b=$(basename "$src")
    g++-13 -O3 -DNDEBUG -fPIC -std=c++17 -I"$TREE/our-src" -I"$TREE/upstream-include" -I"$TREE/upstream-src" -c "$src" -o "$OUT/obj/our-$b.o" &
    n=$((n+1)); [ $((n % 4)) -eq 0 ] && wait
done
wait
echo "our sources: $(ls "$OUT/obj"/our-*.o | wc -l)"

MODINC=""
for d in "$TREE"/module-src/*/; do MODINC="$MODINC -I$d"; done
for src in "$TREE"/modules/*.c; do
    [ -e "$src" ] || continue
    gcc-13 -O2 -fPIC -I"$TREE/modules" $MODINC -c "$src" -o "$OUT/obj/mod-$(basename "$src" .c).o"
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
    $TREE/extra-objects/*.o $flint \
    -Wl,-rpath,"$TREE/libs" \
    "$TREE/libs/libggml.so.0" "$TREE/libs/libggml-base.so.0" \
    "$TREE/libs/libggml-cpu.so.0" "$TREE/libs/libggml-cuda.so.0"

sha256sum "$OUT/libllama.so.0"
echo "built: $OUT/libllama.so.0 ($(stat -c%s "$OUT/libllama.so.0") bytes)"
