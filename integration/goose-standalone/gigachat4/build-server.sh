#!/usr/bin/env bash
set -euo pipefail

src=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
runtime=$(cd -- "${GOOSE_RUNTIME_ROOT:-"$src/../../.."}" && pwd)
out=${1:-"$src/out"}
mkdir -p -- "$out"
out=$(cd -- "$out" && pwd)
host="$runtime/integration/goose-standalone"
unit="$out/runtime-unit"
base="$runtime/vendor/flint/ggml"
compiler=${CC:-gcc-13}

bash "$src/build-graph.sh" "$out"
if [[ ! -s "$unit/main.o" ]]; then
    bash "$runtime/tools/build-main-runtime.sh" "$unit" > "$out/runtime-unit.log" 2>&1
fi
archive="$out/runtime/libfranken-inference.a"
if [[ ! -s "$archive" || "$unit/main.o" -nt "$archive" ]]; then
    CC="$compiler" bash "$runtime/tools/build-goose-inference.sh" "$out/runtime" "$unit" > "$out/runtime.log" 2>&1 ||
        [[ -s "$archive" && ! "$unit/main.o" -nt "$archive" ]]
fi
patch -s "$host/goose-host-llm.c" "$src/server-dispatch.patch" -o "$out/giga-host.c"
flags=(-D_GNU_SOURCE -std=c11 -O1 -ffunction-sections -fdata-sections
       -I"$src" -I"$host" -I"$unit" -I"$base/include" -I"$runtime/integration/m1/host")
"$compiler" "${flags[@]}" -c "$out/giga-host.c" -o "$out/giga-host.o"
"$compiler" "${flags[@]}" -c "$host/windows/web.c" -o "$out/web.o"
"$compiler" "${flags[@]}" -Wl,--gc-sections "$host/cli/infer-main.c" "$out/giga-host.o" \
    "$out/libgiga-goose.a" "$out/web.o" "$archive" -lm -ldl -pthread \
    -o "$out/gooseinfer-gigachat4-infer"
