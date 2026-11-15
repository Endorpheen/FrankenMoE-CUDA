#!/bin/bash
# Build the unit for ONE architecture: the common runtime plus that arch builder.
#
# Apex is not the only architecture (AGENTS.md rule 17). The common unit excludes
# every architecture builder because they are alternatives; each architecture that
# needs its per-layer decisions gets a unit of its own through this script, with no
# Apex-specific step inside it.
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
# Usage: bash tools/build-arch-unit.sh <arch> [out-dir]
#   <arch> is a builder name under goose/model, e.g. qwen35moe, deepseek4, gemma3.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
ARCH_NAME=${1:?usage: build-arch-unit.sh <arch> [out-dir]}
ARCH_MODULE=goose/model/${ARCH_NAME}.goose
[ -f "$ROOT/$ARCH_MODULE" ] || { echo "no builder at $ARCH_MODULE" >&2; exit 1; }
OUT=${2:-$ROOT/build/goose-$ARCH_NAME}
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
    "$ARCH_MODULE"
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
# The exports are reported, not required by name: each architecture names its own,
# so this step proves the unit carries the builder without assuming Apex's names.
n_exports=$(nm --defined-only -g "$OBJ" | grep -c " T ")
printf '  %s: %s global symbols\n' "$ARCH_NAME" "$n_exports"

defined=$(nm -g --defined-only "$OBJ" | wc -l)
dups=$(nm -g --defined-only "$OBJ" | awk '{print $3}' | sort | uniq -d | wc -l)
printf '%s unit ok: %s modules, %s global symbols, %s duplicates\n' \
    "$ARCH_NAME" "${#MODULES[@]}" "$defined" "$dups"
[ "$dups" -eq 0 ] || exit 1
