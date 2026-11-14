#!/usr/bin/env python3
# Mutation run for the decision core (P8-07 S4).
#
# Each mutation is one semantic change to source/decision-core.goose. The gate
# is rebuilt and run; any difference from the gate's own baseline output (a
# build failure counts too) means the mutation was caught. The source is
# restored after every mutation and once at the end.
#
#   python3 tests/mutate.py 0 6    # half 1
#   python3 tests/mutate.py 6 12   # half 2
from pathlib import Path
import subprocess
import sys

S = Path(__file__).resolve().parent.parent
SRC = S / 'source' / 'decision-core.goose'
ORIG = SRC.read_text()


def rep(t, old, new):
    if t.count(old) != 1:
        raise AssertionError('anchor count %d: %r' % (t.count(old), old[:70]))
    return t.replace(old, new)


# ---- half 1: parsing, option numbering and the question layout ------------
def m_score_levels(t):
    # a score question accepts 11 levels
    return rep(t, 't.jn[crit] < 2 || t.jn[crit] > 10) {', 't.jn[crit] < 2 || t.jn[crit] > 11) {')


def m_noul_swap(t):
    # noul options stay [false, true]
    return rep(t, '            var swap = ntf != 0;', '            var swap = false;')


def m_noul_alias(t):
    # the omni/h2o aliases are read the other way round
    return rep(t, 'let alias: const u8[:] = if side == 0 { "no" } else { "yes" };',
                  'let alias: const u8[:] = if side == 0 { "yes" } else { "no" };')


def m_h2o_instr_default(t):
    # an h2o question without instructions gets null, not ""
    return rep(t, '                instr = 0 - 2;', '                instr = 0 - 1;')


def m_choice_sort(t):
    # the choice options sort against the wrong neighbour
    return rep(t, 'dc_less_k(keybuf, koff[b], klen[b], koff[b - 1], klen[b - 1])',
               'dc_less_k(keybuf, koff[b - 1], klen[b - 1], koff[b], klen[b])')


def m_score_keys(t):
    # score level keys start at 1
    return rep(t, '                kb[0] = dc_puti(keybuf, kb[0], i);',
               '                kb[0] = dc_puti(keybuf, kb[0], i + 1);')


# ---- half 2: renderers, the cut and the float layout -----------------------
def m_kev_strip(t):
    # a leading tab is kept in a kev array item
    return rep(t, 'while q < m && (buf[q] == 32 || buf[q] == 9 || buf[q] == 10 || buf[q] == 13) {',
               'while q < m && (buf[q] == 32 || buf[q] == 10 || buf[q] == 13) {')


def m_escape_marker(t):
    # the closing break character of an escaped special token is U+00A7
    return rep(t, '''            p = dc_putc(dst, p, 60);
            p = dc_putc(dst, p, 194);
            p = dc_putc(dst, p, 166);''', '''            p = dc_putc(dst, p, 60);
            p = dc_putc(dst, p, 194);
            p = dc_putc(dst, p, 167);''')


def m_py_float_exp(t):
    # the exponent form starts from 1e17
    return rep(t, '    if exp < 0 - 4 || exp >= 16 {', '    if exp < 0 - 4 || exp >= 17 {')


def m_py_float_dot(t):
    # an integral float prints .1 instead of .0
    return rep(t, '''        var z: i64 = exp - (dn - 1);

        while z > 0 {
            p = dc_putc(dst, p, 48);
            z = z - 1;
        }
        p = dc_putc(dst, p, 46);
        return dc_putc(dst, p, 48);''', '''        var z: i64 = exp - (dn - 1);

        while z > 0 {
            p = dc_putc(dst, p, 48);
            z = z - 1;
        }
        p = dc_putc(dst, p, 46);
        return dc_putc(dst, p, 49);''')


def m_cut_marker(t):
    # the cut marker is ".."
    return rep(t, '''    p = dc_putc(out, p, 10);
    p = dc_putc(out, p, 46);
    p = dc_putc(out, p, 46);
    p = dc_putc(out, p, 46);
    p = dc_putc(out, p, 10);''', '''    p = dc_putc(out, p, 10);
    p = dc_putc(out, p, 46);
    p = dc_putc(out, p, 46);
    p = dc_putc(out, p, 10);''')


def m_quote_close(t):
    # '>' is quoted with an upper-case escape
    return rep(t, 'p = dc_replace_all(dst, p, tmp[0 .. m1], ">", "\\\\u003e");',
               'p = dc_replace_all(dst, p, tmp[0 .. m1], ">", "\\\\u003E");')


M = [
    # ---- half 1 -----------------------------------------------------------
    ('a score question accepts 11 levels', m_score_levels),
    ('noul options are no longer swapped', m_noul_swap),
    ('the omni/h2o aliases are swapped', m_noul_alias),
    ('an h2o question defaults to null instructions', m_h2o_instr_default),
    ('the choice sort compares the wrong neighbour', m_choice_sort),
    ('score level keys start at 1', m_score_keys),
    # ---- half 2 -----------------------------------------------------------
    ('a kev array item keeps its leading tab', m_kev_strip),
    ('the escape uses U+00A7 as its second break', m_escape_marker),
    ('the float exponent form starts from 1e17', m_py_float_exp),
    ('an integral float prints .1', m_py_float_dot),
    ('the cut marker loses a dot', m_cut_marker),
    ("'>' is quoted in upper case", m_quote_close),
]


def build_and_run():
    r = subprocess.run(['bash', str(S / 'tests' / 'build.sh')], capture_output=True,
                       text=True, timeout=600, errors='replace')
    for line in r.stdout.splitlines():
        if line.startswith('FG_DECISION'):
            return r.stdout, line
    tail = (r.stderr or r.stdout).strip().splitlines()
    return None, 'build failed: ' + (tail[-1][:120] if tail else 'no output')


def main():
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    base, base_line = build_and_run()
    if base is None:
        print('baseline does not build: ' + base_line)
        return 2
    caught = 0
    missed = []
    try:
        for idx, (label, fn) in enumerate(M[lo:hi], start=lo):
            try:
                SRC.write_text(fn(ORIG))
            except AssertionError as e:
                print('[skip] %d %s: %s' % (idx, label, e))
                SRC.write_text(ORIG)
                continue
            out, line = build_and_run()
            ok = out is None or out != base
            caught += ok
            if not ok:
                missed.append(idx)
            print('[%s] %d %s: %s' % ('caught' if ok else 'MISSED', idx, label, line))
            SRC.write_text(ORIG)
    finally:
        SRC.write_text(ORIG)
        out, line = build_and_run()
        print('baseline after restore: %s' % line)
    print('mutations %d/%d caught (range %d:%d) missed=%s' % (caught, hi - lo, lo, hi, missed))
    return 0 if caught == hi - lo else 1


if __name__ == '__main__':
    sys.exit(main())
