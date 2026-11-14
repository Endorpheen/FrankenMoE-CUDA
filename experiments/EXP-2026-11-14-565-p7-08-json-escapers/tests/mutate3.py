#!/usr/bin/env python3
# Mutation run for the value-model escaping slice (EXP567).
# Each mutation is one textual edit of one module; that module is rebuilt with
# -O0 and its gate must fail. Usage: mutate3.py [lo hi]
from pathlib import Path
import subprocess
import sys

S = Path('/home/end0/AI/FrankenMoE-CUDA/experiments/EXP-2026-11-14-565-p7-08-json-escapers')

M = [
    # ---- half 1: value.goose ----------------------------------------------
    ('value', 'backspace goes out as a raw byte',
     'p = apps(out, p, "\\\\b");', 'p = appc(out, p, 8);'),
    ('value', 'formfeed goes out as a raw byte',
     'p = apps(out, p, "\\\\f");', 'p = appc(out, p, 12);'),
    ('value', 'newline goes out as a raw byte',
     'p = apps(out, p, "\\\\n");', 'p = appc(out, p, 10);'),
    ('value', 'carriage return goes out as a raw byte',
     'p = apps(out, p, "\\\\r");', 'p = appc(out, p, 13);'),
    ('value', 'tab goes out as a raw byte',
     'p = apps(out, p, "\\\\t");', 'p = appc(out, p, 9);'),
    ('value', 'control hex digits go uppercase',
     '''        if c < 0x20 {
            let d: const u8[:] = "0123456789abcdef";''',
     '''        if c < 0x20 {
            let d: const u8[:] = "0123456789ABCDEF";'''),
    ('value', 'the closing quote is dropped',
     '''        i = i + 1;
    }
    return appc(out, p, '"');
}''',
     '''        i = i + 1;
    }
    return p;
}'''),
    # ---- half 2: value-json.goose -----------------------------------------
    ('value-json', 'backslash is left unescaped',
     '''        if c == 92 {
            p = appc(out, p, 92);
            p = appc(out, p, 92);''',
     '''        if c == 92 {
            p = appc(out, p, 92);'''),
    ('value-json', 'the quote is left unescaped',
     '''        if c == '"' {
            p = appc(out, p, 92);
            p = appc(out, p, '"');''',
     '''        if c == '"' {
            p = appc(out, p, '"');'''),
    ('value-json', 'high bytes are escaped as control',
     'if c < 0x20 {', 'if c < 0x20 || c >= 127 {'),
    ('value-json', 'control bytes skip the escape',
     'if c < 0x20 {', 'if c < 0x10 {'),
    ('value-json', 'esc loses its offset',
     '''fn esc(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    return json_string(out, at, s);
}''',
     '''fn esc(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    return json_string(out, 0, s);
}'''),
    ('value-json', 'esc writes an empty string',
     '''fn esc(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    return json_string(out, at, s);
}''',
     '''fn esc(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    return json_string(out, at, s[0..0]);
}'''),
    ('value-json', 'the escape hex base is wrong',
     '''            p = apps(out, p, "\\\\u00");
            p = appc(out, p, d[ ( (c as i64) / 16) % 16]);
            p = appc(out, p, d[ (c as i64) % 16]);''',
     '''            p = apps(out, p, "\\\\u00");
            p = appc(out, p, d[ ( (c as i64) / 8) % 16]);
            p = appc(out, p, d[ (c as i64) % 16]);'''),
]


def run(mod):
    try:
        r = subprocess.run(['bash', str(S / 'tests' / 'build3.sh'), mod, 'mutate'], text=True,
                           capture_output=True, timeout=280, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout'
    line = [x for x in r.stdout.splitlines() if x.startswith('UNI3_VALUES')]
    if line:
        return r.returncode, line[-1]
    tail = [x for x in (r.stdout + '\n' + r.stderr).splitlines() if x.strip()]
    detail = tail[-1][:140] if tail else 'empty log'
    if not any(x.startswith('build module=') for x in r.stdout.splitlines()):
        return 1, 'build stopped: ' + detail
    return max(r.returncode, 1), 'binary produced no gate line: ' + detail


lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
orig = {}
for mod in ('value', 'value-json'):
    orig[mod] = (S / 'source' / (mod + '.goose')).read_text()
caught = 0
missed = []
try:
    for idx, (mod, label, old, new) in enumerate(M[lo:hi], start=lo):
        src = S / 'source' / (mod + '.goose')
        if orig[mod].count(old) != 1:
            print(f'[skip] {idx} {mod} {label}: anchor count {orig[mod].count(old)}')
            continue
        src.write_text(orig[mod].replace(old, new, 1))
        rc, line = run(mod)
        ok = rc != 0
        caught += ok
        if not ok:
            missed.append(idx)
        print(f'[{"caught" if ok else "MISSED"}] {idx} {mod} {label}: rc={rc} {line}')
        src.write_text(orig[mod])
finally:
    for mod in ('value', 'value-json'):
        (S / 'source' / (mod + '.goose')).write_text(orig[mod])
        rc, line = run(mod)
        print(f'baseline {mod}: rc={rc} {line}')
print(f'mutations {caught}/{hi-lo} caught (range {lo}:{hi}) missed={missed}')
