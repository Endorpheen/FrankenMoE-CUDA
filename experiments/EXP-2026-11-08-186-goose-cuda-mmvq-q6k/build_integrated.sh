#!/usr/bin/env bash
# Rebuild libggml-cuda with the Goose Q6_K matvec linked in (no preload, no entry patching).
# Everything happens under /tmp; the working tree and the active runtime are not touched.
set -euo pipefail
FLINT=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
WORK=/tmp/flint-goose
BUILD=/tmp/build-cuda-fg
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

rsync -a --exclude build-cuda --exclude .git "$FLINT/" "$WORK/"
cd "$WORK/ggml/src/ggml-cuda"
python3 - "$HERE" <<'PY'
import sys
from pathlib import Path
here = Path(sys.argv[1])
p = Path('mmvq.cu'); s = p.read_text()
s = s.replace('void ggml_cuda_mul_mat_vec_q(\n', 'void ggml_cuda_mul_mat_vec_q_native(\n', 1)
p.write_text(s)
h = Path('mmvq.cuh'); t = h.read_text().rstrip()
t += '''

void ggml_cuda_mul_mat_vec_q_native(ggml_backend_cuda_context & ctx,
                                    const ggml_tensor * src0,
                                    const ggml_tensor * src1,
                                    const ggml_tensor * ids,
                                    ggml_tensor * dst,
                                    const ggml_cuda_mm_fusion_args_host * fusion);
'''
h.write_text(t + '\n')
mod = (here / 'source/cuda-runtime.cu').read_text().replace(
    'fg185q_native_ggml_cuda_mul_mat_vec_q(', 'ggml_cuda_mul_mat_vec_q_native(')
Path('fg186q-runtime.cu').write_text(mod)
for f in ('host-unit.c', 'host-qualified.c', 'host-ffi.h', 'device-functions-generated.cuh'):
    (Path('.') / f).write_text((here / 'source' / f).read_text())
c = Path('CMakeLists.txt'); t = c.read_text()
if 'host-unit.c' not in t:
    t = t.replace('    file(GLOB   GGML_SOURCES_CUDA "*.cu")',
                  '    file(GLOB   GGML_SOURCES_CUDA "*.cu")\n    list(APPEND GGML_SOURCES_CUDA "${CMAKE_CURRENT_SOURCE_DIR}/host-unit.c")')
    c.write_text(t)
print('prepared')
PY

cmake -S "$WORK" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=ON \
      -DCMAKE_CUDA_COMPILER=/usr/bin/nvcc -DLLAMA_BUILD_TESTS=OFF \
      -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF >/dev/null
ninja -C "$BUILD" -j 16 ggml-cuda
echo "built: $BUILD/bin/libggml-cuda.so.0.27.0"
