#!/usr/bin/env python3
# Mutation run for state validation and the status contract. Two halves:
#   python3 mutate4.py 1 | 2
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
    ('the content length is not checked against its buffer',
     '       st[SS_CONT] < 0 || st[SS_CONT] > rcont.len ||',
     '       false ||'),
    ('the reasoning length is not checked against its buffer',
     '       st[SS_REAS] < 0 || st[SS_REAS] > rreas.len ||',
     '       false ||'),
    ('the call table is not checked against the count',
     'st[SS_NCALLS] < 0 || st[SS_NCALLS] * SS_STRIDE > rtab.len {',
     'false {'),
    ('the input length is not checked against its buffer',
     'if st[SS_INLEN] < 0 || st[SS_INLEN] > inbuf.len ||',
     'if false ||'),
    ('a negative offset is accepted',
     'if poff < 0 || plen < 0 || poff + plen > rtcb.len {',
     'if plen < 0 || poff + plen > rtcb.len {'),
    ('a negative length is accepted',
     'if poff < 0 || plen < 0 || poff + plen > rtcb.len {',
     'if poff < 0 || poff + plen > rtcb.len {'),
    ('an offset past the tool buffer is accepted',
     'if poff < 0 || plen < 0 || poff + plen > rtcb.len {',
     'if poff < 0 || plen < 0 {'),
    ('only the first call is validated',
     'while pv < st[SS_NCALLS] {',
     'while pv < 1 {'),
]

M2 = [
    ('a bad state is reported as a small output buffer',
     'return ss_fail(st, 0 - 5);',
     'return ss_fail(st, 0 - 4);'),
    ('a failure is not written to the error slot',
     '    st[SS_ERR] = code;',
     '    st[SS_ERR] = 0;'),
    ('a rejection is not written to the error slot',
     '        st[SS_ERR] = 0 - 6;',
     '        st[SS_ERR] = 0;'),
    ('a successful publish clears the error slot',
     '    st[SS_NCALLS] = c_calls;\n    st[SS_STATUS] = 0;\n    st[SS_DIFF] = 0;',
     '    st[SS_NCALLS] = c_calls;\n    st[SS_STATUS] = 0;\n    st[SS_DIFF] = 0;\n    st[SS_ERR] = 0;'),
    ('the non-monotone name is wrong',
     'return ss_put(out, "not_monotone");',
     'return ss_put(out, "kept");'),
    ('the bad-state name is wrong',
     'return ss_put(out, "bad_state");',
     'return ss_put(out, "published");'),
    ('open() leaves the error slot set',
     '    st[SS_ERR] = 0;\n    return 0;\n}',
     '    st[SS_ERR] = 1;\n    return 0;\n}'),
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
        if n == 0 or (n > 1 and not (half == 2 and i == 1)):
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
        l = run(f'g++-13 {RUN}/contract_fixture.o {RUN}/mut.o ' + ' '.join(OBJS) +
                f' {L}/build-cuda/bin/libggml-base.so.0.27.0 -L{L}/build-cuda/bin -lllama-common'
                f' -Wl,-rpath,{L}/build-cuda/bin -o {RUN}/contract-mut')
        if l.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (link) :: {name}')
            broken += 1
        else:
            r = run(f'timeout 120 {RUN}/contract-mut')
            tail = [ln for ln in r.stdout.splitlines() if ln.startswith('FG568_CONTRACT')]
            line = tail[-1] if tail else 'no summary (crash)'
            if r.returncode != 0:
                caught += 1
                print(f'MUT {tag} CAUGHT :: {name} :: {line}')
            else:
                survived += 1
                print(f'MUT {tag} SURVIVED :: {name} :: {line}')
        for f in ('mut.c', 'mut.h', 'mut.o', 'contract-mut'):
            (RUN / f).unlink(missing_ok=True)
    print(f'HALF {half} total={len(MUT[half])} caught={caught} survived={survived} broken={broken}')
    return 0 if survived == 0 and broken == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
