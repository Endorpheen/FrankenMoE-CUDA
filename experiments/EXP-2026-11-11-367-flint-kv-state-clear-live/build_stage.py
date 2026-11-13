#!/usr/bin/env python3
"""Stage: batch both + KV state_clear call site."""
from pathlib import Path
import subprocess, sys
E=Path(__file__).resolve().parent; B=E/'build'
FL=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda'); FLD=FL/'build-cuda/src/CMakeFiles/llama.dir'
RUN=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp244/run')
BASE=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments')
UB=BASE/'EXP-2026-11-11-356-goose-ubatch-add-live'; SE=BASE/'EXP-2026-11-11-360-goose-split-select-live'
BATCH=BASE/'EXP-2026-11-11-362-flint-batch-both-live/build/llama-batch-both.o'
out=B/'libllama-exp367.so.0.7.0'
for src,o in ((UB/'source/bridge.c',B/'b349.o'),(SE/'source/bridge.c',B/'b350.o'),(E/'source/bridge.c',B/'b367.o')):
    q=subprocess.run(['gcc','-O3','-DNDEBUG','-fPIC','-std=c11','-c',str(src),'-o',str(o),'-I',str(src.parent)],
                     capture_output=True,text=True)
    if q.returncode: print(q.stderr[:2000]); sys.exit(1)
objects=sorted(str(p) for p in FLD.rglob('*.o') if p.name not in ('llama-batch.cpp.o','llama-kv-cache.cpp.o'))
objects+=[str(BATCH),str(E/'build/llama-kv-clear.o'),str(B/'b349.o'),str(B/'b350.o'),str(B/'b367.o')]
cmd=['/usr/bin/c++','-fPIC','-O3','-DNDEBUG','-shared','-Wl,-soname,libllama.so.0','-o',str(out)]+objects+[
 f'-Wl,-rpath,{RUN}:',str(RUN/'libggml.so.0'),str(RUN/'libggml-cpu.so.0'),
 str(RUN/'libggml-cuda.so.0'),str(RUN/'libggml-base.so.0')]
q=subprocess.run(cmd,capture_output=True,text=True)
if q.returncode: print(q.stderr[:3000]); sys.exit(1)
print('linked',out,out.stat().st_size)
def undef(p):
    s=subprocess.run(['nm','-D','-u',str(p)],capture_output=True,text=True).stdout
    return sorted({l.split()[-1] for l in s.splitlines() if l.strip()})
def defined(p):
    s=subprocess.run(['nm','-D','--defined-only',str(p)],capture_output=True,text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}
miss=[s for s in undef(RUN/'libllama-common.so.0') if s.startswith('llama_') and s not in defined(out)]
assert not miss, miss[:5]
print('ABI ok; EXP367_STAGE_LINKED')
