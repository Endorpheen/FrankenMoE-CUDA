#!/usr/bin/env python3
"""Build the reconstructed DSO: staged integration files + flint objects + the Goose modules they call."""
from pathlib import Path
import subprocess, sys, re, os

BASE = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
SRC = BASE/'runtime/reconstructed/src'
W = BASE/'runtime/reconstructed/build'; W.mkdir(parents=True, exist_ok=True)
RUN = BASE/'runtime/exp244/run'
OUT = W/'libllama-exp371.so.0.7.0'

FLAGS = ['-DGGML_BACKEND_SHARED','-DGGML_SHARED','-DGGML_USE_CPU','-DGGML_USE_CUDA','-DLLAMA_BUILD',
         '-DLLAMA_SHARED','-Dllama_EXPORTS', f'-I{SRC}/.', f'-I{FL}/build-cuda/src', f'-I{FL}/include',
         f'-I{FL}/ggml/include', '-O3','-DNDEBUG','-fPIC','-w']
CXX = ['/usr/bin/g++-13'] + FLAGS

changed = ['llama-batch.cpp','llama-context.cpp','llama-graph.cpp','llama-hparams.cpp','llama-kv-cache.cpp',
           'llama-memory-recurrent.cpp','llama-model-loader.cpp','llama-sampler.cpp','llama-vocab.cpp','unicode.cpp']
objs = []
for name in changed:
    o = W/(name + '.o')
    q = subprocess.run(CXX + ['-o', str(o), '-c', str(SRC/name)], capture_output=True, text=True)
    if q.returncode: print('FAIL', name); print(q.stderr[:1500]); sys.exit(1)
    objs.append(str(o))
    print('built', name)

# which Goose modules are called
def undef(o):
    s = subprocess.run(['nm','-u',o],capture_output=True,text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}
pref = set()
for o in objs:
    for sym in undef(o):
        m = re.match(r'^(fg\d{3}[a-z])', sym)
        if m: pref.add(m.group(1))
print('modules needed:', ' '.join(sorted(pref)))

# locate host-unit.c per prefix and build it
modules = []
for p in sorted(pref):
    found = None
    # any translation unit in a module's source that defines the prefix; generated files are
    # #included by their bridge, so they are skipped here
    for hu in sorted(BASE.glob('experiments/*/source/*.c')) + sorted(BASE.glob('experiments/*/source/*.cpp')):
        if 'generated' in hu.name: continue
        if p.encode() in hu.read_bytes():
            found = hu; break
    if not found:
        print('module not found:', p); continue
    o = W/(p + '.o')
    cc = 'gcc-13' if found.suffix == '.c' else 'g++-13'
    extra = [] if found.suffix == '.c' else ['-std=c++17']
    q = subprocess.run([cc,'-O2','-fPIC',*extra,f'-I{found.parent}','-c',str(found),'-o',str(o)],
                       capture_output=True, text=True)
    if q.returncode: print('module build failed', p); print(q.stderr[:800]); continue
    modules.append(str(o))
print('modules built:', len(modules))

flint_objs = sorted(str(x) for x in (FL/'build-cuda/src/CMakeFiles/llama.dir').rglob('*.o')
                    if x.name not in [c + '.o' for c in changed])
cmd = ['/usr/bin/c++','-fPIC','-O3','-DNDEBUG','-shared','-Wl,-soname,libllama.so.0','-o',str(OUT)]
cmd += objs + flint_objs + modules
cmd += [f'-Wl,-rpath,{RUN}:', str(RUN/'libggml.so.0'), str(RUN/'libggml-cpu.so.0'),
        str(RUN/'libggml-cuda.so.0'), str(RUN/'libggml-base.so.0')]
q = subprocess.run(cmd, capture_output=True, text=True)
if q.returncode: print(q.stderr[:3000]); sys.exit(1)
print('linked', OUT, OUT.stat().st_size)
