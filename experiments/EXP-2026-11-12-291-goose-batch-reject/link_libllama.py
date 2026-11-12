#!/usr/bin/env python3
"""EXP290: relink libllama with the refusal-returning decoder and the exception at the call site."""
from pathlib import Path
import subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
E = F / 'experiments/EXP-2026-11-12-291-goose-batch-reject'
W = F / 'runtime/reconstructed/build'
RUN = F / 'runtime/exp244/run'

changed = ['llama-batch.cpp', 'llama-context.cpp', 'llama-graph.cpp', 'llama-hparams.cpp', 'llama-kv-cache.cpp',
           'llama-model.cpp', 'llama-memory-recurrent.cpp', 'llama-memory-hybrid.cpp', 'llama-model-loader.cpp',
           'llama-sampler.cpp', 'llama-vocab.cpp', 'unicode.cpp']
objs = []
for c in changed:
    objs.append(str(E / 'batch-goose.o') if c == 'llama-batch.cpp' else str(W / (c + '.o')))

SKIP = {'fg345w','fg367s','fg373s','fg375s','fg376s','fg377s','fg380s','fg381s','fg382s','fg383s',
        'fg384s','fg385s','fg386s','fg387s','fg388s','fg389s','fg390s','fg391s','fg392s','fg393s',
        'fg394s','fg395s','fg396s','fg397s','fg398s','fg401s'}
modules = []
for x in sorted(W.glob('fg*.o')):
    if x.stem in SKIP:
        continue
    modules.append(str(E / 'bridge.o') if x.stem == 'fg291b' else str(x))
bridges = sorted(str(x) for x in W.glob('b*.o'))
flint_objs = sorted(str(x) for x in (FL / 'build-cuda/src/CMakeFiles/llama.dir').rglob('*.o')
                    if x.name not in [c + '.o' for c in changed])

OUT = E / 'libllama-exp291.so.0.7.0'
cmd = ['/usr/bin/c++', '-fPIC', '-O3', '-DNDEBUG', '-shared', '-Wl,-soname,libllama.so.0', '-o', str(OUT)]
cmd += objs + flint_objs + modules + bridges + [str(E / 'bridge.o')]
cmd += [f'-Wl,-rpath,{RUN}:', str(RUN / 'libggml.so.0'), str(RUN / 'libggml-cpu.so.0'),
        str(RUN / 'libggml-cuda.so.0'), str(RUN / 'libggml-base.so.0')]
q = subprocess.run(cmd, capture_output=True, text=True)
(E / 'link.stderr').write_text(q.stderr)
print('link rc =', q.returncode)
if q.returncode:
    print(q.stderr[-3000:]); sys.exit(1)
print('linked', OUT, OUT.stat().st_size)
