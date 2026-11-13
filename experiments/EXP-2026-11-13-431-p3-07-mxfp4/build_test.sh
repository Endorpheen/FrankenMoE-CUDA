#!/bin/bash
# Build the MXFP4 test DSO and fixture.
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-431-p3-07-mxfp4
D=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda
S=$E/source
B=$F/runtime/exp431/build
R=$F/runtime/exp431/run
mkdir -p "$B" "$R"

python3 "$E/emit_sources.py"

python3 - "$F" "$D" "$S" "$B" <<'PY'
import json, shlex, subprocess, sys
from pathlib import Path
F, D, S, B = map(Path, sys.argv[1:5])
db = json.loads((D/'compile_commands.json').read_text())
r = next(x for x in db if x['file'].endswith('/quantize.cu'))
cmd = shlex.split(r['command']); cmd.insert(1, '-I' + str(Path(r['file']).parent)); cmd.insert(1, '-I' + str(S))
cmd[cmd.index('-c')+1] = str(S/'mxfp4-runtime.cu'); cmd[cmd.index('-o')+1] = str(B/'mxfp4-runtime.o')
q = subprocess.run(cmd, cwd=r['directory'], capture_output=True, text=True, timeout=1200)
if q.returncode: print(q.stderr[-4000:]); raise SystemExit(q.returncode)
PY

gcc-13 -std=gnu11 -O3 -DNDEBUG -fPIC -fvisibility=hidden \
  -Werror=implicit-int -Werror=implicit-function-declaration \
  -I"$S" -c "$S/mxfp4-host-unit.c" -o "$B/mxfp4-host.o"

cp "$D/ggml/src/ggml-cuda/CMakeFiles/ggml-cuda.dir/quantize.cu.o" "$B/native-quantize.o"
python3 - "$B/native-quantize.o" "$B/native-mxfp4.o" <<'PY'
import re, subprocess, sys
src, dst = sys.argv[1], sys.argv[2]
nm = subprocess.run(['nm','--defined-only',src], capture_output=True, text=True, check=True).stdout
want = {'quantize_mmq_fp4_cuda':'fg431_native_quantize_mmq_fp4_cuda',
        'quantize_scatter_mmq_fp4_cuda':'fg431_native_quantize_scatter_mmq_fp4_cuda'}
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
import subprocess as sp
sp.run(args+[src,dst], check=True)
print('renamed', sorted(found))
PY

g++-13 -shared -fPIC -Wl,-soname,libmxfp431.so -o "$R/libmxfp431.so" \
  "$B/native-mxfp4.o" "$B/mxfp4-runtime.o" "$B/mxfp4-host.o" \
  -L"$D/bin" -Wl,-rpath,"$D/bin" -lggml-cuda -lggml-base -lggml -lggml-cpu \
  -L/opt/cuda/targets/x86_64-linux/lib -lcudart

python3 - "$F" "$D" "$E" "$B" <<'PY'
import json, shlex, subprocess, sys
from pathlib import Path
F, D, E, B = map(Path, sys.argv[1:5])
db = json.loads((D/'compile_commands.json').read_text())
r = next(x for x in db if x['file'].endswith('/quantize.cu'))
cmd = shlex.split(r['command']); cmd.insert(1, '-I' + str(Path(r['file']).parent)); cmd.insert(1, '-I' + str(E/'source'))
cmd[cmd.index('-c')+1] = str(E/'tests/gpu-fixture.cu'); cmd[cmd.index('-o')+1] = str(B/'gpu-fixture.o')
q = subprocess.run(cmd, cwd=r['directory'], capture_output=True, text=True, timeout=900)
if q.returncode: print(q.stderr[-4000:]); raise SystemExit(q.returncode)
R = F/'runtime/exp431/run'
q = subprocess.run(['g++-13', str(B/'gpu-fixture.o'), '-L'+str(R), '-Wl,-rpath,'+str(R),
    '-lmxfp431', '-L'+str(D/'bin'), '-Wl,-rpath,'+str(D/'bin'),
    '-lggml-cuda','-lggml-base','-lggml','-lggml-cpu',
    '-L/opt/cuda/targets/x86_64-linux/lib','-lcudart','-o',str(B/'gpu-fixture')],
    capture_output=True, text=True)
if q.returncode: print(q.stderr[-4000:]); raise SystemExit(q.returncode)
print('BUILT')
PY
echo READY
