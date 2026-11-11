#!/usr/bin/env python3
"""Integration check for the state-write call site.

Compiles the integrated llama-kv-cache.cpp with the exact flags the working build uses, then
proves two things without a model run: the object's defined-symbol set is unchanged, and its
only new undefined dependencies are the two bridge entry points. Both are required before the
stage link is worth attempting.
"""
from pathlib import Path
import subprocess, sys

E = Path(__file__).resolve().parent
B = E / 'build'; B.mkdir(exist_ok=True)
WORK = Path('/home/end0/AI/FrankenMoE-CUDA/work/llama.cpp-integration')
REF = Path('/home/end0/AI/FrankenMoE-CUDA/build/expert-tier-franken-cuda/src/CMakeFiles/llama.dir/llama-kv-cache.cpp.o')

cmd = ['/usr/bin/c++',
       '-DGGML_BACKEND_SHARED', '-DGGML_SHARED', '-DGGML_USE_CPU', '-DGGML_USE_CUDA',
       '-DLLAMA_BUILD', '-DLLAMA_COMMIT="4aaad5d"', '-DLLAMA_SHARED', '-DLLAMA_VERSION="0.3.0-dev"',
       '-Dllama_EXPORTS',
       f'-I{WORK}/src/.', f'-I{WORK}/src/../include', f'-I{WORK}/ggml/src/../include',
       '-O3', '-DNDEBUG', '-fPIC', '-Wmissing-declarations', '-Wmissing-noreturn', '-Wall', '-Wextra',
       '-Wpedantic', '-Wcast-qual', '-Wno-unused-function', '-Wno-array-bounds', '-Wextra-semi',
       '-o', str(B/'llama-kv-cache-integrated.o'), '-c', str(E/'integration/llama-kv-cache.cpp')]
q = subprocess.run(cmd, capture_output=True, text=True)
if q.returncode or (q.stderr.strip() and 'warning' in q.stderr):
    print(q.stderr[:2000]); sys.exit(1)

def defined(p):
    out = subprocess.run(['nm', '--defined-only', str(p)], capture_output=True, text=True).stdout
    return sorted(line.split()[-1] for line in out.splitlines() if line.strip())

def undefined(p):
    out = subprocess.run(['nm', '-u', str(p)], capture_output=True, text=True).stdout
    return sorted(line.split()[-1] for line in out.splitlines() if line.strip())

new, old = defined(B/'llama-kv-cache-integrated.o'), defined(REF)
assert new == old, ('defined symbols changed', set(new) ^ set(old))
assert 'fg345w_enabled' in undefined(B/'llama-kv-cache-integrated.o')
assert 'fg345w_select' in undefined(B/'llama-kv-cache-integrated.o')
assert not [s for s in undefined(B/'llama-kv-cache-integrated.o') if s.startswith('fg345w_')
            and s not in ('fg345w_enabled', 'fg345w_select')]
print(f'defined symbols identical: {len(old)}')
print('new undefined dependencies: fg345w_enabled, fg345w_select')
print('EXP347_INTEGRATION_PASS')
