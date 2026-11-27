#!/usr/bin/env bash
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
root=$(cd -- "$here/../../.." && pwd)
out=$(realpath -m -- "${1:?Usage: measure.sh EMPTY_OUTPUT_DIR BASELINE_COMMON_OBJECT_DIR}")
base=$(realpath -- "${2:?Missing baseline common object directory}")
goose=${GOOSE_BIN:?Set GOOSE_BIN to the KeelWing v0.3.0 preview compiler}
cc=${CC:-gcc-13}
mkdir -p -- "$out"
test -z "$(ls -A -- "$out")"
python3 - "$root" "$out" <<'PY'
from pathlib import Path
import sys

root, out = map(Path, sys.argv[1:])
universal = root / "integration/goose-standalone/universal"
cpu = (universal / "cpu/cpu.goose").read_text()
bridge = (universal / "inkling/graph-execute.c").read_text()
source = cpu[cpu.index('extern "fg143_graph_node_bytes"'):cpu.index('extern "fg158_prop"')]
(out / "walk.goose").write_text(source + '''
extern "ggml_graph_n_nodes" fn reference_count(graph: u64) -> i32;
extern "ggml_graph_node" fn reference_node(graph: u64, index: i32) -> u64;

export fn goose_node_at(graph: u64, index: i64) -> u64 {
    return graph_node(graph_node_bytes(graph), index);
}

export fn native_walk(graph: u64) -> u64 {
    var result: u64 = 0;
    let count = reference_count(graph) as i64;
    for i in count { result = result ^ reference_node(graph, i as i32); }
    return result;
}

export fn goose_walk(graph: u64) -> u64 {
    var result: u64 = 0;
    let nodes = graph_node_bytes(graph);
    let count = nodes.len / 8;
    var pointers: u64[16] = [0; 16];
    var first: i64 = 0;
    while first < count {
        let remain = count - first;
        let length = if remain < 16 { remain } else { 16 };
        graph_pointer_batch(nodes, first, pointers[..]);
        for i in length { result = result ^ pointers[i]; }
        first += 16;
    }
    return result;
}
''')
(out / "view.c").write_text(
    '#include "ggml-impl.h"\n#include "walk.h"\n#include <string.h>\n'
    + bridge[bridge.index("sl_u8 fg143_graph_node_bytes"):bridge.index("void fg143_graph_inputs")]
)
PY
"$goose" --standalone --header "$out/walk.h" -o "$out/walk.c" "$out/walk.goose"
"$cc" -O1 -std=c11 -ffunction-sections -fdata-sections -c "$out/walk.c" -o "$out/walk.o"
ggml="$root/vendor/flint/ggml"
"$cc" -O2 -std=c11 -D_GNU_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$ggml/include" -I"$ggml/src" -I"$out" "$here/bench.c" "$out/view.c" "$out/walk.o" \
    "$base/ggml.o" "$base/ggml-quants.o" -lm -pthread -o "$out/bench"
