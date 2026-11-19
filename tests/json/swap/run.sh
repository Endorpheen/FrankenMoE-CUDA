#!/usr/bin/env bash
# Permanent check: the swap in common_json::parse, in all three modes.
#
# This is the only test that exercises the flag path in vendor/flint/common/json.cpp rather than the
# adapter alone:
#
#   1. flag off            - the nlohmann path still produces the same bytes;
#   2. flag on + require   - a passing run proves the Goose port answered, because strict mode turns a
#                            decline into an error instead of a silent fallback;
#   3. flag on + require, no door in the loaded library - every parse must be refused, which proves
#                            the strict mode is not accidentally passing.
#
# Tier: fast - two translation units, no model, no GPU, a few seconds.
#
# Usage: bash tests/json/swap/run.sh [lib-dir]
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"
BIN=${1:-${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin}}

# Compile first, before the library check: the two translation units are the part that can be
# verified anywhere, and a change to json.cpp must not go unchecked just because no built library is
# present. The three runtime modes below need the library and skip without it.
fail=0
{
    printf 'source: %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ g++-13 -O2 -std=c++17 probe.cpp json.cpp json-adapter.cpp -ldl\n'
    g++-13 -O2 -std=c++17 -I"$ROOT/vendor/flint/common" -I"$ROOT/vendor/flint/vendor" \
        -I"$ROOT/vendor/flint/vendor/nlohmann" -I"$ROOT/vendor/flint/ggml/include" \
        -I"$ROOT/vendor/flint/include" -I"$ROOT/vendor/flint/src" \
        "$D/probe.cpp" "$ROOT/vendor/flint/common/json.cpp" "$ROOT/vendor/flint/common/json-adapter.cpp" \
        -o "$OUT/probe" -ldl || exit 1
    printf 'ok: json.cpp and json-adapter.cpp compile\n'

    if [ ! -f "$BIN/libllama.so.0" ]; then
        printf 'skip: no libllama.so.0 under %s - the three runtime modes need it\n' "${BIN#$ROOT/}"
        exit 77
    fi
    printf 'library: %s\n' "${BIN#$ROOT/}"
    printf 'sha256:  %s\n' "$(sha256sum "$BIN/libllama.so.0" | cut -d' ' -f1)"

    printf '\n== 1. flag off\n'
    LD_LIBRARY_PATH="$BIN" "$OUT/probe" "$BIN/libllama.so.0" || fail=1

    printf '\n== 2. flag on, strict (a pass proves the port answered)\n'
    FG_JSON_GOOSE_PARSE=1 FG_JSON_GOOSE_REQUIRE=1 LD_LIBRARY_PATH="$BIN" \
        "$OUT/probe" "$BIN/libllama.so.0" || fail=1

    printf '\n== 3. flag on, strict, no door in the loaded library (must refuse)\n'
    if FG_JSON_GOOSE_PARSE=1 FG_JSON_GOOSE_REQUIRE=1 LD_LIBRARY_PATH="$BIN" \
        "$OUT/probe" "$BIN/libggml-base.so.0" >/dev/null 2>&1; then
        printf 'FAIL: strict mode passed without the door\n'; fail=1
    else
        printf 'ok: strict mode refuses every parse without the door\n'
    fi
    printf '\n[exit %d]\n' "$fail"
    exit "$fail"
} 2>&1 | tee "$LOG"
