#!/usr/bin/env bash
# Build every runtime component from repository sources in an isolated directory.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${1:?usage: build-independent-apex.sh /home/end0/AI/LlamaServer/new-directory}
case "$OUT" in /home/end0/AI/LlamaServer/*) ;; *) echo "build output must be inside LlamaServer"; exit 2;; esac
[ ! -e "$OUT" ] || { echo "refusing to overwrite $OUT"; exit 2; }
mkdir -p "$OUT"
cmake -S "$ROOT/vendor/flint" -B "$OUT/support" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 \
    -DCMAKE_CUDA_HOST_COMPILER=g++-13 -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=89 \
    -DLLAMA_BUILD_APP=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF \
    -DLLAMA_BUILD_COMMON=ON -DLLAMA_BUILD_TOOLS=ON -DLLAMA_BUILD_SERVER=ON \
    -DLLAMA_BUILD_UI=OFF -DLLAMA_USE_PREBUILT_UI=OFF -DFRANKEN_REPO="$ROOT"
cmake --build "$OUT/support" --target llama-server -j "${BUILD_JOBS:-6}"
python3 -u "$ROOT/runtime/reconstructed/build.py" \
    --build-dir "$OUT/core" --runtime-libs "$OUT/support/bin"
mkdir "$OUT/run"
for lib in libggml.so.0 libggml-base.so.0 libggml-cpu.so.0 libggml-cuda.so.0 \
           libllama-common.so.0 libmtmd.so.0 libllama-server-impl.so; do
    cp -L "$OUT/support/bin/$lib" "$OUT/run/$lib"
done
cp "$OUT/support/bin/llama-server" "$OUT/run/llama-server"
cp "$OUT/core/libllama.so.0" "$OUT/run/libllama.so.0"
(cd "$OUT/run" && sha256sum llama-server *.so* > SHA256SUMS)
LD_LIBRARY_PATH="$OUT/run" ldd -r "$OUT/run/llama-server" > "$OUT/dependencies.txt" 2>&1
if grep -q 'undefined symbol\|not found' "$OUT/dependencies.txt"; then
    cat "$OUT/dependencies.txt"; exit 1
fi
printf 'built independent runtime: %s/run\n' "$OUT"
