#!/usr/bin/env python3
# Mutation harness for slice 5. Six mutations in the id/layout half and six in
# the grammar/response half. Continuation has no port logic to mutate: it is
# proven by the oracle equality of cases 1 and 30.
from pathlib import Path
import subprocess
import sys

ROOT = Path('/tmp/opencode/sub-569')
SRC = ROOT / 'base/runtime.goose'
ORIG = SRC.read_text()

M = [
    ('ids', 'function_is_key id/gen braces dropped',
     '        if nf > 1 || args_key.len > 0 {',
     '        if nf > 99 || args_key.len > 0 {'),
    ('ids', 'function_is_key arguments dropped',
     '        var inner = argsp;',
     '        var inner = ap_eps(kind, cfirst, ccount, sidx, st);'),
    ('ids', 'function_is_key call-id field dropped',
     '            fields[nf] = ap_rep(kind, cfirst, ccount, children, sidx, mn, mx, st,\n                ap_seq3(kind, cfirst, ccount, children, sidx, st, idp,',
     '            fields[nf] = ap_rep(kind, cfirst, ccount, children, sidx, mn, mx, st,\n                ap_seq3(kind, cfirst, ccount, children, sidx, st, ap_eps(kind, cfirst, ccount, sidx, st),'),
    ('ids', 'nested call-id field dropped',
     '            k4[0] = idp;',
     '            k4[0] = ap_eps(kind, cfirst, ccount, sidx, st);'),
    ('ids', 'nested generated-id field dropped',
     '            k4[0] = genp;',
     '            k4[0] = ap_eps(kind, cfirst, ccount, sidx, st);'),
    ('ids', 'nested field uses the name key',
     '            ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, nested_prefix),',
     '            ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, nested_name),'),
    ('grammar', 'auto tools never include grammar',
     '        if tool_choice == 0 && trigger_marker.len > 0 {',
     '        if tool_choice == 0 && trigger_marker.len > 99999 {'),
    ('grammar', 'required tools never include grammar',
     '        if tool_choice == 1 {',
     '        if tool_choice == 3 {'),
    ('grammar', 'lazy flag wrong',
     '    if has_response_format == 0 && tool_choice == 0 {',
     '    if has_response_format == 0 && tool_choice == 2 {'),
    ('grammar', 'OpenAI wrapper trigger dropped',
     '        if openai_wrapper != 0 {',
     '        if openai_wrapper == 2 {'),
    ('grammar', 'marker bytes not copied',
     '            trig[i] = trigger_marker[i];',
     '            trig[i] = 0;'),
    ('grammar', 'response fence renamed',
     'ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, "```json"),',
     'ap_lit(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, "```jsonx"),'),
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
                r = subprocess.run(['bash', str(ROOT / 'build_gate5.sh')], text=True,
                                   capture_output=True, timeout=240, errors='replace')
            except subprocess.TimeoutExpired:
                rows.append((i, half, name, 'timeout'))
                caught += 1
                continue
            ok = ('FG_DRAFT5 cases=36 values=36 failures=0' in r.stdout
                  and 'FG_GRAMMAR cases=5 failures=0' in r.stdout and r.returncode == 0)
            rows.append((i, half, name, 'NOT CAUGHT' if ok else 'caught'))
            if not ok:
                caught += 1
    finally:
        SRC.write_text(ORIG)

    for i, half, name, res in rows:
        print(f'{i:2d} {half:7s} {name:48s} {res}')
    print(f'FG_MUT5 caught={caught}/{len(M)}')
    return 0 if caught == len(M) else 1


if __name__ == '__main__':
    sys.exit(run())
