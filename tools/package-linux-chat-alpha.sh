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
file "$build/frankenmoe-server" | grep -q 'ELF 64-bit.*x86-64'
install -m 0755 "$build/frankenmoe-server" "$staging/"
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
FRANKENMOE_WEB_ROOT="$PWD/web" ./frankenmoe-server \
  --ctx-size 32768 --batch-size 2048 --ubatch-size 2048 \
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

The console shows model and CUDA allocations, generation progress and
prefill/decode throughput. Set NO_COLOR=1 for unstyled terminal output.
The HTTP log records routes and status codes and omits prompt and output text.
DOC
if find "$staging" \( -type l -o -iname '*.gguf' \) -print -quit | grep -q .; then
    exit 1
fi
tar -C "$build/package" -czf "$build/$package.tar.gz" "$package"
tar -tzf "$build/$package.tar.gz" > /dev/null
(cd "$build" && sha256sum "$package.tar.gz" > SHA256SUMS)
