#!/usr/bin/env python3
# Mutation run for the request-schema slice (P8-07 S1).
# Each mutation edits the Goose port, regenerates the gate driver, runs it with
# `goose run` and compares with the native oracle. Any diff, run failure or
# missing output counts as caught. Usage: mutate_schema.py [lo hi]
from pathlib import Path
import subprocess
import sys

S = Path(__file__).resolve().parent.parent
SRC = S / 'source' / 'server-schema.goose'
ORIG = SRC.read_text()
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'


def rep(t, old, new):
    if t.count(old) != 1:
        raise AssertionError('anchor count %d: %r' % (t.count(old), old[:60]))
    return t.replace(old, new)


def elem(t, name, idx, val):
    """Replace one element of a generated one-line table."""
    lines = t.split('\n')
    for i, line in enumerate(lines):
        if line.startswith('let %s = [' % name):
            vals = line[len('let %s = [' % name):].rstrip('];').split(', ')
            vals[idx] = str(val)
            lines[i] = 'let %s = [%s];' % (name, ', '.join(vals))
            return '\n'.join(lines)
    raise AssertionError('table %s not found' % name)


def m_alias_order(t):
    # the names blob is one literal, so the anchor is the bare line text
    return rep(t, 'n_predict,max_completion_tokens,max_tokens',
                  'n_predict,max_tokens,max_completion_tokens')


def m_n_cmpl_limit(t):
    return rep(t, '        out[1] = np;', '        out[1] = np + 1;')


def m_n_predict_soft(t):
    return elem(t, 'SC_HARD', 8, 0)


def m_n_cmpl_default(t):
    return rep(t, '    I[12] = 1;         // n_cmpl', '    I[12] = 2;         // n_cmpl')


def m_nested_string_ok(t):
    return rep(t, '        if k == 13 {\n            return sc_type_err(I, MSG, "boolean", 3);\n        }',
                  '        if k == 13 {\n            return 0;\n        }')


def m_desc_drift(t):
    return rep(t, 'Include __verbose field in the response with additional',
                  'Include __verbose field in the responses with additional')


def m_clamp_direction(t):
    return rep(t, '            if F[row] < fl[0] {\n                F[row] = fl[0];\n            }',
                  '            if F[row] < fl[0] {\n                F[row] = fl[1];\n            }')


def m_stop_filter(t):
    return rep(t, '        if txt[i] == 255 {\n            p = sc_copy(ANT, p, txt, start, i - start);',
                  '        if txt[i] == 255 && i > start {\n            p = sc_copy(ANT, p, txt, start, i - start);')


def m_stop_fallback(t):
    return rep(t, '            let n = sc_stop_set(I, ANT, txt);\n            if n == 0 {\n'
                  '                sc_stop_fallback(I, ANT, BANT, B);\n            } else {',
                  '            let n = sc_stop_set(I, ANT, txt);\n            if n == 0 {\n'
                  '                I[76] = 0;\n            } else {')


def m_schema_precedence(t):
    return rep(t, '        if (iv & 1) != 0 && (iv & 2) == 0 {',
                  '        if (iv & 1) != 0 && (iv & 2) != 0 {')


def m_dry_base_threshold(t):
    return rep(t, '            if F[row] < 1.0 {', '            if F[row] < 0.0 {')


def m_tts_baseline(t):
    return rep(t, '    F[4] = 1.05;     // penalty_repeat', '    F[4] = 1.00;     // penalty_repeat')


M = [
    # ---- half 1: table shape, limits, defaults, nested, descriptions --------
    ('alias order: max_tokens before max_completion_tokens', m_alias_order),
    ('n_cmpl upper limit is n_parallel + 1', m_n_cmpl_limit),
    ('n_predict becomes a soft limit', m_n_predict_soft),
    ('the n_cmpl task default is 2', m_n_cmpl_default),
    ('a string include_usage is accepted', m_nested_string_ok),
    ('a description word is changed', m_desc_drift),
    # ---- half 2: eval, handlers, TTS ---------------------------------------
    ('a low float clamps to the upper limit', m_clamp_direction),
    ('stop drops the empty string', m_stop_filter),
    ('an empty stop array clears the base list', m_stop_fallback),
    ('json_schema no longer wins over grammar', m_schema_precedence),
    ('dry_base accepts values below 1.0', m_dry_base_threshold),
    ('the TTS penalty baseline is 1.0', m_tts_baseline),
]


def run():
    subprocess.run(['python3', str(S / 'tests' / 'gen_schema.py')],
                   capture_output=True, text=True, timeout=120)
    try:
        r = subprocess.run([G, 'run', str(S / 'build' / 'gate_schema.goose')],
                           capture_output=True, text=True, timeout=300, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'run timeout'
    if r.returncode != 0:
        tail = (r.stderr or r.stdout).strip().splitlines()
        return 1, 'run failed: ' + (tail[-1][:120] if tail else 'no output')
    got = r.stdout
    want = (S / 'build' / 'native_schema.out').read_text()
    if got == want:
        return 0, 'identical'
    diff = sum(1 for a, b in zip(got.splitlines(), want.splitlines()) if a != b)
    diff += abs(len(got.splitlines()) - len(want.splitlines()))
    return 1, f'{diff} differing lines'


lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
caught = 0
missed = []
try:
    for idx, (label, fn) in enumerate(M[lo:hi], start=lo):
        try:
            SRC.write_text(fn(ORIG))
        except AssertionError as e:
            print(f'[skip] {idx} {label}: {e}')
            SRC.write_text(ORIG)
            continue
        rc, detail = run()
        ok = rc != 0
        caught += ok
        if not ok:
            missed.append(idx)
        print(f'[{"caught" if ok else "MISSED"}] {idx} {label}: {detail}')
        SRC.write_text(ORIG)
finally:
    SRC.write_text(ORIG)
    rc, detail = run()
    print(f'baseline after restore: rc={rc} {detail}')
print(f'mutations {caught}/{hi-lo} caught (range {lo}:{hi}) missed={missed}')
