#!/usr/bin/env bash
# Compare the Goose metadata formatter with a captured native load log.
# Usage: run.sh <model.gguf> <native-load.log> [outdir]
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
GOOSE=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
SUP=/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
MODEL=${1:?model.gguf}
NATIVE=${2:?native load log}
OUT=${3:-/tmp/goose-metadata-format}
mkdir -p "$OUT"

python3 "$ROOT/tools/compose-goose.py" "$OUT/unit.goose" \
    "$ROOT/experiments/EXP-2026-11-17-937-goose-gguf-arrays/source/reader.goose" \
    "$ROOT/goose/model/gguf-format.goose" >/dev/null
( cd "$OUT" && "$GOOSE" --standalone -o unit.c --header unit.h unit.goose >/dev/null )
gcc-13 -O1 -I"$OUT" -I"$ROOT/vendor/flint/ggml/include" -o "$OUT/gate" "$HERE/gate.c" "$OUT/unit.c" \
    "$ROOT/experiments/EXP-2026-11-17-938-goose-http-transport/source/forwards.c" \
    "$ROOT/integration/goose-standalone/graph-bindings.c" \
    "$SUP/libggml-base.so.0" "$SUP/libggml-cpu.so.0" -Wl,-rpath,"$SUP" -lm
"$OUT/gate" "$MODEL" > "$OUT/goose.log" 2>&1

python3 - "$NATIVE" "$OUT/goose.log" <<'PY'
import re, sys
def native(p):
    d = {}
    for line in open(p, errors='replace'):
        m = re.search(r'kv\s+(\d+):\s+(\S+)\s+(\S+)\s+=\s(.*)$', line)
        if m: d[int(m.group(1))] = (m.group(2), m.group(4).rstrip('\n'))
    return d
def goose(p):
    d = {}
    for line in open(p, errors='replace'):
        m = re.match(r'KV (\d+): (.*?) = (.*)$', line.rstrip('\n'))
        if m: d[int(m.group(1))] = (m.group(2), m.group(3))
    return d
a, g = native(sys.argv[1]), goose(sys.argv[2])
bad = [(i, k, v, g.get(i)) for i, (k, v) in a.items() if not g.get(i) or g[i][0] != k or g[i][1] != v]
print(f"entries={len(a)} mismatches={len(bad)}")
for i, k, v, gg in bad[:5]:
    print(f"  kv {i} {k}\n    native: {v!r}\n    goose : {gg[1] if gg else None!r}")
sys.exit(0 if not bad and a else 1)
PY
echo "METADATA_FORMAT OK"
