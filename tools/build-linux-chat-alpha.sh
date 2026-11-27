#!/usr/bin/env bash
# Build the Linux Alpha HTTP host against a pinned standalone Goose checkpoint.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:?usage: build-linux-chat-alpha.sh out-dir Goose-unit EXP-1805-checkpoint EXP-1786-build-inputs}
unit=${2:?directory containing the cumulative main.goose}
checkpoint=${3:?directory containing the accepted EXP-1805 libfranken-inference.a}
inputs=${4:?directory containing the pinned EXP-1786 build-inputs}
predictor="$root/experiments/EXP-2026-11-23-1805-route-prefetch/source/pred-image.h"
goose=${GOOSE:?set GOOSE to the pinned Fried Goose compiler}
cc=${CC:-gcc-13}
src="$root/integration/goose-standalone"
mkdir -p "$out"
out=$(cd "$out" && pwd)

printf '%s  %s\n' 59fc73ae03a45eae54cc52a6c92617a2dd9b8b0609f1072b60208760e4d62473 \
 "$checkpoint/libfranken-inference.a" | sha256sum --check --status
mkdir -p "$out/apex-driver-inputs"
install -m 0644 "$predictor" "$out/apex-driver-inputs/"
driver_flags=(-O1 -std=c11 -D_GNU_SOURCE -D_FORTIFY_SOURCE=3 -fstack-protector-strong
              -ffunction-sections -fdata-sections -I "$out/apex-driver-inputs" -I "$inputs"
              -I "$root/vendor/flint/ggml/include" -I "$root/vendor/flint/ggml/src"
              -I "$root/integration/m1/host" -I "$src")
"$cc" "${driver_flags[@]}" -c "$src/apex-checkpoint/cuda-driver.c" -o "$out/cuda-driver.o"
cp "$checkpoint/libfranken-inference.a" "$out/libgooseinfer.a"
"${AR:-ar}" rcs "$out/libgooseinfer.a" "$out/cuda-driver.o"

python3 "$root/tools/compose-alpha-runtime.py" "$unit/main.goose" \
 "$root/goose/server/inference-http.goose" "$out/main.goose"
ulimit -s 65536
"$goose" --ast-budget-mib 2048 --header "$out/main.h" -o "$out/main.c" "$out/main.goose"
flags=(-O1 -std=c11 -D_GNU_SOURCE -ffunction-sections -fdata-sections -I "$out"
       -I "$src" -I "$root/integration/m1/host" -I "$root/vendor/flint/ggml/include")
"$cc" "${flags[@]}" -w -c "$out/main.c" -o "$out/main.o"
"$cc" "${flags[@]}" -DFG_CHAT_ALPHA -DFG_APEX_CUMULATIVE -c "$src/cli/infer-main.c" -o "$out/infer-main.o"
"$cc" "${flags[@]}" -c "$src/windows/web.c" -o "$out/web.o"
"$cc" -Wl,--gc-sections "$out"/{infer-main,main,web}.o "$out/libgooseinfer.a" \
 -lm -ldl -pthread -o "$out/gooseinfer-server.bin"
install -m 0755 "$root/tools/gooseinfer-server-launch.sh" "$out/gooseinfer-server"
ln -sfn gooseinfer-server "$out/frankenmoe-server"
ln -sfn gooseinfer-server.bin "$out/frankenmoe-server.bin"
ln -sfn libgooseinfer.a "$out/libfranken-inference.a"
install -m 0644 "$root/tools/gooseinfer-console.py" "$root/tools/frankenmoe-console.py" "$out/"
