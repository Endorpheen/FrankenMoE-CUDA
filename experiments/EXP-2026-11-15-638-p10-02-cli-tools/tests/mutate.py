#!/usr/bin/env python3
# P10-02 mutation run: 12 mutations per slice, two halves of six.
#
#   python3 tests/mutate.py 1     # lb 6 + tz 6 + fp 6
#   python3 tests/mutate.py 2     # lb 6 + tz 6 + fp 6
#
# Each mutation rewrites one decision in one Goose module, regenerates that
# module at -O0, links its fixture and reruns the gate. A surviving mutation is
# a hole in the gate: every half must be 6/6 for every slice. Builds are serial
# on purpose.
from pathlib import Path
import shutil
import subprocess
import sys

B = Path(__file__).resolve().parent.parent
OUT = B / 'build'
WORK = OUT / 'mut'
GOOSE = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'
NATIVE = Path('/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp')
CC = 'gcc-13'
CXX = 'g++-13'

SRC = {
    'lb': B / 'source' / 'llama-bench-report.goose',
    'tz': B / 'source' / 'tokenize-cli.goose',
    'fp': B / 'source' / 'fit-params-line.goose',
}
FIX = {
    'lb': B / 'tests' / 'lb_fixture.cpp',
    'tz': B / 'tests' / 'tz_fixture.cpp',
    'fp': B / 'tests' / 'fp_fixture.cpp',
}
SUM = {'lb': 'FG637_LB', 'tz': 'FG637_TZ', 'fp': 'FG637_FP'}
HDR = {'lb': 'lb.h', 'tz': 'tz.h', 'fp': 'fp.h'}

# Half 1: format selection, aggregation, escaping, type table.
M1 = [
    ('lb', 'md format is read as upper case',
     '    if lb_eq(s, "md") {\n        return 4;\n    }',
     '    if lb_eq(s, "MD") {\n        return 4;\n    }'),
    ('lb', 'the md name is spelled markdown',
     '        return "md";', '        return "markdown";'),
    ('lb', 'row split mode is renamed',
     '    if v == 2 {\n        return "row";\n    }',
     '    if v == 2 {\n        return "rows";\n    }'),
    ('lb', 'empty average is one',
     '    if vals.len == 0 {\n        return 0;\n    }\n    var sum: i64 = 0;\n    var i: i64 = 0;\n    while i < vals.len {\n        sum = sum + vals[i];',
     '    if vals.len == 0 {\n        return 1;\n    }\n    var sum: i64 = 0;\n    var i: i64 = 0;\n    while i < vals.len {\n        sum = sum + vals[i];'),
    ('lb', 'stdev divides by n',
     '    let a: i64 = sq_sum / (vals.len - 1);', '    let a: i64 = sq_sum / vals.len;'),
    ('lb', 'no_kv_offload falls through to bool',
     '        lb_eq(field, "avg_ns") || lb_eq(field, "stddev_ns") || lb_eq(field, "no_op_offload") || lb_eq(field, "n_cpu_moe") ||\n        lb_eq(field, "fit_target") || lb_eq(field, "fit_min_ctx") || lb_eq(field, "flash_attn") {',
     '        lb_eq(field, "avg_ns") || lb_eq(field, "stddev_ns") || lb_eq(field, "no_op_offload") || lb_eq(field, "n_cpu_moe") ||\n        lb_eq(field, "fit_target") || lb_eq(field, "fit_min_ctx") || lb_eq(field, "flash_attn") || lb_eq(field, "no_kv_offload") {'),

    ('tz', 'stdin no longer wins over the file',
     '    if stdin_flag != 0 {\n        return 0;\n    }\n    if file_present != 0 {\n        return 1;\n    }',
     '    if stdin_flag != 0 {\n        return 1;\n    }\n    if file_present != 0 {\n        return 0;\n    }'),
    ('tz', 'a missing prompt is accepted',
     '    return 0 - 1;\n}', '    return 2;\n}'),
    ('tz', 'add_bos ignores --no-bos',
     '    if model_wants_add_bos != 0 && no_bos == 0 {',
     '    if model_wants_add_bos != 0 {'),
    ('tz', 'escapes are applied to --prompt again',
     '    if source == 0 || source == 1 {', '    if source >= 0 {'),
    ('tz', 'ids lose the space after the comma',
     '                at = tz_copy(", ", out, at);', '                at = tz_copy(",", out, at);'),
    ('tz', 'ids close with an extra newline',
     '        at = tz_copy("]\\n", out, at);', '        at = tz_copy("]\\n\\n", out, at);'),

    ('fp', 'ngl loses its sign',
     '    at = fp_put_int(out, at, ngl);', '    at = fp_put_int(out, at, fp_u32(ngl as f64));'),
    ('fp', 'trailing non-zero splits are trimmed too',
     '    while nd > 1 && splits[nd - 1] == 0.0 {',
     '    while nd > 1 && splits[nd - 1] != 0.0 {'),
    ('fp', 'a single device still prints -ts',
     '    if nd > 1 {', '    if nd > 0 {'),
    ('fp', 'the -ts separator is a slash',
     '                at = fp_putc(out, at, 44);\n            }\n            at = fp_put_int(out, at, fp_u32(splits[id]));',
     '                at = fp_putc(out, at, 47);\n            }\n            at = fp_put_int(out, at, fp_u32(splits[id]));'),
    ('fp', 'uint32 conversion rounds',
     '    return x as i64;', '    return (x + 0.5) as i64;'),
    ('fp', '-ot opens without the leading space',
     '            at = fp_copy(" -ot \\"", out, at);', '            at = fp_copy("-ot \\"", out, at);'),
]

# Half 2: row text, JSON/CSV, ranges, and the rest of the fitted line.
M2 = [
    ('lb', 'JSON backslashes are not doubled',
     '        } else if c == 92 {\n            at = lb_putc(out, at, 92);\n            at = lb_putc(out, at, 92);',
     '        } else if c == 92 {\n            at = lb_putc(out, at, 92);'),
    ('lb', 'zero is formatted as true',
     '        if lb_eq(value, "0") {\n            return lb_copy("false", out, 0);\n        }',
     '        if lb_eq(value, "0") {\n            return lb_copy("true", out, 0);\n        }'),
    ('lb', 'CSV fields lose the opening quote',
     'export fn lb_csv_escape(s: const u8[:], out: u8[:]) -> i64 {\n    var at: i64 = lb_putc(out, 0, 34);',
     'export fn lb_csv_escape(s: const u8[:], out: u8[:]) -> i64 {\n    var at: i64 = 0;'),
    ('lb', 'exactly 1 GiB is reported in MiB',
     '    if bytes < 1073741824 {', '    if bytes <= 1073741824 {'),
    ('lb', 'pp and tg are swapped',
     '    if np > 0 && ng == 0 {\n        at = lb_copy("pp", out, at);',
     '    if np > 0 && ng == 0 {\n        at = lb_copy("tg", out, at);'),
    ('lb', 'multiplication is added like a range',
     '            } else if op == 42 {', '            } else if op == 43 {'),

    ('tz', 'piece ids are five columns wide',
     '    var sp: i64 = 6 - digits;', '    var sp: i64 = 5 - digits;'),
    ('tz', 'the piece arrow loses its quote',
     '            at = tz_put_w6(out, at, tokens[i]);\n            at = tz_copy(" -> \'", out, at);',
     '            at = tz_put_w6(out, at, tokens[i]);\n            at = tz_copy(" ->", out, at);'),
    ('tz', 'the invalid-UTF-8 suffix is reworded',
     '                at = tz_copy("\' (utf-8 decode failure)\\n", out, at);',
     '                at = tz_copy("\' (utf8 decode failure)\\n", out, at);'),
    ('tz', 'the count line loses its wording',
     '        at = tz_copy("Total number of tokens: ", out, at);',
     '        at = tz_copy("Total tokens: ", out, at);'),
    ('tz', 'the count is off by one',
     '        at = tz_put_int(out, at, tokens.len);',
     '        at = tz_put_int(out, at, tokens.len + 1);'),
    ('tz', 'piece lines lose the trailing newline',
     '                at = tz_copy("\'\\n", out, at);', '                at = tz_copy("\'", out, at);'),

    ('fp', 'the override separator is a colon',
     '        at = fp_putc(out, at, 61);', '        at = fp_putc(out, at, 58);'),
    ('fp', 'the override quote is always closed',
     '    if any != 0 {', '    if 1 != 0 {'),
    ('fp', 'the backend name is replaced by the pattern',
     '            at = fp_putc(out, at, bufts[boff[i] + k]);',
     '            at = fp_putc(out, at, pats[poff[i] + k]);'),
    ('fp', 'the device count is not trimmed',
     '    let nd: i64 = fp_ts_n(splits, ndev);', '    let nd: i64 = ndev;'),
    ('fp', 'the line ends with a space',
     '    return fp_putc(out, at, 10);', '    return fp_putc(out, at, 32);'),
    ('fp', 'the context flag is spelled -ctx',
     '    at = fp_copy("-c ", out, at);', '    at = fp_copy("-ctx ", out, at);'),
]

MUT = {1: M1, 2: M2}


def run(cmd, cwd=None):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=900, cwd=cwd)


def main():
    half = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    if half not in MUT:
        print('usage: mutate.py 1|2')
        return 2
    WORK.mkdir(parents=True, exist_ok=True)
    tot = {s: {'caught': 0, 'survived': 0, 'broken': 0} for s in SRC}
    for i, (sl, name, old, new) in enumerate(MUT[half], 1):
        tag = f'h{half}-{i}'
        d = WORK / tag
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)
        base = SRC[sl].read_text()
        n = base.count(old)
        if n != 1:
            print(f'MUT {tag} SETUP-ERROR occurrences={n} :: {sl} :: {name}')
            tot[sl]['broken'] += 1
            continue
        (d / 'p.goose').write_text(base.replace(old, new, 1))
        g = run(f'timeout 300 {GOOSE} --standalone -o p.c --header {HDR[sl]} p.goose', cwd=d)
        if g.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (goose) :: {name}\n{g.stderr.strip()[:200]}')
            tot[sl]['broken'] += 1
            continue
        c = run(f'timeout 300 {CC} -O0 -std=c11 -w -c p.c -o p.o', cwd=d)
        if c.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (gcc) :: {name}')
            tot[sl]['broken'] += 1
            continue
        nat = ''
        flags = ''
        if sl == 'lb':
            nat = f'{OUT / "native_lb.o"}'
            flags = '-Wl,--gc-sections'
        link = (f'timeout 300 {CXX} -O0 -std=c++20 -I{d} {FIX[sl]} p.o {nat} {flags} -o pfix')
        l = run(link, cwd=d)
        if l.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (fixture) :: {name}\n{l.stderr.strip()[:200]}')
            tot[sl]['broken'] += 1
        else:
            r = run('timeout 300 ./pfix', cwd=d)
            line = 'no summary (stopped before it)'
            for ln in r.stdout.splitlines():
                if ln.startswith(SUM[sl]):
                    line = ln
            if r.returncode != 0:
                tot[sl]['caught'] += 1
                print(f'MUT {tag} CAUGHT :: {sl} :: {name} :: {line}')
            else:
                tot[sl]['survived'] += 1
                print(f'MUT {tag} SURVIVED :: {sl} :: {name} :: {line}')
        shutil.rmtree(d, ignore_errors=True)
    shutil.rmtree(WORK, ignore_errors=True)
    bad = 0
    for sl in SRC:
        t = tot[sl]
        print(f'HALF {half} slice={sl} total={t["caught"] + t["survived"] + t["broken"]} '
              f'caught={t["caught"]} survived={t["survived"]} broken={t["broken"]}')
        bad += t['survived'] + t['broken']
    print(f'HALF {half} overall total={len(MUT[half])} caught={sum(t["caught"] for t in tot.values())} '
          f'survived={sum(t["survived"] for t in tot.values())} broken={sum(t["broken"] for t in tot.values())}')
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
