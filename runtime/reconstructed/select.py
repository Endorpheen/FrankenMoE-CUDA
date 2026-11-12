#!/usr/bin/env python3
"""Pick, per file name, the newest integration copy that is a superset of the flint file.

Identity test: compile the copy with the flint flags and require its defined-symbol set to contain
every symbol the flint object defines. That rejects copies from the other line (work/), whose object
defines fewer symbols, without relying on file numbering.
"""
from pathlib import Path
import re, subprocess, sys

BASE = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
E = BASE/'experiments'
WORK = BASE/'runtime/reconstructed/work'
CXX = ['/usr/bin/g++-13', '-DGGML_BACKEND_SHARED', '-DGGML_SHARED', '-DGGML_USE_CPU', '-DGGML_USE_CUDA',
       '-DLLAMA_BUILD', '-DLLAMA_SHARED', '-Dllama_EXPORTS', f'-I{FL}/src/.', f'-I{FL}/build-cuda/src',
       f'-I{FL}/src/../include', f'-I{FL}/ggml/src/../include', '-O3', '-DNDEBUG', '-fPIC',
       '-Wno-missing-declarations', '-Wno-missing-noreturn', '-w']

def syms(obj):
    out = subprocess.run(['nm', '--defined-only', str(obj)], capture_output=True, text=True).stdout
    syms = set()
    for line in out.splitlines():
        if not line.strip(): continue
        name = line.split()[-1]
        if name.startswith('.LC'): continue
        # compiler-generated variants of the same function are not part of the interface
        if any(x in name for x in ('std::', '.part.', '.isra.', '.constprop.', '.cold',
                                   'St6vector', 'St8_Rb_tree', 'St4pair', 'St3map', 'St6atomic')):
            continue
        syms.add(name)
    return syms

def compile_one(src, obj):
    q = subprocess.run(CXX + ['-o', str(obj), '-c', str(src)], capture_output=True, text=True)
    return q.returncode, q.stderr

WORK.mkdir(parents=True, exist_ok=True)
cands = {}
for p in E.glob('EXP-*/integration/*.cpp'):
    m = re.search(r'EXP-(\d{4}-\d{2}-\d{2})-(\d+)', str(p))
    if not m: continue
    # our own composite copies carry an extra suffix; the plain files are the ones to overlay
    if any(p.name.startswith(x) and p.name != x for x in
           ('llama-batch-both', 'llama-batch-flint', 'llama-batch-classify',
            'llama-kv-cache-both', 'llama-kv-cache-flint', 'llama-kv-cache-clear')):
        continue
    cands.setdefault(p.name, []).append((int(m.group(2)), p))

for name, lst in sorted(cands.items()):
    flint = FL/'src'/name
    if not flint.exists():
        continue
    obj_flint = WORK/(name + '.flint.o')
    rc, err = compile_one(flint, obj_flint)
    if rc: print(f'{name}: flint does not compile'); continue
    need = syms(obj_flint)
    chosen = None
    for num, p in sorted(lst, reverse=True):
        obj = WORK/(name + f'.{num}.o')
        rc, err = compile_one(p, obj)
        if rc: continue
        got = syms(obj)
        if need <= got:
            chosen = (num, p)
            break
    print(f'{name:28s} flint_syms={len(need):4d} chosen={chosen[0] if chosen else "-"} {chosen[1] if chosen else ""}')
