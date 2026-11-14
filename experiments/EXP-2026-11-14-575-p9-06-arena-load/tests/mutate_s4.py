#!/usr/bin/env python3
# Mutation run for P9-06 S4 (arena load + build_parse_params).
#   python3 mutate_s4.py 1 | 2
from pathlib import Path
import subprocess, sys

B = Path(__file__).resolve().parent.parent
SRC = B / 'source' / 'parse_params.goose'
WORK = B / 'build' / 'run_s4'
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'

M1 = [
    ('a literal is loaded as an epsilon',
     '                    if pq_eq(t, "literal") {\n                        k = PK_LIT;',
     '                    if pq_eq(t, "literal") {\n                        k = PK_EPS;'),
    ('sequence children are not copied',
     '                    children[cur] = jv[p.jchild[jf[ca] + c]];',
     '                    children[cur] = 0;'),
    ('the child count of a sequence is dropped',
     '            ccount[i] = jn[ca];',
     '            ccount[i] = 0;'),
    ('the repetition minimum is never read',
     '            mn[i] = pj_int(p, node, "min_count", 0);',
     '            mn[i] = 0;'),
    ('the repetition maximum is never read',
     '            mx[i] = pj_int(p, node, "max_count", 0 - 1);',
     '            mx[i] = 0;'),
    ('until delimiters are not stored',
     '                        delims[wst[2]] = sid;',
     '                        wst[2] = wst[2];'),
    ('the delimiter count is dropped',
     '                dcount[i] = jn[da];',
     '                dcount[i] = 0;'),
    ('child ids are no longer bounds checked',
     '            if id < 0 || id >= n {',
     '            if false {'),
]

M2 = [
    ('the root id is no longer bounds checked',
     '    if r < 0 || r >= n {',
     '    if false {'),
    ('the rules object is not required',
     '    if rules < 0 || jk[rules] != JK_OBJECT {\n        return pq_err_c(err, out, 0 - 2, "JSON missing or invalid \'rules\' object");',
     '    if false {\n        return pq_err_c(err, out, 0 - 2, "JSON missing or invalid \'rules\' object");'),
    ('reasoning_format is left at NONE',
     '    out[2] = PQ_REASONING_AUTO;',
     '    out[2] = 0;'),
    ('the format is not copied',
     '    out[0] = format;',
     '    out[0] = 0;'),
    ('a failed load still claims AUTO',
     '    if st != 0 {\n        out[2] = 0;',
     '    if st != 0 {\n        out[2] = 1;'),
    ('the warn-once flag is never set',
     '    st[0] = 1;\n    return 1;',
     '    return 1;'),
    ('the string pool reuses one id',
     '    st[1] = sid + 1;',
     '    st[1] = sid;'),
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
        l = run(f'cd {WORK} && timeout 200 g++-13 {B}/tests/s4_fixture.cpp p.o -I {WORK} -O1 '
                f'-std=c++17 -o pfix -lm')
        if l.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (fixture) :: {name}')
            broken += 1
        else:
            r = run(f'cd {WORK} && timeout 120 ./pfix')
            tail = [ln for ln in r.stdout.splitlines() if ln.startswith('FG577_S4')]
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
