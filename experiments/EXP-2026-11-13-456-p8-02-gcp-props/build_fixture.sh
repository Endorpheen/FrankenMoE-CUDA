#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-456-p8-02-gcp-props
OUT=$F/runtime/exp456/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p "$OUT"
"$G" -o "$E/source/gcp-gen.c" --header "$E/source/gcp-gen.h" "$E/source/gcp.goose"
"$G" -o "$E/source/props-gen.c" --header "$E/source/props-gen.h" "$E/source/props.goose"
if [ ! -f "$OUT/gen-runtime.c" ]; then "$G" --emit-runtime "$OUT/gen-runtime.c"; fi
gcc-13 -O2 -std=c11 -c "$OUT/gen-runtime.c" -o "$OUT/gen-runtime.o"
gcc-13 -O2 -std=c11 -I"$E/source" -c "$E/source/gcp-gen.c" -o "$OUT/gcp.o"
gcc-13 -O2 -std=c11 -I"$E/source" -c "$E/source/props-gen.c" -o "$OUT/props.o"
# both program objects define goose_init; the runtime is initialised once by the
# first module, so the second one's entry is renamed away instead of linked twice
objcopy --redefine-sym goose_init=goose_init_props "$OUT/props.o"
g++-13 -O2 -std=c++17 -I"$E/source" -I"$E/tests" -c "$E/tests/gcp_props_fixture.cpp" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" "$OUT/gcp.o" "$OUT/props.o" "$OUT/gen-runtime.o" -o "$OUT/exp456-fixture"
"$OUT/exp456-fixture"
