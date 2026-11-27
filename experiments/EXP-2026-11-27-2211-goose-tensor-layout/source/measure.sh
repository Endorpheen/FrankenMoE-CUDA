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
cat "$root/goose/misc/ggml-type-table.goose" "$root/goose/tensor/tensor-span.goose" >"$out/tensor.goose"
python3 - "$root" "$out" <<'PY'
from pathlib import Path
import subprocess
import sys

root, out = map(Path, sys.argv[1:])
graph = "integration/goose-standalone/universal/inkling/graph-execute.c"
old = subprocess.check_output(
    ["git", "-C", root, "show", f"c03c2ee9b93ee0709b9bde74bded6070bda8251f:{graph}"],
    text=True,
)
new = (root / graph).read_text()

def section(source, start, end):
    first = source.index(start)
    return source[first:source.index(end, first)]

wrappers = '#include "ggml.h"\n#include "desc.h"\n#include <stddef.h>\n#include <string.h>\n'
wrappers += section(old, "static sl_u8 tensor_slice", "static bool float_slice")
wrappers += section(old, "void fg143_graph_describe", "static sl_u8 borrowed_bytes")
wrappers += section(new, "static void describe_layout", "static sl_u8 borrowed_bytes")
(out / "describe-wrappers.c").write_text(wrappers)
cpu = (root / "integration/goose-standalone/universal/cpu/cpu.goose").read_text()
describe = section(cpu, 'extern "fg143_graph_layout"', 'extern "fg143_graph_regular"')
(out / "desc.goose").write_text(
    (out / "tensor.goose").read_text() + "\n" + describe.replace("fn graph_describe(", "export fn graph_describe(")
)
PY
for unit in tensor desc; do
    "$goose" --standalone --header "$out/$unit.h" -o "$out/$unit.c" "$out/$unit.goose"
    opt=-O1
    [[ $unit != tensor ]] || opt=-O2
    "$cc" "$opt" -std=c11 -ffunction-sections -fdata-sections -c "$out/$unit.c" -o "$out/$unit.o"
done
flags=(-O2 -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections
       -I"$root/vendor/flint/ggml/include" -I"$out")
objects=("$baseline/ggml.o" "$baseline/ggml-quants.o")
"$cc" "${flags[@]}" "$here/parity.c" "$out/tensor.o" "${objects[@]}" -lm -pthread -o "$out/parity"
"$cc" "${flags[@]}" "$here/describe-bench.c" "$out/describe-wrappers.c" "$out/desc.o" "${objects[@]}" -lm -pthread -o "$out/describe-bench"
