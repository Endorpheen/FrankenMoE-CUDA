#!/usr/bin/env python3
"""Stage on the accepted line with the split_equal selection call site."""
from pathlib import Path
import subprocess, sys
E = Path(__file__).resolve().parent; B = E/'build'; B.mkdir(exist_ok=True)
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda'); FLD = FL/'build-cuda/src/CMakeFiles/llama.dir'
RUN = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp244/run')
INT = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-11-360-goose-split-select-live')
out = B/'libllama-exp361.so.0.7.0'
q = subprocess.run(['gcc','-O3','-DNDEBUG','-fPIC','-std=c11','-c',str(INT/'source/bridge.c'),
                    '-o',str(B/'bridge-350.o'),'-I',str(INT/'source')], capture_output=True, text=True)
if q.returncode: print(q.stderr[:2000]); sys.exit(1)
objects = sorted(str(p) for p in FLD.rglob('*.o') if p.name != 'llama-batch.cpp.o')
objects += [str(E/'build/llama-batch-sel.o'), str(B/'bridge-350.o')]
cmd = ['/usr/bin/c++','-fPIC','-O3','-DNDEBUG','-shared','-Wl,-soname,libllama.so.0','-o',str(out)] + objects + [
    f'-Wl,-rpath,{RUN}:', str(RUN/'libggml.so.0'), str(RUN/'libggml-cpu.so.0'),
    str(RUN/'libggml-cuda.so.0'), str(RUN/'libggml-base.so.0')]
q = subprocess.run(cmd, capture_output=True, text=True)
if q.returncode: print(q.stderr[:3000]); sys.exit(1)
print('linked', out, out.stat().st_size)
def undef(p):
    s=subprocess.run(['nm','-D','-u',str(p)],capture_output=True,text=True).stdout
    return sorted({l.split()[-1] for l in s.splitlines() if l.strip()})
def defined(p):
    s=subprocess.run(['nm','-D','--defined-only',str(p)],capture_output=True,text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}
miss=[s for s in undef(RUN/'libllama-common.so.0') if s.startswith('llama_') and s not in defined(out)]
assert not miss, miss[:5]
print('ABI ok; EXP361_STAGE_LINKED')
