#!/usr/bin/env bash
set -euo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
root=$(cd -- "$here/../../.." && pwd)
out=$(realpath -m -- "${1:?Usage: measure.sh EMPTY_OUTPUT_DIR BASELINE_COMMON_OBJECT_DIR}")
baseline=$(realpath -- "${2:?Missing baseline common object directory}")
goose=${GOOSE_BIN:?Set GOOSE_BIN to the KeelWing v0.3.0 preview compiler}
cc=${CC:-gcc-13}
mkdir -p -- "$out"
test -z "$(ls -A -- "$out")"
python3 - "$root" "$out" <<'PY'
from pathlib import Path
import sys

root, out = map(Path, sys.argv[1:])
universal = root / "integration/goose-standalone/universal"
bridge = (universal / "inkling/graph-execute.c").read_text()
cpu = (universal / "cpu/cpu.goose").read_text()
(out / "wrappers.c").write_text(
    '#include "ggml.h"\n#include "dense.h"\n#include <stddef.h>\n#include <string.h>\n'
    + bridge[bridge.index("static void describe_layout"):bridge.index("static sl_u8 borrowed_bytes")]
)
source = (root / "goose/misc/ggml-type-table.goose").read_text()
source += (root / "goose/tensor/tensor-span.goose").read_text()
source += "\n" + cpu[cpu.index('extern "fg143_graph_layout"'):cpu.index('extern "fg143_graph_regular"')]
(out / "dense.goose").write_text(source + '''
extern "ggml_is_contiguous" fn native_dense(tensor: u64) -> bool;
export fn ref_node(tensor: u64, packet: i64[:]) -> bool {
    let answer = native_dense(tensor);
    graph_describe(tensor, packet);
    return answer;
}
export fn goose_node(tensor: u64, packet: i64[:]) -> bool {
    graph_describe(tensor, packet);
    return goose_tensor_dense(packet[38], packet[30 .. 38]);
}
''')
PY
"$goose" --standalone --header "$out/dense.h" -o "$out/dense.c" "$out/dense.goose"
"$cc" -O1 -std=c11 -ffunction-sections -fdata-sections -c "$out/dense.c" -o "$out/dense.o"
for sample in parity bench; do
    "$cc" -O2 -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -I"$root/vendor/flint/ggml/include" -I"$out" "$here/$sample.c" \
        "$out/wrappers.c" "$out/dense.o" "$baseline/ggml.o" "$baseline/ggml-quants.o" \
        -lm -pthread -o "$out/$sample"
done
