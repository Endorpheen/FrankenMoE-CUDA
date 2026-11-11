#!/usr/bin/env python3
"""Stage with four Goose call sites (state-write ranges, split-simple, split-equal tail, ubatch_add)."""
from pathlib import Path
import subprocess, sys

E = Path(__file__).resolve().parent
B = E/'build'; B.mkdir(exist_ok=True)
WB = Path('/home/end0/AI/FrankenMoE-CUDA/build/expert-tier-franken-cuda')
WBD = WB/'src/CMakeFiles/llama.dir'
BASE = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments')
KV = BASE/'EXP-2026-11-11-347-goose-kv-write-bridge'
SS = BASE/'EXP-2026-11-11-354-goose-batch-split-simple-live'
TL = BASE/'EXP-2026-11-11-355-goose-batch-keep-tail-live'
out = B/'libllama-exp357.so.0.3.0'

bridges = [(KV/'source/bridge.c', B/'b345.o'), (SS/'source/bridge.c', B/'b351.o'),
           (TL/'source/bridge.c', B/'b348.o'), (E/'source/bridge.c', B/'b357.o')]
for src, o in bridges:
    q = subprocess.run(['gcc','-O3','-DNDEBUG','-fPIC','-std=c11','-c',str(src),'-o',str(o),'-I',str(src.parent)],
                       capture_output=True, text=True)
    if q.returncode: print(q.stderr[:2000]); sys.exit(1)

objects = sorted(str(p) for p in WBD.rglob('*.o')
                 if p.name not in ('llama-batch.cpp.o', 'llama-kv-cache.cpp.o'))
objects += [str(E/'build/llama-batch-integrated.o'), str(KV/'build/llama-kv-cache-integrated.o')]
objects += [str(o) for _, o in bridges]

cmd = ['/usr/bin/c++','-fPIC','-O3','-DNDEBUG','-shared','-Wl,-soname,libllama-exp357.so.0',
       '-o',str(out)] + objects + [f'-Wl,-rpath,{WB}/bin:',
       str(WB/'bin/libggml.so.0.22.0'), str(WB/'bin/libggml-cpu.so.0.22.0'),
       str(WB/'bin/libggml-cuda.so.0.22.0'), str(WB/'bin/libggml-base.so.0.22.0')]
q = subprocess.run(cmd, capture_output=True, text=True)
if q.returncode: print(q.stderr[:3000]); sys.exit(1)
print('linked', out, out.stat().st_size, 'bytes')
def dyn(p):
    s = subprocess.run(['nm','-D','--defined-only',str(p)], capture_output=True, text=True).stdout
    return set(l.split()[-1] for l in s.splitlines() if l.strip())
missing = dyn(WB/'bin/libllama.so.0.3.0') - dyn(out)
assert not missing, ('lost', sorted(missing)[:10])
print('exported symbols preserved:', len(dyn(WB/'bin/libllama.so.0.3.0')))
print('EXP357_STAGE_LINKED')
