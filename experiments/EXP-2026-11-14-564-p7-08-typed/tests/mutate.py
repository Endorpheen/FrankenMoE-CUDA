#!/usr/bin/env python3
# Mutation run for the typed-direction gate (EXP565).
# Each mutation is a single textual edit of source/runtime.goose; the gate is
# rebuilt from scratch (-O0) and a non-zero exit or a lost gate line counts as
# caught. Usage: mutate.py [lo hi]
from pathlib import Path
import subprocess
import sys

S = Path('/home/end0/AI/FrankenMoE-CUDA/experiments/EXP-2026-11-14-564-p7-08-typed')
SRC = S / 'source/runtime.goose'
ORIG = SRC.read_text()

M = [
    # ---- half 1: serialized shape, the join, and the plain caps logic ------
    ('the array type key is misspelled',
     'q = apps(out, q, "{\\"type\\":");',
     'q = apps(out, q, "{\\"typ\\":");'),
    ('the array text key is misspelled',
     'q = apps(out, q, ",\\"text\\":");',
     'q = apps(out, q, ",\\"txt\\":");'),
    ('the array separator is a semicolon',
     '''        if i > 0 {
            q = appc(out, q, 44);
        }
        let ty = blob[type_off[i] .. type_off[i] + type_len[i]];''',
     '''        if i > 0 {
            q = appc(out, q, 59);
        }
        let ty = blob[type_off[i] .. type_off[i] + type_len[i]];'''),
    ('the array drops its last part',
     '''    var q = appc(out, 0, 91);
    var i: i64 = 0;

    while i < n {''',
     '''    var q = appc(out, 0, 91);
    var i: i64 = 0;

    while i < n - 1 {'''),
    ('every array part is reported as text',
     '''        q = apps(out, q, "{\\"type\\":");
        q = g_esc_quoted(out, q, ty);''',
     '''        q = apps(out, q, "{\\"type\\":");
        q = g_esc_quoted(out, q, "text");'''),
    ('the array text field is filled from the type',
     '''        q = apps(out, q, ",\\"text\\":");
        q = g_esc_quoted(out, q, tx);''',
     '''        q = apps(out, q, ",\\"text\\":");
        q = g_esc_quoted(out, q, ty);'''),
    ('the join separator is a semicolon',
     '''        if add_nl != 0 {
            out[p] = 10;
            p = p + 1;
        }
        let tx = blob[text_off[i] .. text_off[i] + text_len[i]];''',
     '''        if add_nl != 0 {
            out[p] = 59;
            p = p + 1;
        }
        let tx = blob[text_off[i] .. text_off[i] + text_len[i]];'''),
    ('the join never clears the media flag',
     '''            last_media = 0;
        } else if bytes_is(ty, "media_marker") {''',
     '''            last_media = 1;
        } else if bytes_is(ty, "media_marker") {'''),
    ('a media marker no longer forces the string shape',
     'let media = contains_media(blob, type_off, type_len, n);',
     'let media: i64 = 0;'),
    ('only_string ignores the typed cap',
     'let only_string: bool = supports_string != 0 && supports_typed == 0;',
     'let only_string: bool = supports_string != 0;'),
    # ---- half 2: the caps rewrite, the empty branch, escaping, UTF-8 ------
    ('only_typed ignores the string cap',
     '''    let only_string: bool = supports_string != 0 && supports_typed == 0;
    let only_typed: bool = supports_string == 0 && supports_typed != 0;''',
     '''    let only_string: bool = supports_string != 0 && supports_typed == 0;
    let only_typed: bool = supports_typed != 0;'''),
    ('the string branch always joins, caps ignored',
     'if n > 0 && (only_string || media != 0) {',
     'if n > 0 {'),
    ('the string-content normalizer loses its closing brace',
     '''        q = g_esc_quoted(out, q, text);
        q = appc(out, q, 125);''',
     '''        q = g_esc_quoted(out, q, text);
        q = appc(out, q, 32);'''),
    ('the string-content normalizer ignores the string cap',
     '''    let only_typed: bool = supports_string == 0 && supports_typed != 0;
    let text: const u8[:] = blob[off .. off + len];''',
     '''    let only_typed: bool = supports_typed != 0;
    let text: const u8[:] = blob[off .. off + len];'''),
    ('no parts emits an empty array instead of an empty string',
     'return apps(out, 0, "\\"\\"");',
     'return apps(out, 0, "[]");'),
    ('the only_typed empty part becomes an empty array',
     'return apps(out, 0, "[{\\"type\\":\\"text\\",\\"text\\":\\"\\"}]");',
     'return apps(out, 0, "[]");'),
    ('the join + only_typed wrapper loses its closing brace',
     '''            q = g_esc_quoted(out, q, joined);
            q = appc(out, q, 125);''',
     '''            q = g_esc_quoted(out, q, joined);
            q = appc(out, q, 32);'''),
    ('control bytes use uppercase hex digits',
     '''fn g_emit_hex4(dst: u8[:], at: i64, v: i64) -> i64 {
    let digits: const u8[:] = "0123456789abcdef";''',
     '''fn g_emit_hex4(dst: u8[:], at: i64, v: i64) -> i64 {
    let digits: const u8[:] = "0123456789ABCDEF";'''),
    ('valid high bytes are escaped instead of copied raw',
     '''        if c < 32 {
            p = g_emit_hex4(dst, p, c);
        } else {
            p = m_appc(dst, p, c);
        }''',
     '''        if c < 32 {
            p = g_emit_hex4(dst, p, c);
        } else if c < 128 {
            p = m_appc(dst, p, c);
        } else {
            p = g_emit_hex4(dst, p, c);
        }'''),
    ('an overlong two-byte lead is accepted',
     '''        } else if c < 194 {
            return false;
        }''',
     '''        } else if c < 192 {
            return false;
        }'''),
    ('a surrogate sequence is accepted',
     'if c == 237 && s[i + 1] > 159 {',
     'if c == 237 && s[i + 1] > 191 {'),
]


def run():
    try:
        r = subprocess.run(['bash', str(S / 'tests' / 'build.sh'), 'mutate'], text=True,
                           capture_output=True, timeout=280, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG565_TYPED')]
    if line:
        return r.returncode, line[-1]
    tail = [x for x in (r.stdout + '\n' + r.stderr).splitlines() if x.strip()]
    detail = tail[-1][:140] if tail else 'empty log'
    if not any(x.startswith('build mode=') for x in r.stdout.splitlines()):
        return 1, 'build stopped: ' + detail
    return max(r.returncode, 1), 'binary produced no gate line: ' + detail


lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
caught = 0
missed = []
try:
    for idx, (label, old, new) in enumerate(M[lo:hi], start=lo):
        if ORIG.count(old) != 1:
            print(f'[skip] {idx} {label}: anchor count {ORIG.count(old)}')
            continue
        SRC.write_text(ORIG.replace(old, new, 1))
        rc, line = run()
        ok = rc != 0
        caught += ok
        if not ok:
            missed.append(idx)
        print(f'[{"caught" if ok else "MISSED"}] {idx} {label}: rc={rc} {line}')
finally:
    SRC.write_text(ORIG)
    rc, line = run()
    print(f'baseline after restore: rc={rc} {line}')
print(f'mutations {caught}/{hi-lo} caught (range {lo}:{hi}) missed={missed}')
