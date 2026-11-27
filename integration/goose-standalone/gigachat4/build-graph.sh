#!/usr/bin/env bash
set -euo pipefail

src=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
goose=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
runtime=${GOOSE_RUNTIME_ROOT:-"$src/../../.."}
ggml="$runtime/vendor/flint/ggml/include"
out=${1:-"$src/out"}
mkdir -p -- "$out"
out=$(cd -- "$out" && pwd)

cd -- "$src"
reader=${GOOSE_GGUF_READER:-"$runtime/goose/model/gguf-reader.goose"}
base="$runtime/goose"
cat "$base/misc/gguf-headers.goose" "$base/model/gguf-tensors.goose" "$base/model/gguf-offsets.goose" "$reader" layout.goose tensors.goose gated-norm.goose gdn-projections.goose gdn-layer.goose dsa-indexer.goose \
    mla-projections.goose mla-layer.goose feed-forward.goose decoder-layer.goose \
    decoder-graph.goose mtp-graph.goose execute-step.goose graph-config.goose mtp-state.goose dense-placement.goose expert-placement.goose state-placement.goose > "$out/graph.goose"
"$goose" --standalone --header "$out/graph.h" -o "$out/graph.c" "$out/graph.goose"
sed -i "s/void goose_init(/void giga_goose_init(/" "$out/graph.h"
for unit in graph model-bindings request-state server-adapter; do
    source="$src/$unit.c"
    if [[ $unit == graph ]]; then source="$out/graph.c"; fi
    "${CC:-cc}" -O2 -std=gnu11 -ffunction-sections -fdata-sections -I"$out" -I"$ggml" -I"$runtime/integration/goose-standalone" \
        -c "$source" -o "$out/$unit.o"
done
"${CC:-cc}" -r "$out/graph.o" "$out/model-bindings.o" "$out/request-state.o" "$out/server-adapter.o" -o "$out/giga-unit.o"
"${OBJCOPY:-objcopy}" --redefine-sym goose_init=giga_goose_init "$out/giga-unit.o"
"${NM:-nm}" -g --defined-only "$out/giga-unit.o" | awk 'NF == 3 && $3 !~ /^giga_/ { print $3 }' > "$out/private-symbols.txt"
"${OBJCOPY:-objcopy}" --localize-symbols="$out/private-symbols.txt" "$out/giga-unit.o"
"${AR:-ar}" rcs "$out/libgiga-goose.new.a" "$out/giga-unit.o"
mv -f -- "$out/libgiga-goose.new.a" "$out/libgiga-goose.a"
