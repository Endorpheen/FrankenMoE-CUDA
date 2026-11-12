#!/bin/bash
# Re-emit and rebuild the compact test DSO from the current Goose source.
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-430-p3-07-d4-d2s6
D=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda
S=$E/source
B=$F/runtime/exp430/build
python3 "$E/emit_sources.py"
python3 - "$F" "$D" "$S" "$B" <<'PY'
import json, shlex, subprocess, sys
from pathlib import Path
F, D, S, B = map(Path, sys.argv[1:5])
db = json.loads((D/'compile_commands.json').read_text())
r = next(x for x in db if x['file'].endswith('/quantize.cu'))
cmd = shlex.split(r['command']); cmd.insert(1, '-I' + str(Path(r['file']).parent)); cmd.insert(1, '-I' + str(S))
cmd[cmd.index('-c')+1] = str(S/'cuda-runtime.cu'); cmd[cmd.index('-o')+1] = str(B/'cuda-mmq-q8-1.o')
q = subprocess.run(cmd, cwd=r['directory'], capture_output=True, text=True, timeout=900)
if q.returncode: print(q.stderr[-2000:]); raise SystemExit(q.returncode)
q = subprocess.run(['gcc-13','-std=gnu11','-O3','-DNDEBUG','-fPIC','-fvisibility=hidden',
    '-Werror=implicit-int','-Werror=implicit-function-declaration','-I'+str(S),
    '-c',str(S/'host-unit.c'),'-o',str(B/'host.o')], capture_output=True, text=True)
if q.returncode: print(q.stderr[-2000:]); raise SystemExit(q.returncode)
PY
bash "$E/build_test_dso.sh" >/dev/null
echo REBUILT
