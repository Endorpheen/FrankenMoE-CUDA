#!/usr/bin/env python3
# Mutation run for the CLI protocol slice (P9-06 S2).
#   python3 mutate_s2.py 1 | 2
# Each mutation rebuilds only the small standalone (goose + one gcc call, a
# second or two) and runs the 17-case gate.
from pathlib import Path
import shutil, subprocess, sys

B = Path(__file__).resolve().parent.parent
SRC = B / 'source' / 'cli_protocol.goose'
WORK = B / 'build' / 'run'
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'

M1 = [
    ('cancel is not recognised',
     '    if cp_eq(cmd_s, "cancel") == 1 {',
     '    if false {'),
    ('close is not recognised',
     '    if cp_eq(cmd_s, "close") == 1 {',
     '    if false {'),
    ('the id is taken from the wrong node',
     '        st[1] = jv[idn];',
     '        st[1] = jv[idn] + 1;'),
    ('the prompt is not copied out',
     '        st[5] = cp_put(prompt, 0, sb[off .. off + len]);',
     '        st[5] = 0;'),
    ('think is stored inverted',
     '        st[3] = jv[th];',
     '        st[3] = 1 - jv[th];'),
    ('the n_predict default is dropped',
     '    st[2] = dflt_npredict;',
     '    st[2] = 0;'),
    ('clear_kv defaults to false',
     '    st[4] = 1;',
     '    st[4] = 0;'),
]

M2 = [
    ('the reset flag is never emitted',
     '    if reset != 0 {',
     '    if false {'),
    ('the delta always resets',
     '    let ext = cp_prefix(reasoning, snap[0 .. pr]) &&\n              cp_prefix(text, snap[pr .. pr + pt]);',
     '    let ext = false;'),
    ('the text delta length is zero',
     '        st[3] = cp_put(delta, st[2], text[pt .. text.len]) - st[2];',
     '        st[3] = 0;'),
    ('the load size uses one decimal',
     '    p = cp_fixed(out, p, mb, 2);',
     '    p = cp_fixed(out, p, mb, 1);'),
    ('step and steps are swapped',
     '    p = cp_uint(out, p, n[0]);\n    p = cp_put(out, p, ",\\"steps\\":");',
     '    p = cp_uint(out, p, n[1]);\n    p = cp_put(out, p, ",\\"steps\\":");'),
    ('a refused prompt is reported as fatal',
     '    if cp_contains(msg, "exceeds the session n_ctx") == 1 {\n        return 1;',
     '    if cp_contains(msg, "exceeds the session n_ctx") == 1 {\n        return 0;'),
    ('quotes are not escaped on the wire',
     '        if c == 34 {\n            at = cp_put(dst, at, "\\\\\\"");',
     '        if false {\n            at = cp_put(dst, at, "\\\\\\"");'),
    ('the cancelled flag is never true',
     '    if n[1] != 0 {\n        p = cp_put(out, p, "true");',
     '    if false {\n        p = cp_put(out, p, "true");'),
]

MUT = {1: M1, 2: M2}


def run(cmd, timeout=300):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)


def main():
    half = int(sys.argv[1])
    orig = SRC.read_text()
    WORK.mkdir(exist_ok=True)
    (WORK / 'jsonparse.goose').write_text((B / 'source' / 'jsonparse.goose').read_text())
    caught = survived = broken = 0
    for i, (name, old, new) in enumerate(MUT[half], 1):
        tag = f'h{half}-{i}'
        n = orig.count(old)
        if n != 1:
            print(f'MUT {tag} SETUP-ERROR occurrences={n} :: {name}')
            broken += 1
            continue
        (WORK / 'p.goose').write_text(orig.replace(old, new))
        g = run(f'cd {WORK} && timeout 200 {G} --standalone -o p.c --header g.h p.goose')
        if g.returncode != 0 or not (WORK / 'p.c').exists():
            print(f'MUT {tag} BUILD-FAIL (goose) :: {name}')
            broken += 1
            continue
        c = run(f'cd {WORK} && timeout 200 gcc-13 -O1 -std=c11 -c p.c -o p.o')
        if c.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (gcc) :: {name}')
            broken += 1
            continue
        l = run(f'cd {WORK} && timeout 200 g++-13 {B}/tests/protocol_fixture.cpp {WORK}/p.o -I {WORK} -O1 '
                f'-std=c++17 -o pfix')
        if l.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (fixture) :: {name}')
            broken += 1
        else:
            r = run(f'cd {WORK} && timeout 120 ./pfix')
            tail = [ln for ln in r.stdout.splitlines() if ln.startswith('FG577_S2')]
            line = tail[-1] if tail else 'no summary (crash)'
            if r.returncode != 0:
                caught += 1
                print(f'MUT {tag} CAUGHT :: {name} :: {line}')
            else:
                survived += 1
                print(f'MUT {tag} SURVIVED :: {name} :: {line}')
        for f in ('p.c', 'g.h', 'p.o', 'pfix'):
            (WORK / f).unlink(missing_ok=True)
    print(f'HALF {half} total={len(MUT[half])} caught={caught} survived={survived} broken={broken}')
    return 0 if survived == 0 and broken == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
