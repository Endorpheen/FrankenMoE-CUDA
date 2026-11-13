#!/usr/bin/env python3
"""EXP264: integrate the EXP-263 copy decisions into a copy of llama-moe-cache.cpp and check the ABI delta.

The native loop stays as the fallback behind fg263c_enabled(); the Goose entry points replace it only when the switch
is on (default on, as rule 10 requires). The object is compiled with the flint build's own command, and its undefined
symbol set is compared with the unmodified object's.
"""
from pathlib import Path
import json, shlex, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
E = F / 'experiments/EXP-2026-11-12-264-moe-cache-copy-integration'
I = E / 'integration'
I.mkdir(parents=True, exist_ok=True)
(I / 'llama-moe-cache.cpp').write_text((FL / 'src/llama-moe-cache.cpp').read_text())

p = I / 'llama-moe-cache.cpp'
src = p.read_text()

decls = '''
// Goose copy decisions (EXP-263). Default on; GGML_GOOSE_MOE_CACHE_COPY=0 restores the native loops.
extern "C" int     fg263c_enabled(void);
extern "C" int64_t fg263c_run_from_slot(uint64_t slots, int64_t e, int64_t last);
extern "C" int64_t fg263c_run_from_pairs(uint64_t pairs, int64_t i, int64_t n_fills);
extern "C" int64_t fg263c_stats_class(int64_t n_ids, int64_t n_expert_used);
'''
anchor = 'namespace {\n'
assert src.count(anchor) == 1
src = src.replace(anchor, decls + '\n' + anchor, 1)

old_slot = '''        int64_t n = 1;
        while (e + n <= last && slots[e + n] == slots[e] + n) {
            n++;
        }'''
new_slot = '''        int64_t n = 1;
        if (fg263c_enabled()) {
            n = fg263c_run_from_slot((uint64_t) (uintptr_t) slots, e, last);
        } else {
            while (e + n <= last && slots[e + n] == slots[e] + n) {
                n++;
            }
        }'''
assert src.count(old_slot) == 1
src = src.replace(old_slot, new_slot, 1)

old_fill = '''                size_t n = 1;
                while (i + n < fills.size() && fills[i + n].expert == fills[i].expert + (int32_t) n && fills[i + n].slot == fills[i].slot + (int32_t) n) {
                    n++;
                }'''
new_fill = '''                size_t n = 1;
                if (fg263c_enabled()) {
                    n = (size_t) fg263c_run_from_pairs((uint64_t) (uintptr_t) fills.data(), (int64_t) i, (int64_t) fills.size());
                } else {
                    while (i + n < fills.size() && fills[i + n].expert == fills[i].expert + (int32_t) n && fills[i + n].slot == fills[i].slot + (int32_t) n) {
                        n++;
                    }
                }'''
assert src.count(old_fill) == 1
src = src.replace(old_fill, new_fill, 1)

old_stats = '        stats & st = ids.size() <= (size_t) 8*n_expert_used ? stats_small : stats_large;'
new_stats = '''        const int64_t bucket = fg263c_enabled()
            ? fg263c_stats_class((int64_t) ids.size(), (int64_t) n_expert_used)
            : ((int64_t) ids.size() <= 8*(int64_t) n_expert_used ? 0 : 1);
        stats & st = bucket == 0 ? stats_small : stats_large;'''
assert src.count(old_stats) == 1
src = src.replace(old_stats, new_stats, 1)
p.write_text(src)

db = json.loads((FL / 'build-cuda/compile_commands.json').read_text())
row = [r for r in db if r['file'].endswith('/src/llama-moe-cache.cpp')][0]
cmd = shlex.split(row['command'])
cmd[cmd.index('-c') + 1] = str(p)
cmd[cmd.index('-o') + 1] = str(E / 'moe-cache-goose.o')
q = subprocess.run(cmd, cwd=row['directory'], capture_output=True, text=True)
(E / 'compile.stderr').write_text(q.stderr)
(E / 'compile.stdout').write_text(q.stdout)
print('compile rc =', q.returncode)
if q.returncode:
    print((q.stdout + q.stderr)[-2000:]); sys.exit(1)


def undef(o):
    s = subprocess.run(['nm', '-u', str(o)], capture_output=True, text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}


native = undef(FL / 'build-cuda/src/CMakeFiles/llama.dir/llama-moe-cache.cpp.o')
goose = undef(E / 'moe-cache-goose.o')
added = sorted(goose - native)
removed = sorted(native - goose)
print('undefined added:', added)
print('undefined removed:', removed)
assert all(a.startswith('fg263c_') for a in added), added
assert not removed, removed
(E / 'abi.json').write_text(json.dumps(dict(added=added, removed=removed,
                                            native_count=len(native), goose_count=len(goose)), indent=2) + '\n')
print('PASS integration object compiles with only fg263c_ undefined symbols added')
