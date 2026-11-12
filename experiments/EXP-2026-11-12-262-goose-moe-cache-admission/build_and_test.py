#!/usr/bin/env python3
"""EXP262: build the admission module and run it against the native-shaped reference."""
from pathlib import Path
import subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-262-goose-moe-cache-admission'
S, B = E / 'source', E / 'build'
B.mkdir(exist_ok=True)


def run(label, argv):
    q = subprocess.run([str(a) for a in argv], capture_output=True, text=True)
    (B / (label + '.stdout')).write_text(q.stdout)
    (B / (label + '.stderr')).write_text(q.stderr)
    print('---', label, 'rc=', q.returncode)
    out = (q.stdout + q.stderr).strip()
    if out:
        print(out[-2000:])
    if q.returncode:
        sys.exit(q.returncode)


run('generate', ['python3', E / 'emit_sources.py'])
wrapper = S / 'admission-wrapper.c'
wrapper.write_text(
    '#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n'
    'static void my_fail(int64_t c){ fprintf(stderr, "FG262A validation code %lld\\n", (long long) c); abort(); }\n'
    'static int64_t my_ld(uint64_t b, int64_t i){ int64_t v; memcpy(&v, (void*)(uintptr_t)(b+(uint64_t)i), 8); return v; }\n'
    'static void my_st(uint64_t b, int64_t i, int64_t v){ memcpy((void*)(uintptr_t)(b+(uint64_t)i), &v, 8); }\n'
    '#define fg262a_fail my_fail\n#define fg262a_ld_i64 my_ld\n#define fg262a_st_i64 my_st\n'
    '#include "admission-functions.c"\n')
run('module', ['gcc-13', '-O2', '-fPIC', '-fno-strict-aliasing', '-ffp-contract=off', '-c', wrapper, '-o', B / 'admission.o'])
run('fixture', ['gcc-13', '-O2', '-c', E / 'tests/admission_fixture.c', '-o', B / 'fixture.o'])
run('link', ['gcc-13', B / 'fixture.o', B / 'admission.o', '-lm', '-o', B / 'fixture'])
run('run', [B / 'fixture'])
print('PASS admission fixture')
