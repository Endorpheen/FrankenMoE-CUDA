#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-454-p8-02-stream-manager
OUT=$F/runtime/exp454/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p "$OUT"
"$G" -o "$E/source/manager-gen.c" --header "$E/source/manager-gen.h" "$E/source/manager.goose"
if [ ! -f "$OUT/gen-runtime.c" ]; then "$G" --emit-runtime "$OUT/gen-runtime.c"; fi
gcc-13 -O2 -std=c11 -c "$OUT/gen-runtime.c" -o "$OUT/gen-runtime.o"
gcc-13 -O2 -std=c11 -I"$E/source" -c "$E/source/manager-gen.c" -o "$OUT/gen.o"
case "manager_fixture.cpp" in
  *.cpp) g++-13 -O2 -std=c++17 -I"$E/source" -c "$E/tests/manager_fixture.cpp" -o "$OUT/fixture.o"
         g++-13 "$OUT/fixture.o" "$OUT/gen.o" "$OUT/gen-runtime.o" -o "$OUT/exp454-fixture" ;;
  *)     gcc-13 -O2 -std=c11 -I"$E/source" -c "$E/tests/manager_fixture.cpp" -o "$OUT/fixture.o"
         gcc-13 "$OUT/fixture.o" "$OUT/gen.o" "$OUT/gen-runtime.o" -o "$OUT/exp454-fixture" ;;
esac
"$OUT/exp454-fixture"
