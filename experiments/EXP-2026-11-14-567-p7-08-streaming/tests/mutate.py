#!/usr/bin/env python3
# Mutation run for the streaming step. Two halves, six/seven mutations each:
#   python3 mutate.py 1 | 2
# A mutation is CAUGHT when the gate reports failures or the build breaks.
from pathlib import Path
import subprocess, sys, os

B = Path('/tmp/opencode/sub-568')
SRC = B / 'base/runtime.goose'
RUN = B / 'run'
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'
L = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
D = L / 'build-cuda/common/CMakeFiles/llama-common.dir'
OBJS = [str(D / p) for p in ('jinja/lexer.cpp.o', 'jinja/parser.cpp.o', 'jinja/runtime.cpp.o',
                             'jinja/value.cpp.o', 'jinja/string.cpp.o', 'jinja/caps.cpp.o',
                             'unicode.cpp.o', 'json.cpp.o', 'trie.cpp.o')]

# (name, old, new) — every `old` must occur exactly once in the added block.
M1 = [
    ('finished guard disabled',
     '    if st[SS_FIN] != 0 {\n        st[SS_STATUS] = 0 - 2;',
     '    if false {\n        st[SS_STATUS] = 0 - 2;'),
    ('feed after finish reported as unparsed output',
     '    if st[SS_FIN] != 0 {\n        st[SS_STATUS] = 0 - 2;',
     '    if st[SS_FIN] != 0 {\n        st[SS_STATUS] = 0 - 3;'),
    ('a streaming FAIL is always terminal',
     'if ptype == SS_PARSE_FAIL && (is_final != 0 || pend <= 0) {',
     'if ptype == SS_PARSE_FAIL {'),
    ('a final FAIL is not terminal',
     'if ptype == SS_PARSE_FAIL && (is_final != 0 || pend <= 0) {',
     'if false {'),
    ('the partial-FAIL branch ignores is_partial',
     'if ptype == SS_PARSE_FAIL && (is_final != 0 || pend <= 0) {',
     'if ptype == SS_PARSE_FAIL && pend <= 0 {'),
    ('the empty-candidate guard is disabled',
     'if c_cont == 0 && c_reas == 0 && c_calls == 0 {',
     'if false {'),
    ('the empty-candidate guard ignores content',
     'if c_cont == 0 && c_reas == 0 && c_calls == 0 {',
     'if c_calls == 0 {'),
]

M2 = [
    ('a frozen step publishes the empty candidate',
     '        chat_stream_msg(st, rtab, out);\n        return 1;',
     '        out[0] = c_cont;\n        return 1;'),
    ('the published content length comes from reasoning',
     '    st[SS_CONT] = c_cont;',
     '    st[SS_CONT] = c_reas;'),
    ('reasoning is never copied into the result',
     '    m_app(rreas, 0, wreas, 0, c_reas);',
     '    m_app(rreas, 0, wreas, 0, 0);'),
    ('the dense call table never advances',
     'tcb_len = m_app(rtcb, tcb_len, wtcb, mout[b + 2 * f], len);',
     'tcb_len = tcb_len;'),
    ('every call field starts at offset zero',
     'rtab[c * SS_STRIDE + 2 * f] = tcb_len;',
     'rtab[c * SS_STRIDE + 2 * f] = 0;'),
    ('the message layout puts reasoning into content',
     '    out[0] = st[SS_CONT];',
     '    out[0] = st[SS_REAS];'),
    ('open() starts a finished session',
     '    st[SS_FIN] = 0;\n    st[SS_INLEN] = gp_len;',
     '    st[SS_FIN] = 1;\n    st[SS_INLEN] = gp_len;'),
]

MUT = {1: M1, 2: M2}


def run(cmd, log=None, timeout=400):
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)
    if log is not None:
        Path(log).write_text(r.stdout + r.stderr)
    return r


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
        l = run(f'g++-13 {RUN}/fixture.o {RUN}/mut.o ' + ' '.join(OBJS) +
                f' {L}/build-cuda/bin/libggml-base.so.0.27.0 -L{L}/build-cuda/bin -lllama-common'
                f' -Wl,-rpath,{L}/build-cuda/bin -o {RUN}/fixture-mut')
        if l.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (link) :: {name}')
            broken += 1
        else:
            r = run(f'timeout 120 {RUN}/fixture-mut', log=RUN / f'mut-{tag}.log')
            tail = [ln for ln in Path(RUN / f'mut-{tag}.log').read_text().splitlines()
                    if ln.startswith('FG568_STREAM')]
            line = tail[-1] if tail else 'no summary'
            if r.returncode != 0:
                caught += 1
                print(f'MUT {tag} CAUGHT :: {name} :: {line}')
            else:
                survived += 1
                print(f'MUT {tag} SURVIVED :: {name} :: {line}')
        for f in ('mut.c', 'mut.h', 'mut.o', 'fixture-mut'):
            (RUN / f).unlink(missing_ok=True)
    print(f'HALF {half} total={len(MUT[half])} caught={caught} survived={survived} broken={broken}')
    return 0 if survived == 0 and broken == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
