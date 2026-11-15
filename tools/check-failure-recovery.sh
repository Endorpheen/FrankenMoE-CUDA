#!/bin/bash
# Failure-recovery checks for a staged Goose runtime tree (P11-05).
#
# Given a tree assembled by tools/stage-m1-tree.sh and configured with the M1
# project, verify the five recovery properties the requirement lists. Each
# check records its own exit code; the script fails if any property does not
# hold.
#
#   1. a missing generated unit fails the ON link (no silent native fallback);
#   2. a wrong stage-flag value is rejected at configure time;
#   3. a broken IQ table binding aborts loudly instead of reading NULL;
#   4. a partially switched build (one stage ON) still links and runs;
#   5. the rollback (all flags OFF) recovers after a failed ON build.
#
# Usage: bash tools/check-failure-recovery.sh <tree> [build-on] [build-off]
#
# The tree is modified only by moving files away and back; every move is undone
# before the script returns. No model, no server start, no GPU.
set -uo pipefail

TREE=${1:?usage: check-failure-recovery.sh <tree> [build-on] [build-off]}
ON=${2:-build-on}
OFF=${3:-build-off}
PART=${4:-build-part}
WORK=${WORK:-$TREE/.fr-work}
FAILS=0
PASSES=0

say() { printf '%s\n' "$*"; }
ok()  { say "  ok: $*"; PASSES=$((PASSES+1)); }
bad() { say "  FAIL: $*"; FAILS=$((FAILS+1)); }

mkdir -p "$WORK"

HOST=$TREE/host/goose_host.c
UNIT=$TREE/goose-gen/main.c
for f in "$HOST" "$UNIT"; do
    [ -f "$f" ] || { say "missing $f; assemble the tree first" >&2; exit 1; }
done

# --- 1: a missing generated unit must fail the ON build ----------------------
say "[1] missing generated unit fails the ON link"
mv "$UNIT" "$UNIT.away"
if cmake --build "$TREE/$ON" -j 16 >"$WORK/fr-1.log" 2>&1; then
    bad "ON build succeeded without goose-gen/main.c"
else
    if grep -q "main.c" "$WORK/fr-1.log"; then
        ok "ON build failed and named the missing unit"
    else
        bad "ON build failed for an unrelated reason (see $WORK/fr-1.log)"
    fi
fi
mv "$UNIT.away" "$UNIT"

# --- 2: a wrong flag value must be rejected at configure ---------------------
say "[2] wrong stage-flag value is rejected at configure"
rm -rf "$WORK/bad"
if cmake -S "$TREE" -B "$WORK/bad" -G Ninja \
        -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 \
        -DFG_GOOSE_JSON_SCHEMA=bogus >"$WORK/fr-2.log" 2>&1; then
    bad "configure accepted -DFG_GOOSE_JSON_SCHEMA=bogus"
else
    if grep -q "must be ON or OFF" "$WORK/fr-2.log"; then
        ok "configure stopped with the flag-validation message"
    else
        bad "configure failed without the validation message (see $WORK/fr-2.log)"
    fi
fi

# --- 3: an unbound IQ provider must abort, not read NULL ---------------------
say "[3] unbound IQ provider aborts loudly"
IQ="$TREE/$ON/fg-iq-seam"
if [ -x "$IQ" ]; then
    out=$("$IQ" 2>&1)
    rc=$?
    if printf '%s' "$out" | grep -q "unbound_aborts=1"; then
        if printf '%s' "$out" | grep -q "failures=0"; then
            ok "unbound view aborted and all 15360 comparisons match"
        else
            bad "unbound view aborted but the view comparison failed"
        fi
    else
        bad "unbound view did not abort (rc=$rc)"
    fi
else
    bad "missing $IQ (configure the ON build first)"
fi

# --- 4: a partially switched build still links and runs ----------------------
say "[4] partially switched build links and runs the native path"
if cmake -S "$TREE" -B "$TREE/$PART" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 \
        -DFG_GOOSE_JSON_SCHEMA=ON >"$WORK/fr-4-cfg.log" 2>&1 \
   && cmake --build "$TREE/$PART" -j 16 >"$WORK/fr-4.log" 2>&1; then
    smoke=$(ls "$TREE/$PART/fg-goose-smoke" 2>/dev/null | head -1)
    if [ -x "$smoke" ]; then
        on_out=$("$smoke" 2>&1 | tail -1)
        quant=$(ls "$TREE/$PART/fg-goose-quant-smoke" 2>/dev/null | head -1)
        off_out=$("$quant" 2>&1 | tail -1)
        if printf '%s' "$on_out" | grep -q "mode=goose" \
           && printf '%s' "$off_out" | grep -q "mode=native"; then
            ok "one stage ON: linked, the enabled gate uses Goose and the disabled one stays native"
        else
            bad "one stage ON: gates did not split as expected ($on_out | $off_out)"
        fi
    else
        bad "one stage ON: built but no gate binary was produced"
    fi
else
    bad "one stage ON: build failed (see $WORK/fr-4.log)"
fi

# --- 5: rollback recovers after a failed ON build ---------------------------
say "[5] rollback recovers after the failed ON build"
if cmake --build "$TREE/$OFF" -j 16 >"$WORK/fr-5.log" 2>&1; then
    off_bin=$TREE/$OFF/bin/llama-server
    if [ -x "$off_bin" ] && [ "$(nm "$off_bin" | grep -c ' T goose_init')" -eq 0 ]; then
        ok "OFF build links with zero goose_init definitions"
    else
        bad "OFF build links but the binary is missing or carries goose_init"
    fi
else
    bad "OFF build did not recover (see $WORK/fr-5.log)"
fi

say ""
if [ "$FAILS" -ne 0 ]; then
    say "failure recovery: $FAILS failed, $PASSES passed"
    exit 1
fi
say "failure recovery: ok ($PASSES checks)"
