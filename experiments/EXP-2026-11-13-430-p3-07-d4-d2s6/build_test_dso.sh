#!/bin/bash
# Build a compact test DSO for the MMQ Q8_1 layouts (native oracle + Goose bridge).
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-430-p3-07-d4-d2s6
D=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda
B=$F/runtime/exp430/build
OUT=$F/runtime/exp430/run
mkdir -p "$B" "$OUT"

cp "$D/ggml/src/ggml-cuda/CMakeFiles/ggml-cuda.dir/quantize.cu.o" "$B/native-quantize.o"
python3 - "$B/native-quantize.o" "$B/native-mmq-q8-1.o" <<'PY'
import re, subprocess, sys
src, dst = sys.argv[1], sys.argv[2]
nm = subprocess.run(['nm','--defined-only',src], capture_output=True, text=True, check=True).stdout
want = {'quantize_mmq_q8_1_cuda':'fg180q_native_quantize_mmq_q8_1_cuda',
        'quantize_scatter_mmq_q8_1_cuda':'fg180q_native_quantize_scatter_mmq_q8_1_cuda'}
found = {}
for line in nm.splitlines():
    ps = line.split()
    if len(ps) < 3: continue
    sym = ps[-1]
    if sym.endswith('.cold'): continue
    m = re.match(r'_Z(\d+)', sym)
    if not m: continue
    base = sym[m.end():m.end()+int(m[1])]
    if base in want: found[base] = sym
assert set(found) == set(want), found
args = ['objcopy']
for base, new in want.items():
    old = found[base]; m = re.match(r'_Z(\d+)', old)
    args += ['--redefine-sym', old+'=_Z'+str(len(new))+new+old[m.end()+int(m[1]):]]
subprocess.run(args+[src,dst], check=True)
print('renamed', sorted(found))
PY

g++-13 -shared -fPIC -Wl,-soname,libmmq430.so -o "$OUT/libmmq430.so" \
  "$B/native-mmq-q8-1.o" "$B/cuda-mmq-q8-1.o" "$B/host.o" \
  -L"$D/bin" -Wl,-rpath,"$D/bin" -lggml-cuda -lggml-base -lggml -lggml-cpu \
  -L/opt/cuda/targets/x86_64-linux/lib -lcudart
echo "BUILT $OUT/libmmq430.so"
