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

cat > "$OUT/init-shim.c" <<'SHIM'
// The gates call fg950_goose_init because the shipping DSO renames its initialiser.
// The standalone unit does not, so the suite bridges the name here rather than
// keeping two copies of every gate.
void goose_init(int argc, char ** argv);
void fg950_goose_init(int argc, char ** argv) { goose_init(argc, argv); }
SHIM
"$CC" -O1 -std=c11 -w -c "$OUT/init-shim.c" -o "$OUT/init-shim.o"

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
    "typical-live   | experiments/EXP-2026-11-18-966-goose-typical-live/tests/gate.cpp    | llama"
    "tns-live       | experiments/EXP-2026-11-18-967-goose-top-n-sigma-live/tests/gate.cpp| llama"
    "dry-live       | experiments/EXP-2026-11-18-968-goose-dry-live/tests/gate.cpp       | llama"
    "signal-policy  | experiments/EXP-2026-11-18-995-goose-signal-policy/tests/gate.cpp   | unit"
    "sampler-chain  | experiments/EXP-2026-11-18-998-sampler-chain/tests/gate.cpp         | llama"
    "shipped-vs-unit| experiments/EXP-2026-11-18-999-shipped-vs-unit/tests/gate.cpp      | llama"
    "q35-bridge     | experiments/EXP-2026-11-18-1002-q35-bridge-redundant/tests/gate.cpp | dso"
    "apex-array-plan| experiments/EXP-2026-11-18-1005-apex-array-plan/tests/gate.cpp     | gguf"
)

pass=0; fail=0; skip=0
printf '\n%-16s %-8s %s\n' gate result detail
printf '%-16s %-8s %s\n' ---------------- -------- ------
for row in "${GATES[@]}"; do
    IFS='|' read -r name src needs <<< "$row"
    name=$(printf '%s' "$name" | xargs); src=$(printf '%s' "$src" | xargs); needs=$(printf '%s' "$needs" | xargs)

    # Reset everything a previous gate may have set: a leaked FG_GATE_ARG would pass one
    # gate's file to the next, and a leaked loader path would pick the wrong library.
    extra=(); extra_lib=(); unset FG_GATE_ARG FG_GATE_LDPATH
    if [ "$needs" = "gguf" ]; then
        # Needs a real GGUF: the array branch and the formula branch can agree on a
        # synthetic file and disagree on a real one, which is the case worth testing.
        _g=${FG_APEX_GGUF:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
        LACC=/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
        if [ ! -f "$_g" ]; then
            printf '%-16s %-8s %s\n' "$name" SKIP "no GGUF at $_g"
            skip=$((skip+1)); continue
        fi
        extra=("-I$ROOT/vendor/flint/include" "-I$ROOT/vendor/flint/ggml/include")
        extra_lib=("$LACC/libggml-base.so.0" "$LACC/libggml.so.0" "-Wl,-rpath,$LACC"
                   "$ROOT/builds/goose-standalone/graph-bindings.o"
                   "$ROOT/builds/goose-standalone/tensor-data.o")
        export FG_GATE_ARG="$_g"
    fi
    if [ "$needs" = "dso" ]; then
        # A gate against a DSO built from current sources: the fresh build directory
        # FIRST on the loader path, because with the accepted one first the loader
        # satisfies libllama.so.0 from there and the unit's symbols are missing.
        _freshdso=$(ls -1d /home/end0/AI/LlamaServer/exp*-dso-*/out 2>/dev/null | tail -1 || true)
        LACC=/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
        if [ -z "$_freshdso" ] || [ ! -f "$_freshdso/libllama.so.0" ]; then
            printf '%-16s %-8s %s\n' "$name" SKIP "no DSO built from current sources"
            skip=$((skip+1)); continue
        fi
        export FG_DSO_DIR="$_freshdso"
        extra=(); extra_lib=("$_freshdso/libllama.so.0" "-Wl,-rpath,$_freshdso" "-Wl,--allow-shlib-undefined")
        export FG_GATE_LDPATH="$_freshdso:$LACC"
    fi
    if [ "$needs" = "llama" ]; then
        # The live sampler comparisons need the accepted libllama as their oracle, and
        # its ggml dependencies on the link line.
        LACC=${FG_ACCEPTED_LIB:-/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run}
        if [ ! -f "$LACC/libllama.so.0" ]; then
            printf '%-16s %-8s %s\n' "$name" SKIP "no accepted libllama at $LACC"
            skip=$((skip+1)); continue
        fi
        extra=("-I$ROOT/vendor/flint/include" "-I$ROOT/vendor/flint/ggml/include")
        extra_lib=("$LACC/libllama.so.0" "$LACC/libggml-base.so.0" "$LACC/libggml-cpu.so.0" "$LACC/libggml.so.0"
                   "-Wl,-rpath,$LACC")
    fi
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
        extra=("-I$HT"); extra_lib=("$ORA" "$HTLIB")
    fi

    if ! "$CXX" -O1 -Wl,--gc-sections -I"$OUT" "${extra[@]}" -x c++ "$ROOT/$src" \
            -x none "$OUT/unit.o" "$OUT/host.o" "${extra_lib[@]}" "$OUT/init-shim.o" -lm -o "$OUT/g-$name" > "$OUT/link-$name.log" 2>&1; then
        printf '%-16s %-8s %s\n' "$name" LINKFAIL "$(tail -1 "$OUT/link-$name.log")"
        fail=$((fail+1)); continue
    fi
    # FG_GATE_LDPATH carries the loader order for a DSO gate; it must reach the child,
    # not the script's own environment, so a stale value cannot leak between gates.
    if LD_LIBRARY_PATH="${FG_GATE_LDPATH:-}" timeout 180 "$OUT/g-$name" ${FG_GATE_ARG:-} \
            > "$OUT/run-$name.log" 2>&1; then
        printf '%-16s %-8s %s\n' "$name" PASS "$(grep -oE 'checks=[0-9]+ failures=0' "$OUT/run-$name.log" | tail -1)"
        pass=$((pass+1))
    else
        printf '%-16s %-8s %s\n' "$name" FAIL "$(grep -E '^FAIL|checks=' "$OUT/run-$name.log" | tail -1)"
        fail=$((fail+1))
    fi
done

# ---- 3. a module generated on its own, not through the unit ----
# test-runner.goose was recorded in the ledger as unable to generate standalone C.
# It can, and nobody had called the result, so the gate proves both halves: the C
# compiles and links alone, and the exports answer from C after goose_init.
name="test-runner"
gdir=$OUT/generated-$name
mkdir -p "$gdir"
if "$GOOSE" --standalone -o "$gdir/t.c" --header "$gdir/t.h" "$ROOT/goose/misc/test-runner.goose" \
        > "$OUT/gen-$name.log" 2>&1 \
   && "$CC" -O1 -w -I "$gdir" -I "$ROOT/integration/m1/host" -c \
        "$ROOT/experiments/EXP-2026-11-18-990-test-runner-standalone/tests/gate.c" -o "$gdir/gate.o" \
        >> "$OUT/gen-$name.log" 2>&1 \
   && "$CC" -O1 -w -fPIC -c "$gdir/t.c" -o "$gdir/t.o" >> "$OUT/gen-$name.log" 2>&1 \
   && "$CC" -O1 "$gdir/gate.o" "$gdir/t.o" "$OUT/host.o" -lm -o "$gdir/gate" >> "$OUT/gen-$name.log" 2>&1 \
   && line=$("$gdir/gate" 2>&1 | tail -1); then
    printf '%-16s %-8s %s\n' "$name" "$(echo "$line" | cut -c1-4)" "$(echo "$line" | cut -d' ' -f2-)"
    case "$line" in PASS*) pass=$((pass+1));; *) fail=$((fail+1));; esac
else
    printf '%-16s %-8s %s\n' "$name" GENFAIL "see $OUT/gen-$name.log"
    fail=$((fail+1))
fi

# ---- 4. a DSO built from current sources, when one exists ----
# EXP999 measured that the ACCEPTED library contains none of the canonical unit's
# exports. This asks the different question: is a build from today's sources a working
# library? Without it, "not yet accepted" and "does not build" look the same in a report.
_fresh=$(ls -1d /home/end0/AI/LlamaServer/exp*-dso-*/out 2>/dev/null | tail -1 || true)
if [ -n "$_fresh" ] && [ -f "$_fresh/libllama.so.0" ]; then
    name="fresh-dso"
    if [ ! -x "$_fresh/smoke" ]; then
        $CXX -O1 -w -I "$ROOT/vendor/flint/include" -I "$ROOT/vendor/flint/ggml/include" \
            "$ROOT/experiments/EXP-2026-11-18-1000-fresh-dso/tests/smoke.cpp" -ldl \
            "$_fresh/libllama.so.0" -Wl,-rpath,"$_fresh" -o "$_fresh/smoke" \
            > "$OUT/gen-$name.log" 2>&1
    fi
    # The ggml the DSO needs is next to the build directory's parent, one level up:
    # /home/end0/AI/LlamaServer/<exp>/out -> /home/end0/AI/LlamaServer/...
    _rl=$(dirname "$(dirname "$_fresh")")/apex-independent-fast-20261116/run
    if line=$(LD_LIBRARY_PATH="$_fresh:$(cd "$_rl" 2>/dev/null && pwd)" timeout 120 "$_fresh/smoke" \
            "$_fresh/libllama.so.0" 2>/dev/null | tail -1) && [ -n "$line" ]; then
        printf '%-16s %-8s %s\n' "$name" "$(echo "$line" | cut -c1-4)" \
            "$(echo "$line" | cut -d' ' -f2-) (built from current sources)"
        case "$line" in PASS*) pass=$((pass+1));; *) fail=$((fail+1));; esac
    else
        printf '%-16s %-8s %s\n' "$name" SMOKEFAIL "see $_fresh"
        fail=$((fail+1))
    fi
fi

# ---- 5. a linked object that serves nothing ----
# EXP1001 counted dead symbols; this asks the coarser question and it caught a real
# duplicate: fg-q35-bridge.o provides the same Apex plan decision as fg-q35.o, which is
# generated from the canonical unit, and nothing calls the older one. Reported, not
# removed - rule 15 - so a non-zero result is printed as a finding, not as a suite fail.
_fresh_obj=$(ls -1d /home/end0/AI/LlamaServer/exp*-dso-*/out 2>/dev/null | tail -1 || true)
if [ -n "$_fresh_obj" ] && [ -d "$_fresh_obj" ]; then
    _deadout=$(timeout 200 python3 "$ROOT/tools/check-dead-objects.py" "$_fresh_obj" 2>/dev/null || true)
    _deadtail=$(printf '%s' "$_deadout" | grep '^DEAD' | head -3)
    if [ -n "$_deadtail" ]; then
        printf '%-16s %-8s %s\n' "dead-objects" "FINDING" "$(printf '%s' "$_deadtail" | head -1)"
        printf '%s\n' "$_deadout" | grep '^DEAD' | tail -n +2 | sed 's/^/                        /'
    else
        printf '%-16s %-8s %s\n' "dead-objects" "PASS" "$(printf '%s' "$_deadout" | head -1)"
    fi
fi

# ---- 6. named, not silently dropped ----
printf '\nskipped, with the reason (their oracle lives outside the unit):\n'
printf '  %-16s %s\n' "reader-adapter"  "EXP953: needs the EXP952 candidate libllama (the one with the adapter)"
printf '  %-16s %s\n' "vocab-wire"      "EXP954: needs the EXP953 candidate build AND a generated vocab fixture"
printf '  %-16s %s\n' "bpe-engine"      "EXP955: needs the EXP955 candidate build and the same fixture"
printf '  %-16s %s\n' "detokenize"      "EXP956: needs the EXP956 candidate build and the same fixture"
printf '  %-16s %s\n' "sampler-wired"   "EXP970/971: need TWO libllama builds loaded with dlopen side by side"
printf '\n  These are named because each depends on a build directory outside this\n'
printf '  repository and a fixture; a suite that silently omitted them would report\n'
printf '  less coverage than it has, and one that pretended to run them would lie.\n'
printf '\nresult: %d passed, %d failed, %d skipped\n' "$pass" "$fail" "$skip"
[ "$fail" -eq 0 ] || exit 1
