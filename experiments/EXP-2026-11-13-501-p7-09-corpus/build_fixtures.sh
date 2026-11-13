#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-501-p7-09-corpus
L=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
D=$L/build-cuda/common/CMakeFiles/llama-common.dir
OUT=$F/runtime/exp501/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p "$OUT"

# the three modules under test, unchanged, from their own slices
"$G" --standalone -o "$OUT/gbnf-gen.c" --header "$OUT/gbnf-gen.h" "$E/source/gbnf.goose"
"$G" --standalone -o "$OUT/peg1-gen.c" --header "$OUT/peg1-gen.h" "$E/source/pegparse.goose"
"$G" --standalone -o "$OUT/peg2-gen.c" --header "$OUT/peg2-gen.h" "$E/source/pegparse2.goose"

for m in gbnf peg1 peg2; do
  gcc-13 -O2 -std=c11 -c "$OUT/$m-gen.c" -o "$OUT/$m-gen.o"
done

g++-13 -O2 -std=c++17 -I"$OUT" -I"$L/include" -I"$L/src" -I"$L/ggml/include" -c "$E/tests/gbnf_deep_fixture.cpp" -o "$OUT/gbnf-fixture.o"
g++-13 "$OUT/gbnf-fixture.o" "$OUT/gbnf-gen.o" "$L/build-cuda/src/CMakeFiles/llama.dir/llama-grammar.cpp.o" "$L/build-cuda/bin/libllama.so.0.7.0" "$L/build-cuda/bin/libggml-base.so.0.27.0" -Wl,-rpath,"$L/build-cuda/bin" -o "$OUT/gbnf-deep"

g++-13 -O2 -std=c++17 -I"$OUT" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" -c "$E/tests/peg_deep_fixture.cpp" -o "$OUT/peg1-fixture.o"
g++-13 "$OUT/peg1-fixture.o" "$OUT/peg1-gen.o" "$D/peg-parser.cpp.o" "$D/json-schema-to-grammar.cpp.o" "$D/json-schema.cpp.o" "$D/json.cpp.o" "$D/trie.cpp.o" "$D/unicode.cpp.o" "$L/build-cuda/bin/libllama.so.0.7.0" "$L/build-cuda/bin/libggml-base.so.0.27.0" -Wl,-rpath,"$L/build-cuda/bin" -o "$OUT/peg1-deep"

g++-13 -O2 -std=c++17 -I"$OUT" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" -c "$E/tests/peg2_deep_fixture.cpp" -o "$OUT/peg2-fixture.o"
g++-13 "$OUT/peg2-fixture.o" "$OUT/peg2-gen.o" "$D/peg-parser.cpp.o" "$D/json-schema-to-grammar.cpp.o" "$D/json-schema.cpp.o" "$D/json.cpp.o" "$D/trie.cpp.o" "$D/unicode.cpp.o" "$L/build-cuda/bin/libllama.so.0.7.0" "$L/build-cuda/bin/libggml-base.so.0.27.0" -Wl,-rpath,"$L/build-cuda/bin" -o "$OUT/peg2-deep"

"$OUT/gbnf-deep"
"$OUT/peg1-deep"
"$OUT/peg2-deep"
