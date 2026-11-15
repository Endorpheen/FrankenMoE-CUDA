#!/bin/bash
# Staged S1.3 reproduction: compose the Goose quant stack, build the base tree,
# build the staged tree with the flag OFF and ON, run the byte gate, compare
# the two builds, and count the Goose definitions in the libraries.
# Everything happens inside this staged copy; no repository path is written.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
J=${J:-32}
E="$ROOT/evidence"

cd "$ROOT"
mkdir -p goose-gen "$E"

# 1. Compose the seven canon modules into one Goose unit, then one C unit.
#    goose/model holds the canon files byte for byte; the staged fixes in
#    patches/ are applied to a copy, so the canon input stays untouched.
rm -rf goose/model-staged
cp -a goose/model goose/model-staged
patch goose/model-staged/quant-dequant.goose < patches/quant-dequant-signed-zero.diff \
    | tee "$E/q-patch.txt"
python3 scripts/compose-quant.py goose/model-staged goose-gen/quant-stack.goose \
    | tee "$E/q-compose.txt"
"$GOOSE" check goose-gen/quant-stack.goose > "$E/q-goose-check.txt" 2>&1
grep -vE 'warning: redundant' "$E/q-goose-check.txt" | tail -3
"$GOOSE" --standalone -o goose-gen/quant-stack.c \
         --header goose-gen/quant-stack.h goose-gen/quant-stack.goose \
         > "$E/q-goose-standalone.txt" 2>&1
grep -vE 'warning: redundant' "$E/q-goose-standalone.txt" | tail -3

# 2. Base: pristine llama.cpp, CPU only, no flag, no Goose.
cmake -S llama.cpp -B build-base -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DGGML_CUDA=OFF -DGGML_NATIVE=ON -DLLAMA_CURL=OFF -DLLAMA_OPENSSL=OFF \
      -DLLAMA_SUBPROCESS=OFF -DLLAMA_BUILD_COMMON=ON -DLLAMA_BUILD_TESTS=OFF \
      -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF -DLLAMA_BUILD_TOOLS=OFF \
      -DLLAMA_BUILD_APP=OFF > "$E/q-base-configure.log"
cmake --build build-base -j "$J" --target ggml > "$E/q-base-build.log" 2>&1

# 3. Staged builds through this directory's CMakeLists.
for mode in OFF ON; do
    cmake -S . -B "build-${mode,,}" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
          -DFG_GOOSE_QUANT="$mode" > "$E/q-${mode,,}-configure.log"
    cmake --build "build-${mode,,}" -j "$J" \
          --target fg-goose-quant-smoke fg-goose-quant-arms \
          > "$E/q-${mode,,}-build.log" 2>&1
done

# 4. Gate: 12 q4_0 dequant cases, 12 q8_0 quant cases, 10 block sizes, each
#    compared byte for byte with the native body.
./build-off/fg-goose-quant-smoke | tee "$E/q-smoke-off.txt"
./build-on/fg-goose-quant-smoke  | tee "$E/q-smoke-on.txt"

# 4b. Deep arm gate: the chunk and dequant entries behind the switched ones,
#     against the native library. The mutation run (tests/quant_mutate.py)
#     needs it; the smoke numbers above do not depend on it.
./build-off/fg-goose-quant-arms | tee "$E/q-arms-off.txt"
./build-on/fg-goose-quant-arms  | tee "$E/q-arms-on.txt"

# 5. Cross-build parity: the per-case digests must be the same file.
grep '^case=' "$E/q-smoke-off.txt" > "$E/q-dump-off.txt"
grep '^case=' "$E/q-smoke-on.txt"  > "$E/q-dump-on.txt"
if diff -q "$E/q-dump-off.txt" "$E/q-dump-on.txt" > /dev/null; then
    echo "OFF/ON case digests identical ($(wc -l < "$E/q-dump-on.txt") cases)"
else
    echo "OFF/ON case digests DIFFER"
    diff "$E/q-dump-off.txt" "$E/q-dump-on.txt" | head -20
    exit 1
fi

# 5b. The flag off must be the production library: base and OFF identical.
{
    sha256sum build-base/bin/libggml-base.so.0.22.0 build-off/bin/libggml-base.so.0.22.0
    if cmp build-base/bin/libggml-base.so.0.22.0 build-off/bin/libggml-base.so.0.22.0; then
        echo "cmp: base and OFF libraries are bit identical"
    else
        echo "cmp: base and OFF libraries DIFFER"
    fi
} > "$E/q-base-vs-off.txt"
tail -1 "$E/q-base-vs-off.txt"

# 5c. The dispatch is in the code, not only in the counters.
objdump -d --no-show-raw-insn build-on/bin/libggml-base.so \
    | awk '/<ggml_blck_size>:/{f=1} f{print} f&&/^$/{exit}' \
    > "$E/q-disasm-blck.txt" || true
objdump -d --no-show-raw-insn build-on/bin/libggml-base.so \
    | awk '/<dequantize_row_q4_0>:/{f=1} f{print} f&&/^$/{exit}' \
    > "$E/q-disasm-dequant.txt" || true
objdump -d --no-show-raw-insn build-on/bin/libggml-base.so \
    | awk '/<quantize_row_q8_0_ref>:/{f=1} f{print} f&&/^$/{exit}' \
    | grep -E 'call|test|je|jne|jmp' > "$E/q-disasm-quant.txt" || true
grep -h 'fg_goose_quant' "$E"/q-disasm-*.txt

# 6. Symbols: one definition of each Goose export in the ON library, none in
#    the OFF library, and the module's other exports on the same rule.
for lib in on off; do
    nm "build-${lib}/bin/libggml-base.so" \
        | grep -E ' [TtWw] (qz2?|qd|qw|qt|qa|qr|goose)_[A-Za-z0-9_]*$' \
        | sort > "$E/q-nm-${lib}.txt" || true
done
echo "--- goose exports defined in libggml-base.so"
printf 'ON  %s\n' "$(wc -l < "$E/q-nm-on.txt")"
printf 'OFF %s\n' "$(wc -l < "$E/q-nm-off.txt")"
for sym in qz_dequant qz_quantize_row qz_blck qd_dequant qw_quantize_chunk_im goose_init; do
    on=$(nm "build-on/bin/libggml-base.so"  | grep -cE " [TtWw] ${sym}\$" || true)
    off=$(nm "build-off/bin/libggml-base.so" | grep -cE " ${sym}\$" || true)
    printf '%-22s ON=%s OFF=%s\n' "$sym" "$on" "$off"
    echo "$sym ON=$on OFF=$off" >> "$E/q-nm-counts.txt"
done

# 7. SHA-256 of inputs, sources and artefacts.
{
    for f in goose/model/quant-types.goose goose/model/quant-allow.goose \
             goose/model/quant-rows.goose goose/model/quant-rows2.goose \
             goose/model/quant-dequant.goose goose/model/quant-dequant2.goose \
             goose/model/quant-weighted.goose; do sha256sum "$f"; done
    sha256sum goose-gen/quant-stack.goose scripts/compose-quant.py \
              common/fg-goose-quant-bridge.c common/fg-goose-quant-bridge.h \
              tests/fg_goose_quant_smoke.c tests/fg_goose_quant_arms.c \
              tests/quant_mutate.py CMakeLists.txt \
              patches/*.diff
    sha256sum build-off/bin/libggml-base.so.0.22.0 \
              build-on/bin/libggml-base.so.0.22.0
    sha256sum build-off/fg-goose-quant-smoke build-on/fg-goose-quant-smoke \
              build-off/fg-goose-quant-arms build-on/fg-goose-quant-arms
    sha256sum build-on/CMakeFiles/fg-goose-quant.dir/goose-gen/quant-stack.c.o
    sha256sum evidence/q-mutations.txt
} > "$E/q-sha256.txt"

echo "done: evidence/ updated"
