#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-453-p8-02-stream-policy
OUT=$F/runtime/exp453/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p "$OUT"
"$G" -o "$E/source/stream-gen.c" --header "$E/source/stream-gen.h" "$E/source/stream.goose"
if [ ! -f "$OUT/gen-runtime.c" ]; then "$G" --emit-runtime "$OUT/gen-runtime.c"; fi
gcc-13 -O2 -std=c11 -c "$OUT/gen-runtime.c" -o "$OUT/gen-runtime.o"
gcc-13 -O2 -std=c11 -I"$E/source" -c "$E/source/stream-gen.c" -o "$OUT/gen.o"
case "stream_fixture.cpp" in
  *.cpp) g++-13 -O2 -std=c++17 -I"$E/source" -c "$E/tests/stream_fixture.cpp" -o "$OUT/fixture.o"
         g++-13 "$OUT/fixture.o" "$OUT/gen.o" "$OUT/gen-runtime.o" -o "$OUT/exp453-fixture" ;;
  *)     gcc-13 -O2 -std=c11 -I"$E/source" -c "$E/tests/stream_fixture.cpp" -o "$OUT/fixture.o"
         gcc-13 "$OUT/fixture.o" "$OUT/gen.o" "$OUT/gen-runtime.o" -o "$OUT/exp453-fixture" ;;
esac
"$OUT/exp453-fixture"
