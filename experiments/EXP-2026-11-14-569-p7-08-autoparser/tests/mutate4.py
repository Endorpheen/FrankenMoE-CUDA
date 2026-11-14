#!/usr/bin/env python3
# Mutation harness for slice 4. Six mutations in the JSON-layout half
# (function_is_key, nested keys, per-call) and six in the tagged half
# (args wrap, optional-only arguments, close/peek).
from pathlib import Path
import subprocess
import sys

ROOT = Path('/tmp/opencode/sub-569')
SRC = ROOT / 'base/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('json', 'function_is_key drops the arguments',
     '        let inner = argsp;\n        var body = ap_seq4',
     '        let inner = ap_eps(kind, cfirst, ccount, sidx, st);\n        var body = ap_seq4'),
    ('json', 'nested field uses the name key',
     '            ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, nested_prefix),\n            ap_space(kind, cfirst, ccount, sidx, st),\n            ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, ":"),\n            ap_space(kind, cfirst, ccount, sidx, st), nobj);',
     '            ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, nested_name),\n            ap_space(kind, cfirst, ccount, sidx, st),\n            ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, ":"),\n            ap_space(kind, cfirst, ccount, sidx, st), nobj);'),
    ('json', 'per-call wrapper treated as a section',
     '    if per_call_mode != 0 {',
     '    if per_call_mode == 2 {'),
    ('json', 'per-call repetition requires two calls',
     'ap_seq2(kind, cfirst, ccount, children, sidx, st, one,\n                    ap_space(kind, cfirst, ccount, sidx, st)), 1, 0 - 1));',
     'ap_seq2(kind, cfirst, ccount, children, sidx, st, one,\n                    ap_space(kind, cfirst, ccount, sidx, st)), 2, 0 - 1));'),
    ('json', 'nested object loses the space after the name',
     '        k8[2] = nname;\n        k8[3] = ap_space(kind, cfirst, ccount, sidx, st);',
     '        k8[2] = nname;\n        k8[3] = ap_eps(kind, cfirst, ccount, sidx, st);'),
    ('json', 'function_is_key always wraps arguments',
     '        if args_key.len > 0 {',
     '        if args_key.len >= 0 {'),
    ('tagged', 'arguments.start is not emitted',
     '                ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, args_start), args_seq);',
     '                ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, ""), args_seq);'),
    ('tagged', 'optional arguments are dropped',
     '        if has_opt != 0 {',
     '        if has_opt == 2 {'),
    ('tagged', 'arguments.end is not emitted',
     '            args_seq = ap_seq2(kind, cfirst, ccount, children, sidx, st, args_seq,\n                ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, args_end));',
     '            args_seq = ap_seq2(kind, cfirst, ccount, children, sidx, st, args_seq,\n                ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, ""));'),
    ('tagged', 'atomic peek inverted',
     '        if args_name_prefix.len > 0 && has_req != 0 {',
     '        if args_name_prefix.len > 0 && has_req == 0 {'),
    ('tagged', 'function close ignored',
     '    if func_close.len > 0 {',
     '    if func_close.len > 99999 {'),
    ('tagged', 'required argument separator removed',
     '                if has_req != 0 {\n                    req = ap_seq2(kind, cfirst, ccount, children, sidx, st, req,\n                        ap_space(kind, cfirst, ccount, sidx, st));',
     '                if has_req != 0 {\n                    req = ap_seq2(kind, cfirst, ccount, children, sidx, st, req,\n                        ap_eps(kind, cfirst, ccount, sidx, st));'),
]


def run():
    caught = 0
    rows = []
    try:
        for i, (half, name, old, new) in enumerate(M, 1):
            n = ORIG.count(old)
            if n != 1:
                rows.append((i, half, name, f'PATTERN COUNT {n}'))
                continue
            SRC.write_text(ORIG.replace(old, new, 1))
            try:
                r = subprocess.run(['bash', str(ROOT / 'build_gate4.sh')], text=True,
                                   capture_output=True, timeout=240, errors='replace')
            except subprocess.TimeoutExpired:
                rows.append((i, half, name, 'timeout'))
                caught += 1
                continue
            ok = ('FG_DRAFT4 cases=29 values=29 failures=0' in r.stdout and r.returncode == 0)
            rows.append((i, half, name, 'NOT CAUGHT' if ok else 'caught'))
            if not ok:
                caught += 1
    finally:
        SRC.write_text(ORIG)

    for i, half, name, res in rows:
        print(f'{i:2d} {half:6s} {name:52s} {res}')
    print(f'FG_MUT4 caught={caught}/{len(M)}')
    return 0 if caught == len(M) else 1


if __name__ == '__main__':
    sys.exit(run())
