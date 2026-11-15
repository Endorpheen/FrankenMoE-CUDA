#!/bin/bash
# Staged S1.2 reproduction: compose the GGUF stack, build base / OFF / ON, run
# both gates and the symbol checks. Everything happens inside this staged copy;
# no repository path is written.
#
#   FG_GOOSE_GGUF=OFF (default): the native writer and the native split plan.
#   FG_GOOSE_GGUF=ON           : gguf_write_to_buf, gguf_get_meta_size,
#                                gguf_get_meta_data and should_split go into the
#                                composed Goose unit.
#
# Usage: bash scripts/stage-build.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
J=${J:-32}

cd "$ROOT"
mkdir -p goose-gen logs evidence

# 1. The four canonical GGUF modules -> one composed unit -> one C file.
python3 tools/compose-goose.py composed/gguf-stack.goose \
    goose/model/gguf-metadata.goose \
    goose/model/gguf-writer.goose \
    goose/model/gguf-tensors.goose \
    goose/model/gguf-split.goose

"$GOOSE" --standalone -o goose-gen/gguf-stack.c --header goose-gen/gguf-stack.h \
    composed/gguf-stack.goose 2> logs/goose-generate.log
tail -3 logs/goose-generate.log

# 2. Base: this tree's llama.cpp through its own CMake, no staged project, no
#    Goose. The dispatch lines compile to the native call, so this proves the
#    staged edits are inert without the flag.
cmake -S llama.cpp -B build-base -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DGGML_CUDA=OFF -DGGML_NATIVE=ON -DLLAMA_CURL=OFF -DLLAMA_OPENSSL=OFF \
      -DLLAMA_SUBPROCESS=OFF -DLLAMA_BUILD_COMMON=ON -DLLAMA_BUILD_TESTS=OFF \
      -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF -DLLAMA_BUILD_TOOLS=OFF \
      -DLLAMA_BUILD_APP=OFF > logs/base-configure.log 2>&1
cmake --build build-base -j "$J" --target llama llama-common > logs/base-build.log 2>&1

# 3. Staged builds through this directory's CMakeLists; the flag selects the path.
for mode in OFF ON; do
    cmake -S . -B "build-${mode,,}" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
          -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_GGUF="$mode" > "logs/${mode,,}-configure.log" 2>&1
    cmake --build "build-${mode,,}" -j "$J" \
          --target fg-gguf-smoke fg-gguf-make fg-gguf-split > "logs/${mode,,}-build.log" 2>&1
done

# 4. Writer gate: 14 synthetic contexts, 41 byte/size comparisons per build.
./build-off/fg-gguf-smoke | tee evidence/smoke-off.txt
./build-on/fg-gguf-smoke  | tee evidence/smoke-on.txt

# 5. Split-plan gate: OFF against ON, whole shard files and plan text.
bash scripts/split-gate.sh "${SPLIT_WORK:-/tmp/fg630-split}" | tee evidence/split-gate.txt

# 6. Mutations: 12 changes to the composed unit, two halves of six, each built
#    at -O0 in its own tree and run through both gates (MUTATIONS=0 skips them).
if [ "${MUTATIONS:-1}" = "1" ]; then
    python3 tests/gguf_mutate.py --jobs "$J" | grep -E "^(A|B)[0-9]|^half|^total"
fi

# 7. Symbols: one definition per Goose export in ON, none in OFF.
{
    echo "--- definitions in the ON library"
    for s in s1_init s2_write s3_write s3_add_tensor gsp_should_split gsp_split_layout goose_init; do
        printf '%-18s ON=%s OFF=%s\n' "$s" \
            "$(nm build-on/bin/libggml-base.so  | grep -cE " [TtWw] $s$")" \
            "$(nm build-off/bin/libggml-base.so | grep -cE " [TtWw] $s$")"
    done
    echo "--- any reference in the OFF library"
    nm build-off/bin/libggml-base.so | grep -E " (s1_|s2_|s3_|gsp_|goose_init)" || echo "(none)"
} | tee evidence/nm-gates.txt

# 8. Hashes.
sha256sum goose/model/gguf-*.goose composed/gguf-stack.goose \
          goose-gen/gguf-stack.c goose-gen/gguf-stack.h \
          common/fg-goose-gguf-bridge.h common/fg-goose-gguf-bridge.cpp \
          common/fg-goose-gguf-adapter.inl llama.cpp/ggml/src/fg-goose-gguf-native.h \
          tests/fg_gguf_smoke.cpp tests/fg_gguf_make.cpp tests/gguf_mutate.py \
          scripts/stage-build.sh scripts/split-gate.sh CMakeLists.txt \
          > evidence/sha-sources.txt
sha256sum build-base/bin/libggml-base.so.* build-off/bin/libggml-base.so.* \
          build-on/bin/libggml-base.so.* build-off/fg-gguf-smoke build-on/fg-gguf-smoke \
          build-off/fg-gguf-split build-on/fg-gguf-split \
          build-on/CMakeFiles/fg-goose-gguf.dir/goose-gen/gguf-stack.c.o \
          build-mut/fg-gguf-smoke build-mut/fg-gguf-split \
          > evidence/sha-artifacts.txt
cat evidence/sha-sources.txt evidence/sha-artifacts.txt
