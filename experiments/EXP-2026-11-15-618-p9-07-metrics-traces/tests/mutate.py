#!/usr/bin/env python3
# Mutation run for P9-07 (metrics/traces formats and workload validation).
#
#   python3 tests/mutate.py 1     # 6 mutations, the gate must fail on each
#   python3 tests/mutate.py 2     # 6 mutations
#
# Each mutation rewrites one decision in source/metrics-traces.goose, rebuilds the module
# at -O0 and reruns the same fixture. Builds are serial on purpose: the repository measures
# one build at a time.
from pathlib import Path
import shutil
import subprocess
import sys

B = Path(__file__).resolve().parent.parent
SRC = B / 'source' / 'metrics-traces.goose'
WORK = B / 'build' / 'mut'
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'
R = Path('/home/end0/AI/FrankenMoE-CUDA')
CANON = ' '.join(str(R / p) for p in [
    'runtime/core/src/metrics/decode_trace_sink.cpp',
    'runtime/core/src/metrics/route_trace_sink.cpp',
    'runtime/core/src/metrics/csv_metrics_sink.cpp',
    'runtime/core/src/moe/deterministic_workload.cpp',
])

M1 = [
    ('the quote predicate loses the comma',
     "        if c == ',' || c == '\"' || c == '\\n' {",
     "        if c == '\"' || c == '\\n' {"),
    ('the quote predicate quotes a carriage return (the stdlib csv rule)',
     "        if c == ',' || c == '\"' || c == '\\n' {",
     "        if c == ',' || c == '\"' || c == '\\n' || c == '\\r' {"),
    ('a newline inside a quoted field is written raw',
     "        } else if c == '\\n' {\n            p = mt_put(out, p, ' ');",
     "        } else if c == '\\n' {\n            p = mt_put(out, p, c);"),
    ('a quote inside a quoted field is not doubled',
     "            p = mt_put(out, p, '\"');\n            p = mt_put(out, p, '\"');",
     "            p = mt_put(out, p, '\"');"),
    ('the opening quote is skipped',
     "    var p: i64 = mt_put(out, at, '\"');\n    for c in v {\n        if c == '\"' {",
     "    var p: i64 = at;\n    for c in v {\n        if c == '\"' {"),
    ('a NaN weight takes the numeric arm',
     "    if w != w {\n        return MT_WEIGHT_NAN_TEXT;",
     "    if w < (0 as f64) {\n        return MT_WEIGHT_NAN_TEXT;"),
]

M2 = [
    ('the io row writes the requested size as the read size',
     "    p = mt_put(out, p, ',');\n    p = mt_put_u64(out, p, read_bytes as u64);",
     "    p = mt_put(out, p, ',');\n    p = mt_put_u64(out, p, req_bytes as u64);"),
    ('the route row drops the separator before the dropped flag',
     "    p = mt_put(out, p, ',');\n    p = mt_put_u64(out, p, dropped as u64);",
     "    p = mt_put_u64(out, p, dropped as u64);"),
    ('the reader counts commas inside quoted fields',
     "        } else if c == ',' && inq == 0 {",
     "        } else if c == ',' {"),
    ('the unescaper never collapses a doubled quote',
     "        if c == '\"' && i + 1 < last && field[i + 1] == '\"' {",
     "        if c == '\"' && i + 1 < last && field[i + 1] == 'x' {"),
    ('the route shape comparison ignores the phase',
     "    if exp_phase != got_phase || exp_base != got_base || exp_batch != got_batch {\n        return 0;\n    }",
     "    if exp_base != got_base || exp_batch != got_batch {\n        return 0;\n    }"),
    ('finish() checks the tokens before the routes',
     "    if route_cursor != route_n {\n        return MT_WL_ROUTES;\n    }\n    if emitted != output_n {\n        return MT_WL_TOKENS;\n    }",
     "    if emitted != output_n {\n        return MT_WL_TOKENS;\n    }\n    if route_cursor != route_n {\n        return MT_WL_ROUTES;\n    }"),
]

MUT = {1: M1, 2: M2}


def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=600)


def main():
    half = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    if half not in MUT:
        print('usage: mutate.py 1|2')
        return 2
    WORK.mkdir(parents=True, exist_ok=True)
    orig = SRC.read_text()
    caught = survived = broken = 0
    for i, (name, old, new) in enumerate(MUT[half], 1):
        tag = f'h{half}-{i}'
        n = orig.count(old)
        if n != 1:
            print(f'MUT {tag} SETUP-ERROR occurrences={n} :: {name}')
            broken += 1
            continue
        (WORK / 'p.goose').write_text(orig.replace(old, new))
        g = run(f'cd {WORK} && timeout 300 {G} --standalone -o p.c --header g.h p.goose')
        if g.returncode != 0 or not (WORK / 'p.c').exists():
            print(f'MUT {tag} BUILD-FAIL (goose) :: {name}')
            broken += 1
            continue
        c = run(f'cd {WORK} && timeout 300 gcc-13 -O0 -std=c11 -w -c p.c -o p.o')
        if c.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (gcc) :: {name}')
            broken += 1
            continue
        l = run(f'cd {WORK} && timeout 300 g++-13 -O0 -std=c++20 -I. '
                f'-I{R}/runtime/core/include -I{R}/runtime/core/src/moe '
                f'{B}/tests/metrics_traces_fixture.cpp {CANON} p.o -o pfix')
        if l.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (fixture) :: {name}')
            broken += 1
        else:
            r = run(f'cd {WORK} && timeout 120 ./pfix .')
            tail = [ln for ln in r.stdout.splitlines() if ln.startswith('FG617_')]
            line = ' | '.join(tail) if tail else 'no summary (stopped before it)'
            if r.returncode != 0:
                caught += 1
                print(f'MUT {tag} CAUGHT :: {name} :: {line}')
            else:
                survived += 1
                print(f'MUT {tag} SURVIVED :: {name} :: {line}')
        for f in ('p.c', 'g.h', 'p.o', 'pfix'):
            (WORK / f).unlink(missing_ok=True)
    shutil.rmtree(WORK, ignore_errors=True)
    print(f'HALF {half} total={len(MUT[half])} caught={caught} survived={survived} broken={broken}')
    return 0 if survived == 0 and broken == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
