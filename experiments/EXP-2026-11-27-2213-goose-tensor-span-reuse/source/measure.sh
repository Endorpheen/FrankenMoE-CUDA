#!/usr/bin/env bash
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
root=$(cd -- "$here/../../.." && pwd)
out=$(realpath -m -- "${1:?Usage: measure.sh EMPTY_OUTPUT_DIR BASELINE_COMMON_OBJECT_DIR}")
base=$(realpath -- "${2:?Missing baseline common object directory}")
goose=${GOOSE_BIN:?Set GOOSE_BIN to the KeelWing v0.3.0 preview compiler}
cc=${CC:-gcc-13}
ref="$root/experiments/EXP-2026-11-27-2212-goose-tensor-contiguity/source"
bash "$ref/measure.sh" "$out" "$base"
python3 - "$out/dense.goose" <<'PY'
from pathlib import Path
import sys

p = Path(sys.argv[1])
source = p.read_text().split('extern "ggml_is_contiguous" fn native_dense', 1)[0]
p.write_text(source + '''
extern "ggml_nbytes" fn native_span(tensor: u64) -> u64;
export fn ref_node(tensor: u64, packet: i64[:]) -> u64 {
    let bytes = native_span(tensor);
    graph_describe(tensor, packet);
    return bytes + (if goose_tensor_dense(packet[38], packet[30 .. 38]) { 1 } else { 0 });
}
export fn goose_node(tensor: u64, packet: i64[:]) -> u64 {
    graph_describe(tensor, packet);
    return (packet[39] as! u64) + (if goose_tensor_dense(packet[38], packet[30 .. 38]) { 1 } else { 0 });
}
''')
PY
sed -e 's/uint8_t (\*describe)/uint64_t (*describe)/' \
    -e 's/uint8_t dense = describe/uint64_t dense = describe/' "$ref/bench.c" >"$out/span-bench.c"
"$goose" --standalone --header "$out/dense.h" -o "$out/dense.c" "$out/dense.goose"
"$cc" -O1 -std=c11 -ffunction-sections -fdata-sections -c "$out/dense.c" -o "$out/dense.o"
"$cc" -O2 -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$root/vendor/flint/ggml/include" -I"$out" "$out/span-bench.c" "$out/wrappers.c" "$out/dense.o" \
    "$base/ggml.o" "$base/ggml-quants.o" -lm -pthread -o "$out/span-bench"
