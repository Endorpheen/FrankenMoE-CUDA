#!/bin/bash
# Re-run every module gate that needs only the composed unit and the host primitives.
#
# Why: by EXP983 this series had accumulated a thousand checks across a dozen
# experiments, each verified once in its own cycle. A check that passed yesterday and
# has not been run since is not evidence about today's unit - and every module was
# edited after its own gate (http-listen five times). This script builds the current
# unit once and runs each gate against it, so "still passes" is measured rather than
# remembered.
#
# Gates that need the accepted libllama or cpp-httplib are listed at the end as
# SKIPPED with the reason: their oracle object lives outside the unit, and pretending
# to cover them here would be worse than naming them.
#
# Usage: bash tools/verify-unit-gates.sh [out-dir]
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${1:-"$ROOT/builds/verify-gates"}
GOOSE=$(python3 -c "import json,sys;print(json.load(open('$ROOT/runtime/compiler/current.json'))['compiler'])")
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
HOST_INC=$ROOT/integration/m1/host
SHIM=$ROOT/builds/goose-standalone/inc

mkdir -p "$OUT"
printf 'out:   %s\n' "$OUT"

# ---- 1. compose and build the unit once ----
mapfile -t MODULES < <(sed -n '/^MODULES=(/,/^)/p' "$ROOT/tools/build-main-runtime.sh" | grep -oE 'goose/[a-z0-9/_-]+\.goose')
printf 'unit:  %s modules\n' "${#MODULES[@]}"
( cd "$ROOT" && python3 tools/compose-goose.py "$OUT/unit.goose" "${MODULES[@]}" ) > "$OUT/compose.log" 2>&1
if ! grep -q 'composed' "$OUT/compose.log"; then printf 'compose FAILED\n'; tail -3 "$OUT/compose.log"; exit 1; fi

if ! "$GOOSE" --standalone -o "$OUT/unit.c" --header "$OUT/unit.h" "$OUT/unit.goose" > "$OUT/gen.log" 2>&1; then
    printf 'generate FAILED\n'; tail -3 "$OUT/gen.log"; exit 1
fi
# Function sections are required, not cosmetic: without them --gc-sections keeps every
# host symbol the whole unit references and the gates fail to link. That cost a run.
if ! "$CC" -O1 -ffunction-sections -fdata-sections -std=c11 -w -c "$OUT/unit.c" -o "$OUT/unit.o" > "$OUT/cc.log" 2>&1; then
    printf 'compile FAILED\n'; tail -5 "$OUT/cc.log"; exit 1
fi
# The gates include "runtime.h"; the generated header is named after the unit, so
# publish it under the name they expect rather than editing a dozen tests.
cp -f "$OUT/unit.h" "$OUT/runtime.h"
printf 'build: ok (%s globals, %s duplicates)\n' \
    "$(nm --defined-only --extern-only "$OUT/unit.o" | wc -l)" \
    "$(nm --defined-only --extern-only "$OUT/unit.o" | awk '{print $3}' | sort | uniq -d | wc -l)"

"$CC" -O1 -std=c11 -w -I "$HOST_INC" -I "$SHIM" -c "$ROOT/integration/m1/host/goose_host.c" -o "$OUT/host.o" 2>"$OUT/host.log"
if [ ! -f "$OUT/host.o" ]; then printf 'host primitives FAILED\n'; tail -3 "$OUT/host.log"; exit 1; fi

# ---- 2. the gate table: name | source | needs ----
# "unit": link unit.o + host.o only.  "tls": the same plus pthread for the socket gate.
GATES=(
    "typical        | experiments/EXP-2026-11-18-959-goose-typical/tests/gate.cpp          | unit"
    "top-n-sigma    | experiments/EXP-2026-11-18-960-goose-top-n-sigma/tests/gate.cpp      | unit"
    "dry            | experiments/EXP-2026-11-18-961-goose-dry/tests/gate.cpp              | unit"
    "http-request   | experiments/EXP-2026-11-18-977-goose-http-request/tests/gate.cpp     | httplib"
    "http-listen    | experiments/EXP-2026-11-18-978-goose-http-listen/tests/gate.cpp      | unit"
)

pass=0; fail=0; skip=0
printf '\n%-16s %-8s %s\n' gate result detail
printf '%-16s %-8s %s\n' ---------------- -------- ------
for row in "${GATES[@]}"; do
    IFS='|' read -r name src needs <<< "$row"
    name=$(printf '%s' "$name" | xargs); src=$(printf '%s' "$src" | xargs); needs=$(printf '%s' "$needs" | xargs)

    extra=()
    if [ "$needs" = "httplib" ]; then
        HT=$ROOT/vendor/flint/vendor/cpp-httplib
        HTLIB=$OUT/httplib.o
        if [ ! -f "$HTLIB" ]; then
            "$CXX" -O1 -std=c++17 -w -c "$HT/httplib.cpp" -o "$HTLIB" 2>/dev/null
        fi
        ORA=$ROOT/experiments/EXP-2026-11-18-977-goose-http-request/tests/oracle.cpp
        if [ ! -f "$HTLIB" ] || [ ! -f "$ORA" ]; then
            printf '%-16s %-8s %s\n' "$name" SKIP "no httplib object or oracle available"
            skip=$((skip+1)); continue
        fi
        extra=("$ORA" "$HTLIB" "-I$HT")
    fi

    if ! "$CXX" -O1 -Wl,--gc-sections -I"$OUT" "${extra[@]}" -x c++ "$ROOT/$src" \
            -x none "$OUT/unit.o" "$OUT/host.o" -lm -o "$OUT/g-$name" > "$OUT/link-$name.log" 2>&1; then
        printf '%-16s %-8s %s\n' "$name" LINKFAIL "$(tail -1 "$OUT/link-$name.log")"
        fail=$((fail+1)); continue
    fi
    if timeout 120 "$OUT/g-$name" > "$OUT/run-$name.log" 2>&1; then
        printf '%-16s %-8s %s\n' "$name" PASS "$(grep -oE 'checks=[0-9]+ failures=0' "$OUT/run-$name.log" | tail -1)"
        pass=$((pass+1))
    else
        printf '%-16s %-8s %s\n' "$name" FAIL "$(grep -E '^FAIL|checks=' "$OUT/run-$name.log" | tail -1)"
        fail=$((fail+1))
    fi
done

# ---- 3. named, not silently dropped ----
printf '\nskipped, with the reason (their oracle lives outside the unit):\n'
printf '  %-16s %s\n' "reader-adapter"  "EXP-11-18-953: needs the accepted libllama for the differential"
printf '  %-16s %s\n' "vocab-wire"      "EXP-11-18-954: needs libllama for vocab_only load and tokenize"
printf '  %-16s %s\n' "bpe-engine"      "EXP-11-18-955: needs libllama as the tokenizer oracle"
printf '  %-16s %s\n' "detokenize"      "EXP-11-18-956: needs libllama for 496640 piece comparisons"
printf '  %-16s %s\n' "sampler-live"    "EXP-11-18-966..968: need the accepted libllama sampler objects"
printf '  %-16s %s\n' "sampler-wired"   "EXP-11-18-970/971: need two libllama builds loaded side by side"
printf '\nresult: %d passed, %d failed, %d skipped\n' "$pass" "$fail" "$skip"
[ "$fail" -eq 0 ] || exit 1
