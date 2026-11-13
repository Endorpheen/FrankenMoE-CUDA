#!/usr/bin/env python3
"""Stage from the line the accepted runtime is built from: llama.cpp-flint-cuda objects.

No Goose call sites yet: this proves the line and the ABI, which the previous stage runs got wrong.
"""
from pathlib import Path
import subprocess, sys

E = Path(__file__).resolve().parent
B = E/'build'; B.mkdir(exist_ok=True)
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
FLD = FL/'build-cuda/src/CMakeFiles/llama.dir'
RUN = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp244/run')
out = B/'libllama-exp358.so.0.7.0'

objects = sorted(str(p) for p in FLD.rglob('*.o'))
print('objects:', len(objects))
cmd = ['/usr/bin/c++','-fPIC','-O3','-DNDEBUG','-shared','-Wl,-soname,libllama.so.0',
       '-o',str(out)] + objects + [f'-Wl,-rpath,{RUN}:',
       str(RUN/'libggml.so.0'), str(RUN/'libggml-cpu.so.0'),
       str(RUN/'libggml-cuda.so.0'), str(RUN/'libggml-base.so.0')]
q = subprocess.run(cmd, capture_output=True, text=True)
if q.returncode: print(q.stderr[:3000]); sys.exit(1)
print('linked', out, out.stat().st_size)

# the ABI the consumer needs must be present
def undef(p):
    s = subprocess.run(['nm','-D','-u',str(p)], capture_output=True, text=True).stdout
    return sorted({l.split()[-1] for l in s.splitlines() if l.strip()})
def defined(p):
    s = subprocess.run(['nm','-D','--defined-only',str(p)], capture_output=True, text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}
missing = [s for s in undef(RUN/'libllama-common.so.0') if s.startswith('llama_') and s not in defined(out)]
assert not missing, ('ABI gap', missing[:5])
print('ABI: every llama_ symbol the server needs is exported')
print('EXP358_STAGE_LINKED')
