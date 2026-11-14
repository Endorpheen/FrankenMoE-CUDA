#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-518-p7-08-jinja-format'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('format always takes the first argument', 'as_string_into(A, B, F, st, argv[ai], out[base + p..], 0, E)', 'as_string_into(A, B, F, st, argv[0], out[base + p..], 0, E)'),
    ('format accepts any brace pair', 'if i + 1 < src.len && src[i + 1] == 125 {', 'if i + 1 < src.len {'),
    ('format keeps the closing brace', '                    ai = ai + 1;\n                    i = i + 2;', '                    ai = ai + 1;\n                    i = i + 1;'),
    ('format writes a brace for every byte', 'out[base + p] = c;', 'out[base + p] = 125;'),
    ('indent default width is three', '            } else {\n                iw = 4;\n            }\n        } else {\n            iw = 4;', '            } else {\n                iw = 3;\n            }\n        } else {\n            iw = 3;'),
    ('indent string width becomes spaces', 'out[base + k] = ws[k];', 'out[base + k] = 32;'),
    ('indent first flag is inverted', 'if fflag != 0 {', 'if fflag == 0 {'),
    ('indent ignores blank', 'if lend > start || bflag != 0 {', 'if lend > start {'),
    ('indent always prefixes', 'if doind {', 'if true {'),
    ('indent drops the trailing newline', 'if src.len > 0 && src[src.len - 1] == 10 {', 'if false {'),
    ('indent drops the separator between lines', 'if p > cstart {', 'if false {'),
    ('indent counts lines instead of checking the output', 'if p == cstart {', 'if start == 0 {'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG518_RUNTIME_FMT')]
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
