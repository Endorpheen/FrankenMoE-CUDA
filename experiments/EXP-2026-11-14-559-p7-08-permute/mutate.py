#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

F = Path('/home/end0/AI/FrankenMoE-CUDA')
E = F / 'experiments/EXP-2026-11-14-559-p7-08-permute'
SRC = E / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('an empty permute is not epsilon', '    if n == 0 {\n        return eps0;\n    }', '    if n == 0 {\n        return peg_seq1(kind, cfirst, ccount, children, sidx, st, eps0);\n    }'),
    ('a single parser is duplicated', '    if n == 1 {\n        return peg_seq1(kind, cfirst, ccount, children, sidx, st, parsers[0]);\n    }', '    if n == 1 {\n        return peg_seq2(kind, cfirst, ccount, children, sidx, st, parsers[0], parsers[0]);\n    }'),
    ('every size falls back to a sequence', '    if n == 1 {', '    if true {'),
    ('each zero subset uses the shared epsilon', '                if mask - bit == 0 {\n                    rest = peg_eps_node(kind, cfirst, ccount, sidx, st);\n                } else {\n                    rest = rule_id[mask - bit];\n                }', '                rest = rule_id[0];'),
    ('rule names lose the prefix', '        while k < prefix.len {\n            namebuf[k] = prefix[k];\n            k = k + 1;\n        }', '        while false {\n            namebuf[k] = prefix[k];\n            k = k + 1;\n        }'),
    ('rule names are off by one', '        nl = peg_app_i64(namebuf, nl, mask);', '        nl = peg_app_i64(namebuf, nl, mask + 1);'),
    ('a single alternative is not a choice', '        if alt_count == 1 {\n            acc = peg_choice1(kind, cfirst, ccount, children, sidx, st, acc);\n        }', '        if false {\n            acc = peg_choice1(kind, cfirst, ccount, children, sidx, st, acc);\n        }'),
    ('alternatives are visited in reverse', '            let bit = 1 << i;', '            let bit = 1 << (n - 1 - i);'),
    ('the root is the previous rule', '    return rule_id[full];', '    return rule_id[full - 1];'),
    ('the cycle marker is spelled differently', '            let cyc = peg_app(out, st[70], "[cycle]");', '            let cyc = peg_app(out, st[70], "[cycle!]");'),
    ('the fallback threshold is five', '    if n > 6 {', '    if n > 5 {'),
    ('the mask walk skips a value', '        mask = mask + 1;', '        mask = mask + 2;'),
]








































def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=100, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG559_RUNTIME_PERMUTE')]
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
