#!/bin/bash
# EXP-2026-11-15-636 mutation run: twelve one-site edits, two halves of six.
# The Goose object is compiled at -O0 (tests/build.sh), the gate is the
# off-grid corpus of tests/fixture.cpp against the native library.
set -uo pipefail
S="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
E="$(cd "$S/.." && pwd)"
OUT="$E/results"
LOG="$OUT/mutations.txt"

: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }

say "EXP636 mutation run - IQ off-grid quantizers"
say "module at -O0, gate: OFFGRID corpus (iq2_s, iq3_xxs) vs native libggml-base"
say ""

say "baseline: $(tests/build.sh fixed > /dev/null 2>&1 && "$OUT/bin/fixed" | tail -1)"

caughtA=0; totalA=0; caughtB=0; totalB=0
while read -r mid half; do
    [ -z "${mid:-}" ] && continue
    if [ "$half" = "A" ]; then totalA=$((totalA+1)); else totalB=$((totalB+1)); fi
    if ! "$S/build.sh" mut "$mid" > "$OUT/work/build-$mid.log" 2>&1; then
        say "$mid $half BUILD-FAILED"
        continue
    fi
    out="$("$OUT/bin/mut-$mid" 2>&1)"; rc=$?
    line="$(echo "$out" | tail -1)"
    if [ $rc -ne 0 ]; then
        verdict=DETECTED
        if [ "$half" = "A" ]; then caughtA=$((caughtA+1)); else caughtB=$((caughtB+1)); fi
    else
        verdict=UNDETECTED
    fi
    mech="$(python3 "$S/mutate.py" list | awk -v id="$mid" '$1==id { $1=""; $2=""; sub(/^  /,""); print }')"
    say "$mid $half $verdict  $line"
    say "      $mech"
done < <(python3 "$S/mutate.py" list | awk '{print $1, $2}')

say ""
say "half A: caught $caughtA/$totalA"
say "half B: caught $caughtB/$totalB"

# restore
"$S/build.sh" fixed > /dev/null 2>&1
say ""
say "after restore: $("$OUT/bin/fixed" | tail -1)"

[ "$caughtA" = "$totalA" ] && [ "$caughtB" = "$totalB" ]
