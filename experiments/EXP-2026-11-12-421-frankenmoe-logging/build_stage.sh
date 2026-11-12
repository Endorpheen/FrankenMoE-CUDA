#!/bin/bash
# Build the staged FrankenMoE logging library and fixture.
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-12-421-frankenmoe-logging
B=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda
OUT=$F/runtime/exp421/build
mkdir -p "$OUT/bin"

python3 "$E/emit_sources.py"
gcc-13 -O2 -std=c11 -fPIC -c "$E/source/logfmt-unit.c" -o "$OUT/logfmt.o"
gcc-13 -O2 -std=c11 -fPIC -c "$E/integration/logfmt_bridge.c" -o "$OUT/logfmt_bridge.o"
g++-13 -DCPPHTTPLIB_OPENSSL_SUPPORT -DGGML_BACKEND_SHARED -DGGML_SHARED \
  -DGGML_USE_CPU -DGGML_USE_CUDA -DLLAMA_SHARED -DLLAMA_SUBPROCESS -Dllama_common_EXPORTS \
  -I"$E/integration" -I"$B/../common/." -I"$B/../vendor/nlohmann/.." \
  -I"$B/../vendor/sheredom/.." -I"$B/../src/../include" -I"$B/../ggml/src/../include" \
  -O3 -DNDEBUG -fPIC -Wmissing-declarations -Wmissing-noreturn -Wall -Wextra -Wpedantic \
  -Wcast-qual -Wno-unused-function -Wno-array-bounds -Wextra-semi \
  -c "$E/integration/log.cpp" -o "$OUT/log.o"

python3 - "$F" "$B" "$OUT" <<'PY'
import subprocess, shlex, sys
from pathlib import Path
F, B, OUT = map(Path, sys.argv[1:4])
q = subprocess.run(['ninja', '-t', 'commands', 'bin/libllama-common.so.0.7.0'],
                   cwd=B, capture_output=True, text=True, check=True)
argv = [a for a in shlex.split(q.stdout.strip().splitlines()[-1]) if a not in (':', '&&')]
out = OUT / 'bin' / 'libllama-common.so.0.7.0'
n = 0
for i, a in enumerate(argv):
    if a.endswith('common/CMakeFiles/llama-common.dir/log.cpp.o'):
        argv[i] = str(OUT / 'log.o'); n += 1
    if a == '-o':
        argv[i + 1] = str(out)
assert n == 1, n
at = argv.index('-o') + 2
for extra in [OUT / 'logfmt.o', OUT / 'logfmt_bridge.o']:
    argv.insert(at, str(extra)); at += 1
r = subprocess.run(argv, cwd=B, capture_output=True, text=True)
if r.returncode:
    print(r.stderr[-3000:]); raise SystemExit(r.returncode)
print('linked', out)
PY

rm -rf "$F/runtime/exp421/stage"
mkdir -p "$F/runtime/exp421/stage/bin"
for f in "$B"/bin/*; do ln -s "$(readlink -f "$f")" "$F/runtime/exp421/stage/bin/$(basename "$f")"; done
cp "$OUT/bin/libllama-common.so.0.7.0" "$F/runtime/exp421/stage/bin/libllama-common.so.0.7.0"
ln -sf libllama-common.so.0.7.0 "$F/runtime/exp421/stage/bin/libllama-common.so.0"
ln -sf libllama-common.so.0.7.0 "$F/runtime/exp421/stage/bin/libllama-common.so"

gcc-13 -O2 -std=c11 -I"$E/source" "$E/tests/logfmt_fixture.c" "$E/source/logfmt-unit.c" \
  -o "$OUT/logfmt_fixture"
echo STAGE_READY
