#!/usr/bin/env bash
# Build the Goose decision hosts.
#
#   bash tools/goose-hosts/build.sh                 # -O1, into build/goose-hosts
#   bash tools/goose-hosts/build.sh --mutate        # -O0, for a mutation run
#   GOOSE=/path/to/goose bash tools/goose-hosts/build.sh
#
# Each host links one generated unit. The Goose sources are never edited: the
# generated C, the headers and the objects are build artefacts and are not
# committed. The output directory derives from this script's location, so the
# checkout can be moved.
set -euo pipefail

HOSTS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HOSTS/../.." && pwd)"
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CCB=${CC:-gcc-13}
OUT=${BMOE_GOOSE_HOST_OUT:-$ROOT/build/goose-hosts}

OPT=-O1
if [ "${1:-}" = "--mutate" ]; then
    OPT=-O0
fi
mkdir -p "$OUT"

# host <name> <module> [shim]: generate the unit, compile it, link the host.
host() {
    local name=$1 module=$2 shim=${3:-}
    local unit="$OUT/$name-unit.goose"
    cat "$ROOT/$module" > "$unit"
    if [ -n "$shim" ]; then
        cat "$HOSTS/$shim" >> "$unit"
    fi
    timeout 300 "$GOOSE" --standalone -o "$OUT/$name.c" --header "$OUT/$name.h" "$unit" >/dev/null
    "$CCB" "$OPT" -std=c11 -w -c "$OUT/$name.c" -o "$OUT/$name.o"
    "$CCB" "$OPT" -std=c11 -I"$OUT" "$HOSTS/$name-main.c" "$OUT/$name.o" -lm -o "$OUT/$name"
    printf '%s\n' "$OPT" > "$OUT/.opt-$name"
    printf 'built %s %s\n' "$OUT/$name" "$OPT"
}

host art goose/misc/generated-artifact-policy.goose
host env goose/misc/build-env.goose
host pkg goose/misc/release-plan.goose
host fx  goose/misc/fixture-plan.goose
