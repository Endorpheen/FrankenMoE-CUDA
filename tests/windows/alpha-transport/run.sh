#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
if [[ -z "${FRANKENMOE_WINDOWS_BUILD:-}" ]]; then
    echo "SKIP Windows Alpha: no cross-built Windows artifact selected"
    exit 0
fi
out=$FRANKENMOE_WINDOWS_BUILD
out=$(cd "$out" && pwd)
cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc-posix}
src="$root/integration/goose-standalone"
"$cc" -O1 -std=c11 -D_WIN32_WINNT=0x0a00 -DWIN32_LEAN_AND_MEAN \
  -I "$out" -I "$src" -I "$root/integration/m1/host" -municode -static -Wl,--gc-sections \
  -Wl,--wrap=fg_q35_generation_prepare,--wrap=fg_q35_generation_eval,--wrap=hose_reset_state \
  -Wl,--wrap=fg_q35_generation_profile,--wrap=fg_http_eog \
  "$here/fixture.c" "$out"/{main-windows,cpu,graph-execute,cuda-driver,graph-bindings,model-host,libc-forwards,slru-shim,goose-host,ggml,ggml-quants,ggml-system,probability,probability-pipeline,platform,web}.o \
  -lws2_32 -lpsapi -lwinpthread -o "$out/transport-fixture.exe"
mkdir -p "$out/transport-fixture"
model="$out/transport-fixture/Модель с пробелами.gguf"
printf 'GGUF\003\000\000\000' > "$model"
export WINEPREFIX="${FRANKENMOE_WINEPREFIX:-$out/wineprefix}" WINEARCH=win64 WINEDEBUG=-all
export FRANKENMOE_WEB_ROOT
FRANKENMOE_WEB_ROOT=$(winepath -w "$out/web")
limit=$(python3 "$here/probe.py" --count)
cd "$out/transport-fixture"
rm -f -- frankenmoe-http.log
timeout 30s wine "$out/transport-fixture.exe" "$(winepath -w "$model")" "$limit" > "$out/transport-fixture/log.txt" 2>&1 &
server=$!
trap 'kill "$server" 2>/dev/null || true' EXIT
python3 "$here/probe.py"
wait "$server"
trap - EXIT
if [[ "${FRANKENMOE_HTTP_LEGACY:-0}" != 1 ]]; then
    ! rg 'PRIVATE-FIXTURE-PROMPT|PRIVATE-FIXTURE-QUERY' frankenmoe-http.log
    if [[ -z "${FRANKENMOE_HTTP_CASES:-}" ]]; then
        rg -q '^POST /v1/chat/completions 200' frankenmoe-http.log
        rg -q '^POST /completion 400 .*send messages to /v1/chat/completions' frankenmoe-http.log
        test "$(grep -c '^warning: response_format is unsupported and ignored' frankenmoe-http.log)" -eq 1
        rg -q 'prefill [0-9]+ tokens \| .*tok/s' log.txt
        rg -q 'decode +[0-9]+ tokens \| .*tok/s' log.txt
    fi
fi
