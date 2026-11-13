#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-13-512-p7-08-jinja-filters'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('upper becomes lower', 'c = ascii_upper(c);\n            } else if filter_name_is(fname, "lower")', 'c = ascii_lower(c);\n            } else if filter_name_is(fname, "lower")'),
    ('capitalize lowers the first char', 'if i == 0 {\n                    c = ascii_upper(c);', 'if i == 0 {\n                    c = ascii_lower(c);'),
    ('count alias removed', 'fname = "length";', 'fname = "count";'),
    ('string coercion removed', 'if vk(A, st, sval) != V_STRING && vk(A, st, sval) != V_UNDEF && text_filter(fname) {', 'if false {'),
    ('length is off by one', 'return mk_int(A, F, st, vstr(B, A, st, sval).len);', 'return mk_int(A, F, st, vstr(B, A, st, sval).len + 1);'),
    ('strip does not trim the left side', 'while a < b && go != 0 {', 'while false && a < b && go != 0 {'),
    ('strip ignores its chars argument', 'if argc > 0 && vk(A, st, argv[0]) != V_UNDEF {', 'if false {'),
    ('replace drops the insertion at the end of an empty pattern', 'while j < src.len {\n                out[base + n] = src[j];\n                n = n + 1;\n                var k: i64 = 0;\n\n                while k < rep.len {\n                    out[base + n] = rep[k];\n                    n = n + 1;\n                    k = k + 1;\n                }', 'while j < src.len {\n                out[base + n] = src[j];\n                n = n + 1;'),
    ('replace consumes one byte less than the pattern', 'i = i + pat.len;', 'i = i + pat.len - 1;'),
    ('container length uses the wrong count', 'if vk(A, st, sval) == V_OBJECT {\n            return mk_int(A, F, st, vkc(A, st, sval));', 'if vk(A, st, sval) == V_OBJECT {\n            return mk_int(A, F, st, vkc(A, st, sval) + 1);'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG512_RUNTIME_FILTERS')]
    if line:
        return r.returncode, line[-1]
    # No gate line: either the build stopped (goose, gcc or link) or the binary
    # died before reporting. Both are detected differences, so both count as
    # caught; only the detail differs.
    tail = [x for x in (r.stdout + '\n' + r.stderr).splitlines() if x.strip()]
    detail = tail[-1][:140] if tail else 'empty log'
    if not any(x.startswith('build mode=') for x in r.stdout.splitlines()):
        return 1, 'build stopped: ' + detail
    return max(r.returncode, 1), 'binary produced no gate line: ' + detail


lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
caught = 0
try:
    for label, old, new in M[lo:hi]:
        if old not in ORIG:
            print('[skip]', label)
            continue
        SRC.write_text(ORIG.replace(old, new, 1))
        rc, line = run()
        if rc is None:
            print('[build-fail]', label)
            continue
        ok = rc != 0
        caught += ok
        print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
finally:
    SRC.write_text(ORIG)
    rc, line = run()
    print(f'baseline after restore: rc={rc} {line}')
print(f'mutations {caught}/{hi-lo} caught (range {lo}:{hi})')
