#!/usr/bin/env python3
"""Stage on the accepted line with the ubatch_add call site (EXP349 module + EXP356 bridge)."""
from pathlib import Path
import subprocess, sys

E = Path(__file__).resolve().parent
B = E/'build'; B.mkdir(exist_ok=True)
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
FLD = FL/'build-cuda/src/CMakeFiles/llama.dir'
RUN = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp244/run')
INT = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-11-356-goose-ubatch-add-live')
out = B/'libllama-exp359.so.0.7.0'

q = subprocess.run(['gcc','-O3','-DNDEBUG','-fPIC','-std=c11','-c',str(INT/'source/bridge.c'),
                    '-o',str(B/'bridge-349.o'),'-I',str(INT/'source')], capture_output=True, text=True)
if q.returncode: print(q.stderr[:2000]); sys.exit(1)

objects = sorted(str(p) for p in FLD.rglob('*.o') if p.name != 'llama-batch.cpp.o')
objects += [str(INT/'build/llama-batch-flint.o'), str(B/'bridge-349.o')]

cmd = ['/usr/bin/c++','-fPIC','-O3','-DNDEBUG','-shared','-Wl,-soname,libllama.so.0',
       '-o',str(out)] + objects + [f'-Wl,-rpath,{RUN}:',
       str(RUN/'libggml.so.0'), str(RUN/'libggml-cpu.so.0'),
       str(RUN/'libggml-cuda.so.0'), str(RUN/'libggml-base.so.0')]
q = subprocess.run(cmd, capture_output=True, text=True)
if q.returncode: print(q.stderr[:3000]); sys.exit(1)
print('linked', out, out.stat().st_size)

def undef(p):
    s = subprocess.run(['nm','-D','-u',str(p)], capture_output=True, text=True).stdout
    return sorted({l.split()[-1] for l in s.splitlines() if l.strip()})
def defined(p):
    s = subprocess.run(['nm','-D','--defined-only',str(p)], capture_output=True, text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}
missing = [s for s in undef(RUN/'libllama-common.so.0') if s.startswith('llama_') and s not in defined(out)]
assert not missing, ('ABI gap', missing[:5])
print('ABI ok; witness strings:', sum('FG349S' in l for l in
      [subprocess.run(['strings',str(out)],capture_output=True,text=True).stdout]))
print('EXP359_STAGE_LINKED')
