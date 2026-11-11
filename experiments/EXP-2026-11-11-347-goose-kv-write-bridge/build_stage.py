#!/usr/bin/env python3
"""Link a stage DSO with the Goose state-write call site in place of the native one.

Read-only against the working build: every object is taken from
build/expert-tier-franken-cuda, one object is swapped for the integrated copy, and the bridge is
linked in. The output goes under this experiment's ignored build directory.
"""
from pathlib import Path
import subprocess, sys

E = Path(__file__).resolve().parent
B = E / 'build'; B.mkdir(exist_ok=True)
WB = Path('/home/end0/AI/FrankenMoE-CUDA/build/expert-tier-franken-cuda')
WBD = WB / 'src/CMakeFiles/llama.dir'

out = B / 'libllama-exp347.so.0.3.0'

# bridge, position independent, with the same optimisation
q = subprocess.run(['gcc', '-O3', '-DNDEBUG', '-fPIC', '-std=c11', '-c', str(E/'source/bridge.c'),
                    '-o', str(B/'bridge.o'), '-I', str(E/'source')], capture_output=True, text=True)
if q.returncode: print(q.stderr[:2000]); sys.exit(1)

objects = sorted(str(p) for p in WBD.rglob('*.o') if p.name != 'llama-kv-cache.cpp.o')
assert objects, 'no objects in the working build'
objects.append(str(B/'llama-kv-cache-integrated.o'))
objects.append(str(B/'bridge.o'))

cmd = ['/usr/bin/c++', '-fPIC', '-O3', '-DNDEBUG', '-shared',
       '-Wl,-soname,libllama-exp347.so.0',
       '-o', str(out)] + objects + [
       f'-Wl,-rpath,{WB}/bin:',
       str(WB/'bin/libggml.so.0.22.0'), str(WB/'bin/libggml-cpu.so.0.22.0'),
       str(WB/'bin/libggml-cuda.so.0.22.0'), str(WB/'bin/libggml-base.so.0.22.0')]
q = subprocess.run(cmd, capture_output=True, text=True)
if q.returncode: print(q.stderr[:3000]); sys.exit(1)
print('linked', out, out.stat().st_size, 'bytes')

# the swap must be visible in the dynamic symbol table, and nothing may be lost
def dyn(p):
    s = subprocess.run(['nm', '-D', '--defined-only', str(p)], capture_output=True, text=True).stdout
    return set(l.split()[-1] for l in s.splitlines() if l.strip())
if (WB/'bin/libllama.so.0.3.0').exists():
    ref, new = dyn(WB/'bin/libllama.so.0.3.0'), dyn(out)
    missing = ref - new
    assert not missing, ('exported symbols lost', sorted(missing)[:10])
    print('exported symbols preserved:', len(ref))
print('EXP347_STAGE_LINKED')
