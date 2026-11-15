#!/usr/bin/env bash
# P10-06 gate build. Works from any working directory; no absolute paths, the
# experiment root is derived from this script's location.
#
# Layout: <experiment>/source/<slice>.goose, <experiment>/tests/<slice>_fixture.cpp
# Usage:  bash tests/build.sh <arch|tgt|inst> [final|mutate]
#
# final  -O1, gen.o cached by source SHA + optimisation + compiler version
# mutate -O0, always regenerates (a mutation changes the source)
set -euo pipefail
SLICE=${1:-arch}
MODE=${2:-final}
E="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${FG_CC:-gcc-13}
CXX=${FG_CXX:-g++-13}
OUT=${FG_OUT:-$E/build}
case "$SLICE" in
arch) SRC=$E/source/cuda-arch-policy.goose; FIX=$E/tests/arch_fixture.cpp ;;
tgt)  SRC=$E/source/cuda-target-flags.goose; FIX=$E/tests/tgt_fixture.cpp ;;
inst) SRC=$E/source/cuda-install-layout.goose; FIX=$E/tests/inst_fixture.cpp ;;
*) echo "unknown slice: $SLICE" >&2; exit 2 ;;
esac
if [ "$MODE" = "mutate" ]; then OPT=-O0; BIN=$OUT/gate-$SLICE-mutate; else OPT=-O1; BIN=$OUT/gate-$SLICE; fi
NAME=$(basename "$SRC" .goose)
GEN=$OUT/gen-$SLICE
mkdir -p "$OUT" "$GEN"

T0=$(date +%s%N)
# the compiler has no --version flag; its first help line names the build
GV=$("$G" --help 2>/dev/null | head -2 | tr ' ' '_' | tr -d '\n' || true)
KEY=$(sha256sum "$SRC" | cut -d' ' -f1)"-$OPT-${GV:-unknown}"
if [ "$MODE" = "mutate" ] || [ ! -f "$GEN/gen.o" ] || [ "$(cat "$GEN/.key" 2>/dev/null)" != "$KEY" ]; then
    # the header keeps the module name so the fixture's include resolves
    timeout 300 "$G" --standalone -o "$GEN/gen.c" --header "$GEN/$NAME.h" "$SRC" >/dev/null 2>&1
    timeout 300 "$CC" $OPT -std=c11 -w -c "$GEN/gen.c" -o "$GEN/gen.o"
    printf '%s' "$KEY" > "$GEN/.key"
fi
T1=$(date +%s%N)
if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/fix-$SLICE.o" ] || [ "$FIX" -nt "$OUT/fix-$SLICE.o" ]; then
    timeout 300 "$CXX" -O2 -std=c++17 -I"$GEN" -c "$FIX" -o "$OUT/fix-$SLICE.o"
fi
T2=$(date +%s%N)
timeout 300 "$CXX" "$OUT/fix-$SLICE.o" "$GEN/gen.o" -o "$BIN"
T3=$(date +%s%N)
set +e
timeout 600 "$BIN" "$E/tests/cases/$SLICE.tsv" "$E/tests/oracle_$SLICE.py" "$GEN"
RC=$?
set -e
T4=$(date +%s%N)
ms() { echo $(( ($1 - $2) / 1000000 )); }
echo "build slice=$SLICE mode=$MODE opt=$OPT gen_ms=$(ms "$T1" "$T0") fixture_ms=$(ms "$T2" "$T1") link_ms=$(ms "$T3" "$T2") run_ms=$(ms "$T4" "$T3")"
exit $RC
