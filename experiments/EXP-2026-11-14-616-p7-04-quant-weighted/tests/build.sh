#!/bin/bash
# Differential gate build for P7-04 S4: weighted (imatrix) row quantizers.
#
#   tests/build.sh                  build source/quant-weighted.goose and run the gate (-O1)
#   GOOSE_SRC=... OUT_DIR=... OPT=-O0 tests/build.sh    used by tests/mutate.py
#
# The port is compiled to C by goose --standalone and linked into one binary
# with the native oracle: ggml_quantize_chunk and ggml_quantize_init from
# libggml-base (libggml-cpu supplies the CPU init). build/native-tables.c is
# regenerated from the native ggml-quants.c by tests/make-tables.py and always
# built at -O2: it is fixed test infrastructure, not the mutated port, and its
# table construction is slow at -O0.
#
# The NULL-arm regression binary (tests/null-regress.cpp) is built twice: once
# against this port and once against tests/s3b/quant-rows2.goose, the accepted
# S3b source frozen at commit b68cffed (EXP611), and the two SHA-256 digests
# must match. The snapshot is a verbatim copy: the live EXP611 directory is a
# different agent's workspace and must not be read by this gate.
#
# No absolute temporary paths: the experiment root comes from this script.
set -euo pipefail

S=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
L=${LLAMA_DIR:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
SRC=${GOOSE_SRC:-$S/source/quant-weighted.goose}
S3B=${S3B_SRC:-$S/tests/s3b/quant-rows2.goose}
OUT=${OUT_DIR:-$S/build}
OPT=${OPT:--O1}
BIN=$OUT/fg615-weighted

mkdir -p "$OUT"

if [ ! -f "$OUT/native-tables.c" ]; then
    python3 "$S/tests/make-tables.py" "$L/ggml/src/ggml-quants.c" "$OUT/native-tables.c"
fi

echo "[fg615] goose $OPT --standalone $SRC"
"$G" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$SRC" >/dev/null

echo "[fg615] $CC $OPT"
"$CC" "$OPT" -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"

echo "[fg615] $CC -O2 native tables"
"$CC" -O2 -std=c11 -I"$L/include" -I"$L/ggml/include" -I"$L/ggml/src" \
    -c "$OUT/native-tables.c" -o "$OUT/native-tables.o"

echo "[fg615] $CXX $OPT fixture"
"$CXX" "$OPT" -std=c++17 -I"$OUT" -I"$L/include" -I"$L/ggml/include" \
    -c "$S/tests/fixture.cpp" -o "$OUT/fixture.o"

echo "[fg615] link"
"$CXX" "$OUT/fixture.o" "$OUT/gen.o" "$OUT/native-tables.o" \
    -L"$L/build-cuda/bin" -lggml-base -lggml-cpu -lm \
    -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"

echo "[fg615] run"
"$BIN" "${GATE_ARG:---quick}" 2>"$OUT/run.err"

# ---- S3b NULL-arm regression ---------------------------------------------
# A second standalone build of the same fixture source, linked against the
# accepted S3b port, must produce the same digest of all NULL-arm outputs.
if [ -f "$S3B" ] && [ -z "${GOOSE_SRC:-}" ] && [ -z "${OUT_DIR:-}" ]; then
    R=$OUT/null-regress
    mkdir -p "$R"
    cp "$OUT/native-tables.c" "$R/native-tables.c"
    for pair in "new:$SRC" "s3b:$S3B"; do
        name=${pair%%:*}
        src=${pair#*:}
        mkdir -p "$R/$name"
        "$G" --standalone -o "$R/$name/runtime-gen.c" --header "$R/$name/runtime-gen.h" "$src" >/dev/null
        "$CC" -O1 -std=c11 -c "$R/$name/runtime-gen.c" -o "$R/$name/gen.o"
        defs=""
        if [ "$name" = "s3b" ]; then defs="-DS3B_PORT"; fi
        "$CXX" -O1 -std=c++17 $defs -I"$R/$name" -I"$L/include" -I"$L/ggml/include" \
            -c "$S/tests/null-regress.cpp" -o "$R/$name/fx.o"
        "$CXX" "$R/$name/fx.o" "$R/$name/gen.o" "$OUT/native-tables.o" \
            -L"$L/build-cuda/bin" -lggml-base -lggml-cpu -lm \
            -Wl,-rpath,"$L/build-cuda/bin" -o "$R/nr-$name"
    done
    echo "[fg615] S3b NULL-arm regression"
    h_new=$("$R/nr-new")
    h_s3b=$("$R/nr-s3b")
    echo "  new $h_new"
    echo "  s3b $h_s3b"
    if [ "$h_new" != "$h_s3b" ]; then
        echo "[fg615] NULL-arm regression FAILED" >&2
        exit 1
    fi
    echo "[fg615] NULL-arm regression OK"
fi
