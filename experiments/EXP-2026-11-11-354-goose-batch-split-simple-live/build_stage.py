#!/usr/bin/env python3
"""Link the stage DSO with two live call sites: batch split-simple and KV state-write ranges."""
from pathlib import Path
import subprocess, sys

E = Path(__file__).resolve().parent
B = E / 'build'; B.mkdir(exist_ok=True)
WB = Path('/home/end0/AI/FrankenMoE-CUDA/build/expert-tier-franken-cuda')
WBD = WB / 'src/CMakeFiles/llama.dir'
KV = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-11-347-goose-kv-write-bridge')

out = B / 'libllama-exp354.so.0.3.0'

for src, o in ((E/'source/bridge.c', B/'bridge-351.o'), (KV/'source/bridge.c', B/'bridge-345.o')):
    q = subprocess.run(['gcc', '-O3', '-DNDEBUG', '-fPIC', '-std=c11', '-c', str(src), '-o', str(o),
                        '-I', str(src.parent)], capture_output=True, text=True)
    if q.returncode: print(q.stderr[:2000]); sys.exit(1)

objects = sorted(str(p) for p in WBD.rglob('*.o')
                 if p.name not in ('llama-batch.cpp.o', 'llama-kv-cache.cpp.o'))
objects += [str(B/'llama-batch-integrated.o'), str(KV/'build/llama-kv-cache-integrated.o'),
            str(B/'bridge-351.o'), str(B/'bridge-345.o')]

cmd = ['/usr/bin/c++', '-fPIC', '-O3', '-DNDEBUG', '-shared', '-Wl,-soname,libllama-exp354.so.0',
       '-o', str(out)] + objects + [
       f'-Wl,-rpath,{WB}/bin:',
       str(WB/'bin/libggml.so.0.22.0'), str(WB/'bin/libggml-cpu.so.0.22.0'),
       str(WB/'bin/libggml-cuda.so.0.22.0'), str(WB/'bin/libggml-base.so.0.22.0')]
q = subprocess.run(cmd, capture_output=True, text=True)
if q.returncode: print(q.stderr[:3000]); sys.exit(1)
print('linked', out, out.stat().st_size, 'bytes')

def dyn(p):
    s = subprocess.run(['nm', '-D', '--defined-only', str(p)], capture_output=True, text=True).stdout
    return set(l.split()[-1] for l in s.splitlines() if l.strip())
ref, new = dyn(WB/'bin/libllama.so.0.3.0'), dyn(out)
missing = ref - new
assert not missing, ('exported symbols lost', sorted(missing)[:10])
print('exported symbols preserved:', len(ref))
print('EXP354_STAGE_LINKED')
