#!/usr/bin/env bash
set -uo pipefail
D=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd "$D/../../.." && pwd)
. "$ROOT/tests/lib-default.sh"
SET=${1:-$(lib_default "$ROOT")}
LIB=$SET/libllama.so.0
if [ ! -f "$LIB" ]; then
    echo "skip: no libllama in $SET"
    exit 77
fi
SYMBOLS=$(nm -D --defined-only -P "$LIB") || exit 1
for symbol in llama_goose_check goose_sampler_calls fg_goose_host_ensure_init; do
    if ! grep -q "^$symbol " <<<"$SYMBOLS"; then
        echo "skip: $SET does not carry $symbol"
        exit 77
    fi
done

OUT=$(mktemp -d /tmp/franken-path-b-abi.XXXXXX)
echo "path B fixture log: $OUT/run.log"
(
    set -ex
    sha256sum "$LIB"
    g++-13 -O2 -std=c++17 -I"$ROOT/vendor/flint/include" -I"$ROOT/vendor/flint/ggml/include" \
        "$D/probe.cpp" "$LIB" "$SET/libggml-base.so.0" -Wl,-rpath-link,"$SET" -o "$OUT/probe"
    g++-13 -O2 -std=c++17 -I"$ROOT/runtime/reconstructed/src" -I"$ROOT/vendor/flint/include" \
        -I"$ROOT/vendor/flint/ggml/include" "$D/kv-restore.cpp" "$LIB" "$SET/libggml-base.so.0" \
        -Wl,-rpath-link,"$SET" -o "$OUT/kv-restore"
    export LD_LIBRARY_PATH="$SET"
    "$OUT/probe" gguf "$OUT/fixture.gguf"
    "$OUT/kv-restore"
    GGML_GOOSE_KV_SEQPOS=0 "$OUT/kv-restore"
    GGML_GOOSE_SAMPLER=1 GGML_GOOSE_SAMPLER_DRY=1 "$OUT/probe" goose >"$OUT/goose.txt" 2>"$OUT/goose.log" \
        || { cat "$OUT/goose.log"; exit 1; }
    GGML_GOOSE_SAMPLER=1 GGML_GOOSE_SAMPLER_DRY=0 "$OUT/probe" native >"$OUT/native.txt" 2>"$OUT/native.log" \
        || { cat "$OUT/native.log"; exit 1; }
    grep -h '^DRY mode=' "$OUT/goose.log" "$OUT/native.log"
    cmp "$OUT/goose.txt" "$OUT/native.txt"
) 2>&1 | tee "$OUT/run.log"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$OUT/run.log"
exit "$rc"
