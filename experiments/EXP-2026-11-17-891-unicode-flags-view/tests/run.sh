#!/usr/bin/env bash
# The normalised classification view against what Unicode says each codepoint is.
#
# Expected classes are written from the characters' meaning, not read back from the same function, and every
# entry is checked to stay inside the documented mask. No model, no build.
set -euo pipefail
E="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$E/../../.." && pwd)"
gcc-13 -O1 "$E/flags.c" -ldl -o "${TMPDIR:-/tmp}/fg_flags_chk"
"${TMPDIR:-/tmp}/fg_flags_chk" "${1:-$ROOT/runtime/goose-stage/lib/libllama.so}"
