#!/usr/bin/env python3
# Mutation run for the diff slice. Two halves:
#   python3 mutate2.py 1 | 2
from pathlib import Path
import subprocess, sys

B = Path('/tmp/opencode/sub-568')
SRC = B / 'base/runtime.goose'
RUN = B / 'run'
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'
L = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
D = L / 'build-cuda/common/CMakeFiles/llama-common.dir'
OBJS = [str(D / p) for p in ('jinja/lexer.cpp.o', 'jinja/parser.cpp.o', 'jinja/runtime.cpp.o',
                             'jinja/value.cpp.o', 'jinja/string.cpp.o', 'jinja/caps.cpp.o',
                             'unicode.cpp.o', 'json.cpp.o', 'trie.cpp.o')]

M1 = [
    ('the reasoning entry is never built',
     '    if df_eq(preas, 0, prlen, nreas, 0, nrlen) == 0 {',
     '    if false {'),
    ('the content entry is never built',
     '    if df_eq(pcont, 0, pclen, ncont, 0, nclen) == 0 {',
     '    if false {'),
    ('a shrinking tool-call count is accepted',
     '    if ncalls < pcalls {',
     '    if false {'),
    ('a tool-name mismatch is accepted',
     '        if p_nl != 0 && n_nl != 0 &&',
     '        if false &&'),
    ('a shrink to a prefix becomes an error',
     '        return 1, 0;',
     '        return 0 - 1, 0;'),
    ('the growth test is inverted',
     '    if df_prefix(prev, po, pl, next, no, nl) == 1 {\n        return 0, nl - pl;',
     '    if df_prefix(next, no, nl, prev, po, pl) == 1 {\n        return 0, nl - pl;'),
    ('the last previous call needs an arguments delta to be reported',
     '        if adl > 0 || name_changed == 0 || id_changed == 0 {',
     '        if adl > 0 {'),
]

M2 = [
    ('id and name are not reported together',
     '                if id_changed == 0 || name_changed == 0 {',
     '                if name_changed == 0 {'),
    ('appended calls are not reported',
     '    while c < ncalls {',
     '    while false {'),
    ('the entry stride is seven',
     'const DF_STRIDE: i64 = 8;',
     'const DF_STRIDE: i64 = 7;'),
    ('the delta cursor is not advanced after the first string',
     '    at = at + l1;\n    out[b + 4] = at;',
     '    out[b + 4] = at;'),
    ('the entry count is never written',
     '        out[0] = ne;',
     '        out[0] = 0;'),
    ('the arguments delta ignores the new call base offset',
     '                              ntcb, n_ao + p_al, adl, dbuf, cur);',
     '                              ntcb, p_al, adl, dbuf, cur);'),
    ('the monotone path builds entries into tiny buffers',
     '                              d0[0 .. 8], o0[0 .. 8], 0);',
     '                              d0[0 .. 8], o0[0 .. 8], 1);'),
]

MUT = {1: M1, 2: M2}


def run(cmd, timeout=400):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)


def main():
    half = int(sys.argv[1])
    orig = SRC.read_text()
    caught = survived = broken = 0
    for i, (name, old, new) in enumerate(MUT[half], 1):
        tag = f'h{half}-{i}'
        n = orig.count(old)
        if n != 1:
            print(f'MUT {tag} SETUP-ERROR occurrences={n} :: {name}')
            broken += 1
            continue
        (RUN / 'mut.goose').write_text(orig.replace(old, new))
        g = run(f'timeout 300 {G} --standalone -o {RUN}/mut.c --header {RUN}/mut.h {RUN}/mut.goose')
        if g.returncode != 0 or not (RUN / 'mut.c').exists():
            print(f'MUT {tag} BUILD-FAIL (goose) :: {name}')
            broken += 1
            continue
        c = run(f'timeout 290 gcc-13 -O0 -std=c11 -c {RUN}/mut.c -o {RUN}/mut.o')
        if c.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (gcc) :: {name}')
            broken += 1
            (RUN / 'mut.c').unlink(missing_ok=True)
            continue
        l = run(f'g++-13 {RUN}/diff_fixture.o {RUN}/mut.o ' + ' '.join(OBJS) +
                f' {L}/build-cuda/bin/libggml-base.so.0.27.0 -L{L}/build-cuda/bin -lllama-common'
                f' -Wl,-rpath,{L}/build-cuda/bin -o {RUN}/diff-mut')
        if l.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (link) :: {name}')
            broken += 1
        else:
            r = run(f'timeout 120 {RUN}/diff-mut')
            tail = [ln for ln in r.stdout.splitlines() if ln.startswith('FG568_DIFF')]
            line = tail[-1] if tail else 'no summary'
            if r.returncode != 0:
                caught += 1
                print(f'MUT {tag} CAUGHT :: {name} :: {line}')
            else:
                survived += 1
                print(f'MUT {tag} SURVIVED :: {name} :: {line}')
        for f in ('mut.c', 'mut.h', 'mut.o', 'diff-mut'):
            (RUN / f).unlink(missing_ok=True)
    print(f'HALF {half} total={len(MUT[half])} caught={caught} survived={survived} broken={broken}')
    return 0 if survived == 0 and broken == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
