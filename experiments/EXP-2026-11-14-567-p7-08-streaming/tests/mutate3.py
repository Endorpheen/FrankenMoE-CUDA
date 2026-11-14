#!/usr/bin/env python3
# Mutation run for monotone publication. Two halves:
#   python3 mutate3.py 1 | 2
from pathlib import Path
import subprocess, sys

B = Path('/home/end0/AI/FrankenMoE-CUDA/experiments/EXP-2026-11-14-567-p7-08-streaming')
SRC = B / 'source/runtime.goose'
RUN = B / 'run'
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'
L = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
D = L / 'build-cuda/common/CMakeFiles/llama-common.dir'
OBJS = [str(D / p) for p in ('jinja/lexer.cpp.o', 'jinja/parser.cpp.o', 'jinja/runtime.cpp.o',
                             'jinja/value.cpp.o', 'jinja/string.cpp.o', 'jinja/caps.cpp.o',
                             'unicode.cpp.o', 'json.cpp.o', 'trie.cpp.o')]

REJECT = """        if is_final != 0 {
            st[SS_FIN] = 1;
        }
        chat_stream_msg(st, rtab, out);
        return 0 - 6;"""

M1 = [
    ('the monotone check is skipped',
     '    if mono < 0 {\n        st[SS_DIFF] = mono;',
     '    if false {\n        st[SS_DIFF] = mono;'),
    ('a rejected candidate is reported as published',
     '        chat_stream_msg(st, rtab, out);\n        return 0 - 6;',
     '        chat_stream_msg(st, rtab, out);\n        return 0;'),
    ('rejection reported as the unparsed-output status',
     '        st[SS_DIFF] = mono;\n        st[SS_STATUS] = 0 - 6;',
     '        st[SS_DIFF] = mono;\n        st[SS_STATUS] = 0 - 3;'),
    ('the diff status of a rejection is dropped',
     '        st[SS_DIFF] = mono;',
     '        st[SS_DIFF] = 0;'),
    ('a rejected candidate overwrites the content length',
     '        st[SS_DIFF] = mono;',
     '        st[SS_DIFF] = mono;\n        st[SS_CONT] = c_cont;'),
    ('a rejected finish leaves the session unfinished',
     REJECT,
     '        if false {\n            st[SS_FIN] = 1;\n        }\n'
     '        chat_stream_msg(st, rtab, out);\n        return 0 - 6;'),
    ('the candidate is compared with itself',
     '        rcont, st[SS_CONT], rreas, st[SS_REAS], rtcb, rtab, st[SS_NCALLS],\n'
     '        wcont, c_cont, wreas, c_reas, wtcb, mout[3 .. 3 + c_calls * SS_STRIDE], c_calls);',
     '        wcont, c_cont, wreas, c_reas, wtcb, mout[3 .. 3 + c_calls * SS_STRIDE], c_calls,\n'
     '        wcont, c_cont, wreas, c_reas, wtcb, mout[3 .. 3 + c_calls * SS_STRIDE], c_calls);'),
]

M2 = [
    ('the empty-candidate guard is disabled',
     '    if c_cont == 0 && c_reas == 0 && c_calls == 0 {',
     '    if false {'),
    ('a publish leaves a negative diff status behind',
     '    st[SS_NCALLS] = c_calls;\n    st[SS_STATUS] = 0;\n    st[SS_DIFF] = 0;',
     '    st[SS_NCALLS] = c_calls;\n    st[SS_STATUS] = 0;\n    st[SS_DIFF] = 0 - 1;'),
    ('the candidate table is read one entry late',
     'mout[3 .. 3 + c_calls * SS_STRIDE]',
     'mout[3 + SS_STRIDE .. 3 + c_calls * SS_STRIDE + SS_STRIDE]'),
    ('a rejection publishes the candidate layout in out',
     REJECT,
     '        if is_final != 0 {\n            st[SS_FIN] = 1;\n        }\n'
     '        out[0] = c_cont;\n        out[1] = c_reas;\n        out[2] = c_calls;\n'
     '        return 0 - 6;'),
    ('the previous content length comes from the candidate',
     '        rcont, st[SS_CONT], rreas, st[SS_REAS],',
     '        rcont, c_cont, rreas, st[SS_REAS],'),
    ('the candidate table starts at index zero',
     'mout[3 .. 3 + c_calls * SS_STRIDE]',
     'mout[0 .. c_calls * SS_STRIDE]'),
    ('a rejected feed marks the session finished',
     REJECT,
     '        if true {\n            st[SS_FIN] = 1;\n        }\n'
     '        chat_stream_msg(st, rtab, out);\n        return 0 - 6;'),
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
        (RUN / 'mut.goose').write_text(orig.replace(old, new, 1))
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
        l = run(f'g++-13 {RUN}/mono_fixture.o {RUN}/mut.o ' + ' '.join(OBJS) +
                f' {L}/build-cuda/bin/libggml-base.so.0.27.0 -L{L}/build-cuda/bin -lllama-common'
                f' -Wl,-rpath,{L}/build-cuda/bin -o {RUN}/mono-mut')
        if l.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (link) :: {name}')
            broken += 1
        else:
            r = run(f'timeout 120 {RUN}/mono-mut')
            tail = [ln for ln in r.stdout.splitlines() if ln.startswith('FG568_MONO')]
            line = tail[-1] if tail else 'no summary'
            if r.returncode != 0:
                caught += 1
                print(f'MUT {tag} CAUGHT :: {name} :: {line}')
            else:
                survived += 1
                print(f'MUT {tag} SURVIVED :: {name} :: {line}')
        for f in ('mut.c', 'mut.h', 'mut.o', 'mono-mut'):
            (RUN / f).unlink(missing_ok=True)
    print(f'HALF {half} total={len(MUT[half])} caught={caught} survived={survived} broken={broken}')
    return 0 if survived == 0 and broken == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
