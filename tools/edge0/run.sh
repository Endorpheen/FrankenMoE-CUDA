#!/bin/bash
# Build and run the Edge0 pack reader gate on a pack directory.
# Usage: tools/edge0/run.sh [pack-directory]
set -euo pipefail
PACK="${1:-/home/end0/AI/LocalAI/Models/Edge0-120B-q3k}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${TMPDIR:-/tmp}/edge0-pack-test"
g++-13 -O2 -std=c++17 -Wall -Wextra "$HERE/pack.cpp" "$HERE/pack_test.cpp" -o "$OUT"
exec "$OUT" "$PACK"
