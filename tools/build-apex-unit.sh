#!/bin/bash
# Build the Apex unit: the common runtime plus the Qwen3.5-MoE architecture builder.
#
# The common unit deliberately excludes architecture builders - they are
# per-architecture alternatives, and composing two of them is a conflict. Apex
# nevertheless needs its builder, because the per-layer structure decisions
# (q35_kind, q35_plan, q35_shape) and the base family decisions (is_recr,
# gating_kind, group_select, the NextN crops) are what a full model graph is made
# of. So the architecture gets a unit of its own rather than a bigger common one.
#
# The module list is derived from tools/build-main-runtime.sh (never typed twice)
# and the architecture modules are appended here.
#
# Usage: bash tools/build-apex-unit.sh [out-dir]
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
OUT=${1:-$ROOT/build/goose-apex}
UNIT=$OUT/main.goose
C=$OUT/main.c
H=$OUT/main.h
OBJ=$OUT/main.o
mkdir -p "$OUT"

# Architecture modules for Apex 35B-A3B. base/model_graph.goose is deliberately
# absent: it is goose-model-graph.goose without its main(), which the common unit
# already carries, and composing both is a conflict. The composer aliases the
# import to the canonical module instead.
ARCH=(
    goose/model/qwen35moe.goose
)

mapfile -t COMMON < <(awk '/^MODULES=\(/{f=1;next} /^\)/{f=0} f && /\.goose/{gsub(/^ +/,"");print}' \
                      "$ROOT/tools/build-main-runtime.sh")
MODULES=("${COMMON[@]}" "${ARCH[@]}")

echo "[1/5] compose ${#MODULES[@]} modules (${#COMMON[@]} common + ${#ARCH[@]} arch)"
python3 "$ROOT/tools/compose-goose.py" "$UNIT" "${MODULES[@]/#/$ROOT/}"

echo "[2/5] typecheck"
"$GOOSE" check "$UNIT" 2>&1 | grep -E "typechecked ok|error" | tail -2

echo "[3/5] generate C"
"$GOOSE" --standalone -o "$C" --header "$H" "$UNIT" >/dev/null

echo "[4/5] compile object at -O1"
"$CC" -O1 -std=c11 -w -c "$C" -o "$OBJ"

echo "[5/5] arch exports present"
for sym in q35_kind q35_plan q35_shape; do
    if nm --defined-only -g "$OBJ" | grep -q " T $sym$"; then
        printf '  %-12s yes\n' "$sym"
    else
        printf '  %-12s MISSING\n' "$sym"
        exit 1
    fi
done

defined=$(nm -g --defined-only "$OBJ" | wc -l)
dups=$(nm -g --defined-only "$OBJ" | awk '{print $3}' | sort | uniq -d | wc -l)
printf 'apex unit ok: %s modules, %s global symbols, %s duplicates\n' \
    "${#MODULES[@]}" "$defined" "$dups"
[ "$dups" -eq 0 ] || exit 1
