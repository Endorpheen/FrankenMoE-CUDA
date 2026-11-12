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

changed = ['llama-batch.cpp','llama-context.cpp','llama-graph.cpp','llama-hparams.cpp','llama-kv-cache.cpp','llama-model.cpp',
           'llama-memory-recurrent.cpp','llama-memory-hybrid.cpp','llama-model-loader.cpp','llama-sampler.cpp',
           'llama-vocab.cpp','llama-moe-cache.cpp','unicode.cpp']
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
    if p in ('fg345w', 'fg367s', 'fg373s', 'fg375s', 'fg376s', 'fg377s', 'fg380s', 'fg381s', 'fg382s', 'fg383s', 'fg384s', 'fg385s', 'fg386s', 'fg387s', 'fg388s', 'fg389s', 'fg390s', 'fg391s', 'fg392s', 'fg393s', 'fg394s', 'fg395s', 'fg396s', 'fg397s', 'fg398s', 'fg401s'):
        continue  # our own bridges are linked explicitly below
    found = None
    # any translation unit in a module's source that defines the prefix; generated files are
    # #included by their bridge, so they are skipped here
    # prefer the LATEST experiment that defines the prefix: bridges were revised across experiments
    import re as _re
    cands = []
    for hu in list(BASE.glob('experiments/*/source/*.c')) + list(BASE.glob('experiments/*/source/*.cpp')):
        if 'generated' in hu.name: continue
        # a module DEFINES the prefix when it carries its enabled() entry point; merely calling
        # another module's function must not make this file its owner (EXP392 called fg199k_can_use)
        if (p + '_enabled').encode() in hu.read_bytes():
            m = _re.search(r'EXP-\d{4}-\d{2}-\d{2}-(\d+)', str(hu))
            if m: cands.append((int(m.group(1)), hu))
    if cands:
        found = max(cands)[1]
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

BR = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments')
bridge_objs = []
for mod, out_name in (('EXP-2026-11-11-347-goose-kv-write-bridge', 'b345.o'),
                      ('EXP-2026-11-11-367-flint-kv-state-clear-live', 'b367.o'),
                      ('EXP-2026-11-11-373-flint-read-sinfo-live', 'b373.o'),
                      ('EXP-2026-11-11-375-flint-hybrid-memory-live', 'b375.o'),
                      ('EXP-2026-11-11-376-flint-hybrid-context-live', 'b376.o'),
                      ('EXP-2026-11-11-377-flint-file-alignment-live', 'b377.o'),
                      ('EXP-2026-11-11-380-flint-penalties-accept-live', 'b380.o'),
                      ('EXP-2026-11-11-381-flint-kv-whole-clear-live', 'b381.o'),
                      ('EXP-2026-11-11-382-flint-recurrent-io-plan-live', 'b382.o'),
                      ('EXP-2026-11-11-383-flint-batch-container-plan-live', 'b383.o'),
                      ('EXP-2026-11-11-384-goose-sampler-softmax-live', 'b384.o'),
                      ('EXP-2026-11-11-385-goose-sampler-guards-live', 'b385.o'),
                      ('EXP-2026-11-11-386-goose-sampler-dist-pick-live', 'b386.o'),
                      ('EXP-2026-11-11-387-goose-sampler-chain-live', 'b387.o'),
                      ('EXP-2026-11-11-388-goose-sampler-filters-guard-live', 'b388.o'),
                      ('EXP-2026-11-11-389-goose-gguf-inventory-live', 'b389.o'),
                      ('EXP-2026-11-11-390-goose-vocab-partition-live', 'b390.o'),
                      ('EXP-2026-11-11-391-goose-fused-ops-resolve-live', 'b391.o'),
                      ('EXP-2026-11-11-392-goose-kv-find-slot-walk', 'b392.o'),
                      ('EXP-2026-11-11-393-goose-kv-meta-plan', 'b393.o'),
                      ('EXP-2026-11-11-394-goose-kv-layout-span', 'b394.o'),
                      ('EXP-2026-11-11-395-goose-kv-read-meta', 'b395.o'),
                      ('EXP-2026-11-11-396-goose-recurrent-payload-span', 'b396.o'),
                      ('EXP-2026-11-11-397-goose-hybrid-delegation', 'b397.o'),
                      ('EXP-2026-11-11-398-goose-batch-container-layout', 'b398.o'),
                      ('EXP-2026-11-11-401-goose-detok-decisions', 'b401.o')):
    o = W/out_name
    q = subprocess.run(['gcc','-O3','-DNDEBUG','-fPIC','-std=c11','-c', str(BR/mod/'source/bridge.c'),
                        '-o', str(o), '-I', str(BR/mod/'source')], capture_output=True, text=True)
    if q.returncode: print('bridge failed', mod); print(q.stderr[:800]); sys.exit(1)
    bridge_objs.append(str(o))

extra_mod = [W/'b291.o', W/'fx286pic.o', W/'fx262a.o', W/'fx263-pic.o', W/'gates.o']
bridge_objs += [str(x) for x in extra_mod if x.exists()]
flint_objs = sorted(str(x) for x in (FL/'build-cuda/src/CMakeFiles/llama.dir').rglob('*.o')
                    if x.name not in [c + '.o' for c in changed])
cmd = ['/usr/bin/c++','-fPIC','-O3','-DNDEBUG','-shared','-Wl,-soname,libllama.so.0','-o',str(OUT)]
cmd += objs + flint_objs + modules + bridge_objs
cmd += [f'-Wl,-rpath,{RUN}:', str(RUN/'libggml.so.0'), str(RUN/'libggml-cpu.so.0'),
        str(RUN/'libggml-cuda.so.0'), str(RUN/'libggml-base.so.0')]
q = subprocess.run(cmd, capture_output=True, text=True)
if q.returncode: print(q.stderr[:3000]); sys.exit(1)
print('linked', OUT, OUT.stat().st_size)
