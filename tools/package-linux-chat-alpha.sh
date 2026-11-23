#!/usr/bin/env bash
# Package the Linux x86-64 CUDA chat host and its branded web assets.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build=$(cd "${1:?usage: package-linux-chat-alpha.sh build-dir release-tag}" && pwd)
tag=${2:?release tag required}
case "$tag" in *[!a-zA-Z0-9.-]*|'') exit 2;; esac
package="FrankenMoE-$tag-linux-x64"
staging="$build/package/$package"
mkdir -p "$staging/licenses"
binary="$build/frankenmoe-server.bin"
if [[ ! -f "$binary" ]]; then
    binary="$build/frankenmoe-server"
fi
file "$binary" | grep -q 'ELF 64-bit.*x86-64'
install -m 0755 "$binary" "$staging/frankenmoe-server.bin"
install -m 0755 "$root/tools/frankenmoe-server-launch.sh" "$staging/frankenmoe-server"
install -m 0644 "$root/tools/frankenmoe-console.py" "$staging/"
install -m 0644 "$root/LICENSE" "$staging/"
install -m 0644 "$root/vendor/flint/LICENSE" "$staging/licenses/ggml-MIT.txt"
install -m 0644 "$root/runtime/LICENSE.BigMoeOnEdge" "$staging/licenses/BigMoeOnEdge.txt"
test -s "$build/web/index.html"
cp -a "$build/web/." "$staging/web/"
cat > "$staging/README.md" <<'DOC'
# FrankenMoE Linux Chat Alpha

Requires a Linux x86-64 host, a compatible NVIDIA driver and GPU, and the
Apex-35B-V1.0-Q4_K_M GGUF. The model is distributed separately. The build
embeds the CUDA images and uses the installed NVIDIA driver.

Start from the extracted directory. Choose a GPU expert cache that fits
your free VRAM after the model's CUDA allocations.

```bash
./frankenmoe-server \
  --ctx-size 262144 --batch-size 4096 --ubatch-size 4096 \
  --cache-type-k q4_0 --cache-type-v q4_0 --expert-cache-mib 5600 \
  --threads 8 --threads-batch 16 "/path/to/Apex-35B-V1.0-Q4_K_M.gguf" --serve 8080
```

Open http://127.0.0.1:8080/ after the startup summary. For OpenCode use
http://127.0.0.1:8080/v1 as the OpenAI-compatible base URL. Enable tools
and reasoning for the model. The server runs inference. OpenCode
executes approved tools and returns their results to the server.

Chat defaults to 8,192 output tokens and accepts requests up to 131,072
within the context budget. This alpha uses the Apex text chat template.
Structured output, forced tool selection, and multimodal inputs remain
outside the alpha API. See the repository's OpenCode smoke checklist.

On an interactive Linux terminal with Python 3, the launcher opens the dashboard
with startup events, prefill/decode, VRAM, RAM and expert telemetry. Set
FRANKENMOE_CONSOLE=0 for plain server logs, or FRANKENMOE_LOG to a file path to
save the raw log from the dashboard. Redirected output and systems without
Python 3 use the native server directly.
The HTTP log records routes and status codes and omits prompt and output text.
DOC
if find "$staging" \( -type l -o -iname '*.gguf' \) -print -quit | grep -q .; then
    exit 1
fi
tar -C "$build/package" -czf "$build/$package.tar.gz" "$package"
tar -tzf "$build/$package.tar.gz" > /dev/null
(cd "$build" && sha256sum "$package.tar.gz" > SHA256SUMS)
