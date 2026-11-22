#!/usr/bin/env bash
# Build the Linux Alpha HTTP host against a pinned standalone Goose checkpoint.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:?usage: build-linux-chat-alpha.sh out-dir Goose-unit Linux-checkpoint}
unit=${2:?directory containing the cumulative main.goose}
checkpoint=${3:?directory containing the accepted Linux libfranken-inference.a}
goose=${GOOSE:?set GOOSE to the pinned Fried Goose compiler}
cc=${CC:-gcc-13}
src="$root/integration/goose-standalone"
mkdir -p "$out"
out=$(cd "$out" && pwd)
python3 "$root/tools/compose-alpha-runtime.py" "$unit/main.goose" \
 "$root/goose/server/inference-http.goose" "$out/main.goose"
ulimit -s 65536
"$goose" --ast-budget-mib 2048 --header "$out/main.h" -o "$out/main.c" "$out/main.goose"
flags=(-O1 -std=c11 -D_GNU_SOURCE -ffunction-sections -fdata-sections -I "$out"
       -I "$src" -I "$root/integration/m1/host" -I "$root/vendor/flint/ggml/include")
"$cc" "${flags[@]}" -w -c "$out/main.c" -o "$out/main.o"
"$cc" "${flags[@]}" -DFG_CHAT_ALPHA -c "$src/cli/infer-main.c" -o "$out/infer-main.o"
"$cc" "${flags[@]}" -c "$src/windows/web.c" -o "$out/web.o"
"$cc" -Wl,--gc-sections "$out"/{infer-main,main,web}.o "$checkpoint/libfranken-inference.a" \
 -lm -ldl -pthread -o "$out/frankenmoe-server"
