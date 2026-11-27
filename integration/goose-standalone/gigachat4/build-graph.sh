#!/usr/bin/env bash
set -euo pipefail

src=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
goose=${GOOSE_CUDA_DRIVER_BIN:-${GOOSE_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}}
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
    decoder-graph.goose mtp-graph.goose execute-step.goose graph-config.goose mtp-state.goose dense-placement.goose expert-placement.goose state-placement.goose request-lifecycle.goose step-inputs.goose server-step.goose server-placement.goose model-descriptors.goose tensor-abi.goose request-abi.goose model-abi.goose server-abi.goose > "$out/graph.goose"
"$goose" --standalone --header "$out/graph.h" -o "$out/graph.c" "$out/graph.goose"
for unit in graph server-adapter; do
    source="$src/$unit.c"
    if [[ $unit == graph ]]; then source="$out/graph.c"; fi
    "${CC:-cc}" -O2 -std=gnu11 -Dgoose_init=giga_goose_init -ffunction-sections -fdata-sections -I"$out" -I"$ggml" -I"$runtime/integration/goose-standalone" \
        -c "$source" -o "$out/$unit.o"
done
"${CC:-cc}" -r "$out/graph.o" "$out/server-adapter.o" -o "$out/giga-unit.o"
sed -nE 's/^export fn (giga_abi_(giga_[a-zA-Z0-9_]+))\(.*/\1 \2/p' "$out/graph.goose" > "$out/public-symbols.txt"
"${OBJCOPY:-objcopy}" --redefine-syms="$out/public-symbols.txt" "$out/giga-unit.o"
"${NM:-nm}" -g --defined-only "$out/giga-unit.o" | awk 'NF == 3 && $3 !~ /^giga_/ { print $3 }' > "$out/private-symbols.txt"
"${OBJCOPY:-objcopy}" --localize-symbols="$out/private-symbols.txt" "$out/giga-unit.o"
"${AR:-ar}" rcs "$out/libgiga-goose.new.a" "$out/giga-unit.o"
mv -f -- "$out/libgiga-goose.new.a" "$out/libgiga-goose.a"
