#!/bin/bash
# Compose the three accepted tokenizer decision modules into ONE goose unit and
# compile it with --standalone into one C translation unit + header.
#
# The goose CLI takes a single source file, so composition is textual: the
# module bodies are concatenated with a banner that names the source of each
# part. The three modules use disjoint helper prefixes (vt_, stp_, cs_) and no
# top-level state, so the concatenation is a plain sequence of declarations.
# tests/check-native-parity.sh verifies that no name is defined twice.
#
#   scripts/compose-goose.sh            generate goose-gen/tokenizer-decisions.{c,h}
#   GOOSE=... OUT=... scripts/compose-goose.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
OUT=${OUT:-$ROOT/goose-gen}

cd "$ROOT"
mkdir -p "$OUT"

composed="$OUT/tokenizer-decisions.goose"
{
    echo "// Composed by scripts/compose-goose.sh. Do not edit; edit the modules."
    echo "//"
    echo "// Part 1/3: goose/vocab-type.goose            -> vt_dispatch"
    echo "// Part 2/3: goose/special-partition.goose     -> stp_partition"
    echo "// Part 3/3: goose/common-strings.goose        -> cs_strip (cs_* family)"
    echo
    cat goose/vocab-type.goose
    echo
    cat goose/special-partition.goose
    echo
    cat goose/common-strings.goose
} > "$composed"

"$GOOSE" --standalone -O2 -o "$OUT/tokenizer-decisions.c" \
         --header "$OUT/tokenizer-decisions.h" "$composed"

wc -c "$composed" "$OUT/tokenizer-decisions.c" "$OUT/tokenizer-decisions.h"
grep -E '^(int64_t|void) [a-z_0-9]+\(' "$OUT/tokenizer-decisions.h" | grep -vc 'goose_init' | sed 's/^/module exports: /'
