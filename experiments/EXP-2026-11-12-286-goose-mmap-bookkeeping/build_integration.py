#!/usr/bin/env python3
"""EXP286 (integration): wire the mmap/lock bookkeeping into a copy of the reconstructed loader."""
from pathlib import Path
import json, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
SRC = F / 'runtime/reconstructed/src/llama-model-loader.cpp'
E = F / 'experiments/EXP-2026-11-12-286-goose-mmap-bookkeeping'
I = E / 'integration'
I.mkdir(parents=True, exist_ok=True)
src = SRC.read_text()

decls = '''
// Goose mmap/lock bookkeeping (EXP-286). Default on; GGML_GOOSE_MMAP_BOOK=0 restores the native expressions.
extern "C" int     fg286m_enabled(void);
extern "C" int64_t fg286m_should_lock(int64_t has_locks, int64_t is_lazy);
extern "C" uint8_t fg286m_range_update(int64_t first, int64_t second, int64_t off, int64_t size, uint64_t out);
'''
anchor = 'static const size_t kiB = 1024;'
assert src.count(anchor) == 1
src = src.replace(anchor, decls + '\n' + anchor, 1)

old_lock = '''                if (lmlocks && !lazy.has(cur)) {'''
new_lock = '''                const bool do_lock = fg286m_enabled()
                    ? fg286m_should_lock(lmlocks ? 1 : 0, lazy.has(cur) ? 1 : 0) != 0
                    : (lmlocks && !lazy.has(cur));
                if (do_lock) {'''
assert src.count(old_lock) == 1
src = src.replace(old_lock, new_lock, 1)

old_range = '''                auto & mmap_used = mmaps_used[weight->idx];
                mmap_used.first  = std::min(mmap_used.first,  weight->offs);
                mmap_used.second = std::max(mmap_used.second, weight->offs + n_size);'''
new_range = '''                auto & mmap_used = mmaps_used[weight->idx];
                if (fg286m_enabled()) {
                    int64_t rng[2] = { (int64_t) mmap_used.first, (int64_t) mmap_used.second };
                    fg286m_range_update(rng[0], rng[1], (int64_t) weight->offs, (int64_t) n_size,
                                        (uint64_t) (uintptr_t) rng);
                    mmap_used.first  = (size_t) rng[0];
                    mmap_used.second = (size_t) rng[1];
                } else {
                    mmap_used.first  = std::min(mmap_used.first,  weight->offs);
                    mmap_used.second = std::max(mmap_used.second, weight->offs + n_size);
                }'''
assert src.count(old_range) == 1
src = src.replace(old_range, new_range, 1)
(I / 'llama-model-loader.cpp').write_text(src)

FLAGS = ['-DGGML_BACKEND_SHARED','-DGGML_SHARED','-DGGML_USE_CPU','-DGGML_USE_CUDA','-DLLAMA_BUILD',
         '-DLLAMA_SHARED','-Dllama_EXPORTS', f'-I{F}/runtime/reconstructed/src/.',
         '-I/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/src',
         '-I/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/include',
         '-I/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/include',
         '-O3','-DNDEBUG','-fPIC','-w','-std=c++17','-c', str(I / 'llama-model-loader.cpp'),
         '-o', str(E / 'llama-model-loader-goose.o')]
q = subprocess.run(['/usr/bin/g++-13'] + FLAGS, capture_output=True, text=True)
(E / 'compile.stderr').write_text(q.stderr)
print('compile rc =', q.returncode)
if q.returncode:
    print(q.stderr[-2500:]); sys.exit(1)

def undef(o):
    s = subprocess.run(['nm', '-u', str(o)], capture_output=True, text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}
native = undef(F / 'runtime/reconstructed/build/llama-model-loader.cpp.o')
goose = undef(E / 'llama-model-loader-goose.o')
added = sorted(x for x in goose - native if x.startswith('fg'))
print('fg symbols added:', added)
(E / 'abi.json').write_text(json.dumps(dict(added=added), indent=2) + '\n')
