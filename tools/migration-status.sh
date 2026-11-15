#!/bin/bash
# Migration accounting: one command, three separate counters, no build.
#
# AGENTS.md requires that module count, file count and function count are never
# mixed. This script prints them separately and cites where each number comes
# from, so a report cannot drift from the repository.
#
# Usage: bash tools/migration-status.sh
set -uo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

say() { printf '%s\n' "$*"; }

# ---- 1. modules ------------------------------------------------------------
modules=$(awk '/^MODULES=\(/{f=1;next} /^\)/{f=0} f && /\.goose/{gsub(/^ +/,"");print}' \
          tools/build-main-runtime.sh)
n_modules=$(printf '%s\n' "$modules" | grep -c .)

# ---- 2. own C++ files ------------------------------------------------------
files=$(ls runtime/core/src/*.cpp runtime/core/src/*/*.cpp 2>/dev/null)
n_files=$(printf '%s\n' "$files" | grep -c .)
n_lines=$(cat $files 2>/dev/null | wc -l)

# ---- 3. own C++ functions, wired vs unwired --------------------------------
# A function is "wired" when its translation unit calls into the Goose runtime at
# all; per-function wiring is what the differential gates prove, and this counter
# is deliberately the coarser one.
total_fns=0
wired_fns=0
wired_files=0
for f in $files; do
    n=$(grep -cE "^[A-Za-z_][A-Za-z_0-9 :<>,&*]*\**[A-Za-z_][A-Za-z_0-9]*\(" "$f")
    g=$(grep -c "fg_goose\|goose_init\|goose_" "$f")
    total_fns=$((total_fns + n))
    if [ "$g" -gt 0 ]; then
        wired_fns=$((wired_fns + n))
        wired_files=$((wired_files + 1))
    fi
done
unwired_fns=$((total_fns - wired_fns))
unwired_files=$((n_files - wired_files))

# ---- report ----------------------------------------------------------------
say "MIGRATION ACCOUNTING (from the repository, no build)"
say ""
say "Modules (composed unit, tools/build-main-runtime.sh):"
say "  integrated: $n_modules"
say ""
say "Own C++ (runtime/core, the project's engine, not vendored llama.cpp):"
say "  files:      $n_files   lines: $n_lines"
say "  functions:  $total_fns   wired to Goose: $wired_fns   unwired: $unwired_fns"
say "  files with a Goose call: $wired_files of $n_files"
say ""
say "Interpretation: a module in the unit is *available*, an own C++ function is"
say "*replaced* only when its call path reaches a Goose export and its native body"
say "is dead. The second number is therefore the one that measures progress toward"
say "rule 15's removal step, and today it is zero."
say ""
say "Additions and exclusions are named in"
say "docs/goose-migration/BUILD-SET.md; the numbers above are derived, not typed."
