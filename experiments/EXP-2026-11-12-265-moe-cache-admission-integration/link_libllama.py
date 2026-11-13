#!/usr/bin/env python3
"""EXP265: relink libllama with the EXP-264 integrated cache object, then A/B the copy switch on one DSO.

Same link recipe as runtime/reconstructed/build.py, with llama-moe-cache.cpp taken from this experiment's integration
copy and its bridge appended. The cache variant's content is known to differ from the accepted profile, so the gate is
an A/B on the same DSO: GGML_GOOSE_MOE_CACHE_COPY=1 vs 0 must produce identical content, tokens and probabilities, and
the witness must appear only in the Goose arm.
"""
from pathlib import Path
import glob, json, os, shutil, signal, socket, subprocess, sys, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
W = F / 'runtime/reconstructed/build'
RUN = F / 'runtime/exp244/run'
E = F / 'experiments/EXP-2026-11-12-265-moe-cache-admission-integration'
B = E / 'build'
B.mkdir(exist_ok=True)

changed = ['llama-batch.cpp', 'llama-context.cpp', 'llama-graph.cpp', 'llama-hparams.cpp', 'llama-kv-cache.cpp',
           'llama-model.cpp', 'llama-memory-recurrent.cpp', 'llama-memory-hybrid.cpp', 'llama-model-loader.cpp',
           'llama-sampler.cpp', 'llama-vocab.cpp', 'unicode.cpp']
objs = [str(W / (c + '.o')) for c in changed]
objs.append(str(E / 'moe-cache-goose.o'))
bridge = [str(B / 'bridge.o'), str(B / 'admission-bridge.o')]

flint_objs = sorted(str(x) for x in (FL / 'build-cuda/src/CMakeFiles/llama.dir').rglob('*.o')
                    if x.name not in [c + '.o' for c in changed] and x.name != 'llama-moe-cache.cpp.o')
SKIP = {'fg345w','fg367s','fg373s','fg375s','fg376s','fg377s','fg380s','fg381s','fg382s','fg383s',
        'fg384s','fg385s','fg386s','fg387s','fg388s','fg389s','fg390s','fg391s','fg392s','fg393s',
        'fg394s','fg395s','fg396s','fg397s','fg398s','fg401s'}
modules = sorted(str(x) for x in W.glob('fg*.o') if x.stem not in SKIP)
bridges = sorted(str(x) for x in W.glob('b*.o'))

OUT = E / 'libllama-exp265.so.0.7.0'
cmd = ['/usr/bin/c++', '-fPIC', '-O3', '-DNDEBUG', '-shared', '-Wl,-soname,libllama.so.0', '-o', str(OUT)]
cmd += objs + flint_objs + modules + bridges + bridge
cmd += [f'-Wl,-rpath,{RUN}:', str(RUN / 'libggml.so.0'), str(RUN / 'libggml-cpu.so.0'),
        str(RUN / 'libggml-cuda.so.0'), str(RUN / 'libggml-base.so.0')]
q = subprocess.run(cmd, capture_output=True, text=True)
(E / 'link.stderr').write_text(q.stderr)
print('link rc =', q.returncode)
if q.returncode:
    print(q.stderr[-3000:]); sys.exit(1)
print('linked', OUT, OUT.stat().st_size)
