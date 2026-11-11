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
if 'if (!ids && dst->ne[1] > 1) {' not in mod:
    anchor = ' const char*flag=getenv("GGML_GOOSE_CUDA_MMVQ_Q6K");'
    loop = ''' if (!ids && dst->ne[1] > 1) {
     for (int64_t c = 0; c < dst->ne[1]; ++c) {
         ggml_tensor s1 = *src1, d1 = *dst;
         s1.data = (char *) src1->data + c * src1->nb[1];
         d1.data = (char *) dst->data  + c * dst->nb[1];
         s1.ne[1] = 1; d1.ne[1] = 1;
         ggml_cuda_mul_mat_vec_q(ctx, src0, &s1, nullptr, &d1, fusion);
     }
     return;
 }
'''
    mod = mod.replace(anchor, loop + anchor)
if '_Alignas(x) alignas(x)' not in mod:
    mod = mod.replace('#include "device-functions-generated.cuh"',
        '#if defined(__cplusplus) && !defined(_Alignas)\n#define _Alignas(x) alignas(x)\n#endif\n#include "device-functions-generated.cuh"')
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


# --- FG196P (CUDA_CONTROL pinned-memory policy), same pattern ---
ROOT="$(cd "$HERE/../.." && pwd)"
export E196_DIR="$ROOT/experiments/EXP-2026-11-08-196-goose-cuda-pinned-pool"
python3 - <<'PY2'
from pathlib import Path
import re
flint = Path('/tmp/flint-goose/ggml/src/ggml-cuda')
import os
src = Path(os.environ['E196_DIR']) / 'source/shared-cuda.cu'
lines = src.read_text().splitlines()
def grab(marker):
    for i, l in enumerate(lines):
        if marker in l:
            depth = 0; out = []
            for j in range(i, len(lines)):
                out.append(lines[j]); depth += lines[j].count('{') - lines[j].count('}')
                if depth == 0 and j > i: return out
    return []
blocks = [grab('static void * ggml_cuda_host_malloc(size_t size) {'),
          grab('static ggml_backend_buffer_t ggml_backend_cuda_host_buffer_type_alloc_buffer('),
          grab('bool ggml_backend_cuda_register_host_buffer(void * buffer, size_t size) {'),
          grab('void ggml_backend_cuda_unregister_host_buffer(void * buffer) {')]
body = '\n\n'.join('\n'.join(b) for b in blocks)
body = body.replace('bool ggml_backend_cuda_register_host_buffer(', 'bool ggml_backend_cuda_register_host_buffer_goose(')
body = body.replace('void ggml_backend_cuda_unregister_host_buffer(', 'void ggml_backend_cuda_unregister_host_buffer_goose(')
c = flint / 'ggml-cuda.cu'; t = c.read_text()
for n in ['ggml_cuda_host_malloc', 'ggml_backend_cuda_host_buffer_type_alloc_buffer',
          'ggml_backend_cuda_register_host_buffer', 'ggml_backend_cuda_unregister_host_buffer']:
    t = t.replace(f'void * {n}(', f'void * {n}_native(')
    t = t.replace(f'ggml_backend_buffer_t {n}(', f'ggml_backend_buffer_t {n}_native(')
    t = t.replace(f'bool {n}(', f'bool {n}_native(')
    t = t.replace(f'void {n}(void * buffer) {{', f'void {n}_native(void * buffer) {{')
marker = '#include "ggml-cuda.h"'
if 'static void * ggml_cuda_host_malloc(size_t size);' not in t:
    t = t.replace(marker, marker + '\nstatic void * ggml_cuda_host_malloc(size_t size);\nstatic ggml_backend_buffer_t ggml_backend_cuda_host_buffer_type_alloc_buffer(ggml_backend_buffer_type_t buft, size_t size);', 1)
t = t.rstrip() + '\n\n// ---- Goose pinned policy (EXP196) ----\nextern "C" int fg196p_enabled(void);\nextern "C" void fg196p_note_native(void);\nextern "C" void fg196p_plan_host_alloc(int64_t,int64_t,uint64_t);\nextern "C" void fg196p_plan_host_alloc_result(int64_t,uint64_t);\nextern "C" void fg196p_plan_fallback(int64_t,uint64_t);\nextern "C" void fg196p_plan_register(int64_t,int64_t,uint64_t);\nextern "C" void fg196p_plan_register_result(int64_t,uint64_t);\nextern "C" void fg196p_plan_unregister(int64_t,int64_t,uint64_t);\n' + body + '\nbool ggml_backend_cuda_register_host_buffer(void * buffer, size_t size) { return ggml_backend_cuda_register_host_buffer_goose(buffer, size); }\nvoid ggml_backend_cuda_unregister_host_buffer(void * buffer) { ggml_backend_cuda_unregister_host_buffer_goose(buffer); }\n'
c.write_text(t)
b = flint / 'fg196p-bridge.c'
b.write_text((Path(str(src).replace('shared-cuda.cu','bridge.c'))).read_text().replace('#include "host-generated.c"', '#include "fg196p-host-generated.c"'))
(flint / 'fg196p-host-generated.c').write_text(Path(str(src).replace('shared-cuda.cu','host-generated.c')).read_text())
cm = flint / 'CMakeLists.txt'; ct = cm.read_text()
if 'fg196p-bridge.c' not in ct:
    ct = ct.replace('    list(APPEND GGML_SOURCES_CUDA "${CMAKE_CURRENT_SOURCE_DIR}/host-unit.c")',
                    '    list(APPEND GGML_SOURCES_CUDA "${CMAKE_CURRENT_SOURCE_DIR}/host-unit.c")\n    list(APPEND GGML_SOURCES_CUDA "${CMAKE_CURRENT_SOURCE_DIR}/fg196p-bridge.c")')
    cm.write_text(ct)
print('fg196p prepared')
PY2


# --- FG195V (CUDA_CONTROL VMM pool arithmetic), same pattern ---
export E195_DIR="$ROOT/experiments/EXP-2026-11-08-195-goose-cuda-vmm-pool"
python3 - <<'PY3'
import os
from pathlib import Path
flint = Path('/tmp/flint-goose/ggml/src/ggml-cuda')
src = Path(os.environ['E195_DIR']) / 'source/shared-cuda.cu'
lines = src.read_text().splitlines()
def grab(marker):
    for i, l in enumerate(lines):
        if marker in l:
            depth = 0; out = []
            for j in range(i, len(lines)):
                out.append(lines[j]); depth += lines[j].count('{') - lines[j].count('}')
                if depth == 0 and j > i: return out
    return []
alloc = grab('    void * alloc(size_t size, size_t * actual_size) override {')
free = grab('    void free(void * ptr, size_t size) override {')
c = flint / 'ggml-cuda.cu'; s = c.read_text()
def body(sig):
    i = s.index(sig); depth = 0
    for j in range(i, len(s)):
        if s[j] == '{': depth += 1
        elif s[j] == '}':
            depth -= 1
            if depth == 0: return i, j + 1
i1, j1 = body('    void * alloc(size_t size, size_t * actual_size) override {')
i2, j2 = body('    void free(void * ptr, size_t size) override {')
s = s[:i1] + '\n'.join(alloc) + s[j1:i2] + '\n'.join(free) + s[j2:]
m = '#include "ggml-cuda.h"'
if 'fg195v_enabled' not in s:
    s = s.replace(m, m + '\nextern "C" int fg195v_enabled(void);\nextern "C" void fg195v_plan_alloc(int64_t,int64_t,int64_t,int64_t,uint64_t);\nextern "C" int64_t fg195v_plan_free(int64_t,int64_t,int64_t);', 1)
c.write_text(s)
e = Path(os.environ['E195_DIR']) / 'source'
(flint / 'fg195v-bridge.c').write_text((e / 'bridge.c').read_text().replace('#include "device-generated.c"', '#include "fg195v-generated.c"'))
(flint / 'fg195v-generated.c').write_text((e / 'device-generated.c').read_text())
cm = flint / 'CMakeLists.txt'; ct = cm.read_text()
if 'fg195v-bridge.c' not in ct:
    ct = ct.replace('    list(APPEND GGML_SOURCES_CUDA "${CMAKE_CURRENT_SOURCE_DIR}/fg196p-bridge.c")',
                    '    list(APPEND GGML_SOURCES_CUDA "${CMAKE_CURRENT_SOURCE_DIR}/fg196p-bridge.c")\n    list(APPEND GGML_SOURCES_CUDA "${CMAKE_CURRENT_SOURCE_DIR}/fg195v-bridge.c")')
    cm.write_text(ct)
print('fg195v prepared')
PY3

cmake -S "$WORK" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=ON \
      -DCMAKE_CUDA_COMPILER=/usr/bin/nvcc -DLLAMA_BUILD_TESTS=OFF \
      -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF >/dev/null
ninja -C "$BUILD" -j 16 ggml-cuda
echo "built: $BUILD/bin/libggml-cuda.so.0.27.0"
