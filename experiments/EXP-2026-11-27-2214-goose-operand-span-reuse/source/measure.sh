#!/usr/bin/env bash
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
root=$(cd -- "$here/../../.." && pwd)
out=$(realpath -m -- "${1:?Usage: measure.sh EMPTY_OUTPUT_DIR BASELINE_COMMON_OBJECT_DIR}")
base=$(realpath -- "${2:?Missing baseline common object directory}")
goose=${GOOSE_BIN:?Set GOOSE_BIN to the KeelWing v0.3.0 preview compiler}
cc=${CC:-gcc-13}
bash "$root/experiments/EXP-2026-11-27-2213-goose-tensor-span-reuse/source/measure.sh" "$out" "$base"
python3 - "$root" "$out" <<'PY'
from pathlib import Path
import sys

root, out = map(Path, sys.argv[1:])
universal = root / "integration/goose-standalone/universal"
bridge = (universal / "inkling/graph-execute.c").read_text()
cpu = (universal / "cpu/cpu.goose").read_text()
with (out / "wrappers.c").open("a") as stream:
    stream.write(bridge[bridge.index("void fg143_graph_inputs"):bridge.index("static void describe_layout")])
    stream.write("""
uint64_t benchmark_tensor_data(uint64_t address) {
    const struct ggml_tensor *tensor = (const void *)(uintptr_t) address;
    return (uint64_t)(uintptr_t)tensor->data;
}
""")
p = out / "dense.goose"
source = p.read_text().split("export fn ref_node(", 1)[0]
source += cpu[cpu.index("fn activation_spans_overlap"):cpu.index("export fn fg143_activation_lookup")]
source += '''
extern "fg143_graph_inputs" fn test_inputs(tensor: u64, sources: u64[:]);
extern "benchmark_tensor_data" fn test_data(tensor: u64) -> u64;
'''
for name, span in (
    ("ref_node", "native_span(t)"),
    ("goose_node", "if i < 3 { packet[9 + 10 * i] as! u64 } else if i == 3 { packet[68] as! u64 } else { native_span(t) }"),
):
    source += f'''export fn {name}(tensor: u64, packet: i64[:]) -> u64 {{
    graph_describe(tensor, packet);
    let address = test_data(tensor);
    let bytes = packet[39] as! u64;
    var sources: u64[10] = [0; 10];
    test_inputs(tensor, sources[..]);
    for i in sources.len {{
        let t = sources[i];
        if t == 0 {{ continue; }}
        let src = test_data(t);
        let source_bytes = {span};
        if activation_spans_overlap(address, bytes, src, source_bytes) {{ return 0; }}
    }}
    return 1;
}}
'''
p.write_text(source)
PY
"$goose" --standalone --header "$out/dense.h" -o "$out/dense.c" "$out/dense.goose"
"$cc" -O1 -std=c11 -ffunction-sections -fdata-sections -c "$out/dense.c" -o "$out/dense.o"
"$cc" -O2 -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$root/vendor/flint/ggml/include" -I"$out" "$here/bench.c" "$out/wrappers.c" "$out/dense.o" \
    "$base/ggml.o" "$base/ggml-quants.o" -lm -pthread -o "$out/operand-bench"
