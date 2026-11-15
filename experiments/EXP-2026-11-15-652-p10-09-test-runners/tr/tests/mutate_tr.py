#!/usr/bin/env python3
# Mutation run for the test-runner slice (P10-09 S1).
#
# Each mutation edits one decision in source/tr-testplan.goose, regenerates every
# gate driver and re-compares with the cached native truth (build/native.json,
# produced by gen_tr.py from a real ctest). Any disagreement, build failure or
# crash counts as caught. Usage:
#
#   mutate_tr.py 0 6     # half 1: selection and the regex
#   mutate_tr.py 6 12    # half 2: fixtures, statuses, summary, exit code
import subprocess
import sys
from pathlib import Path

S = Path(__file__).resolve().parent.parent
SRC = S / 'source' / 'tr-testplan.goose'
ORIG = SRC.read_text()

M = [
    # ---- half 1: selection, filters, the regex subset ----------------------
    ('the filter search becomes anchored',
     '    var start: i64 = 0;\n    while start <= s.len {',
     '    var start: i64 = 0;\n    while start < 0 {'),
    ('class negation is ignored',
     '    if neg != 0 {\n        return hit == 0;\n    }',
     '    if neg != 0 {\n        return hit != 0;\n    }'),
    ('dot stops matching',
     '    if a == 46 {\n        return c != 10;\n    }',
     '    if a == 46 {\n        return false;\n    }'),
    ('plus allows zero repetitions',
     '    if q == 43 {\n        mn = 1;\n    }',
     '    if q == 43 {\n        mn = 0;\n    }'),
    ('alternation is not split',
     '        if i == pat.len || pat[i] == 124 {',
     '        if i == pat.len {'),
    ('an empty exclude filter excludes everything',
     '    if exclude.len != 0 && tr_match(exclude, name) {\n        return false;\n    }',
     '    if tr_match(exclude, name) {\n        return false;\n    }'),
    # ---- half 2: fixtures, statuses, summary ------------------------------
    ('a failed setup stops blocking its dependents',
     '            if st == TR_FX_FAIL || st == TR_FX_NOTRUN {',
     '            if st == TR_FX_NOTRUN {'),
    ('a skipped setup blocks its dependents',
     '            if st == TR_FX_FAIL || st == TR_FX_NOTRUN {',
     '            if st == TR_FX_FAIL || st == TR_FX_NOTRUN || st == TR_FX_SKIP {'),
    ('cleanup tests are never appended',
     '                            if tr_has(clean_fx[c], f) && inlist[c] == 0 {',
     '                            if tr_has(clean_fx[c], f) && inlist[c] == 99 {'),
    ('setup tests are never appended',
     '                            if tr_has(setup_fx[s], f) && inlist[s] == 0 {',
     '                            if tr_has(setup_fx[s], f) && inlist[s] == 99 {'),
    ('dependencies are emitted after their dependents',
     '        if tr_has(dep[i], b) {\n            tr_dfs(b, n, dep, emitted, order, n_out);\n        }',
     '        if tr_has(dep[i], b) && b < 0 {\n            tr_dfs(b, n, dep, emitted, order, n_out);\n        }'),
    ('the disabled test is classified from its command',
     '    if disabled != 0 {\n        return TR_DISABLED;\n    }',
     '    if disabled < 0 {\n        return TR_DISABLED;\n    }'),
    ('the percentage truncates instead of rounding',
     '    return (100 * good + t / 2) / t;',
     '    return (100 * good) / t;'),
    ('the denominator counts disabled tests',
     '    p = tr_put_int(out, p, reported - disabled);',
     '    p = tr_put_int(out, p, reported);'),
    ('a skipped test stops being a skip',
     '    if has_skip != 0 && rc == skip_rc {',
     '    if has_skip == 0 && rc == skip_rc {'),
]


def gate():
    return subprocess.run(['python3', str(S / 'tests' / 'gen_tr.py')],
                          capture_output=True, text=True, timeout=600)


def main():
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    caught = 0
    for idx in range(lo, min(hi, len(M))):
        name, find, repl = M[idx]
        if ORIG.count(find) != 1:
            print('%-3d SETUP  %-52s (pattern occurs %d times)' % (idx, name, ORIG.count(find)))
            continue
        SRC.write_text(ORIG.replace(find, repl))
        try:
            r = gate()
            ok = r.returncode != 0
        finally:
            SRC.write_text(ORIG)
        if ok:
            caught += 1
            first = [l for l in (r.stdout + r.stderr).splitlines()
                     if l.startswith('FAIL') or 'error' in l]
            print('%-3d caught %-52s %s' % (idx, name, first[0].strip()[:70] if first else ''))
        else:
            print('%-3d MISSED %-52s' % (idx, name))
    print('caught %d/%d' % (caught, min(hi, len(M)) - lo))
    return 0 if caught == min(hi, len(M)) - lo else 1


if __name__ == '__main__':
    sys.exit(main())
