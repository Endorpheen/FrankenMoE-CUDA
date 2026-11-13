#!/usr/bin/env python3
"""Mutation gate for the route table (EXP449).

The table is generated from the native source, so the mutations act on the
generator's Goose template: the shared path/category mapping stays intact and
only the emitted Goose module diverges from the expected table.
"""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-449-p8-02-route-table'
SRC = E / 'gen_routes.py'
ORIG = SRC.read_text()

MUTATIONS = [
    ('emitted category off by one', "{{return {cat} as i64;}}", "{{return {cat + 1} as i64;}}"),
    ('method shifted to the next one', 'if m=={method} as i64 && p==', 'if m=={method + 1} as i64 && p=='),
    ('slots prefix points at the wrong route', 'if starts_with(p, "/slots/") {', 'if starts_with(p, "/slot/") {'),
    ('slots rule matches nested paths', "if find(p[7..], \"/\") < 0 as i64", "if find(p[7..], \"/\") >= 0 as i64"),
    ('health loses the public exemption', "        ' if cat==1 as i64 {return 0 as i64;}',",
     "        ' if cat==99 as i64 {return 0 as i64;}',"),
    ('one route is not emitted',
     "        lines.append(f' if m=={method} as i64 && p==\"{path}\"",
     "        if path == \"/metrics\": continue\n        lines.append(f' if m=={method} as i64 && p==\"{path}\""),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG449_ROUTES')]
    err = [x for x in (r.stdout + r.stderr).splitlines() if 'FAIL' in x or 'MISMATCH' in x][:1]
    if not line and r.returncode != 0:
        return None, 'build failed'
    return r.returncode, (line[-1] if line else (err[0] if err else ''))


if __name__ == '__main__':
    caught = 0
    try:
        for label, old, new in MUTATIONS:
            if old not in ORIG:
                print(f'[skip] {label}: anchor missing')
                continue
            SRC.write_text(ORIG.replace(old, new, 1))
            rc, line = run()
            if rc is None:
                print(f'[build-fail] {label}')
                continue
            ok = rc != 0
            caught += ok
            print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
    finally:
        SRC.write_text(ORIG)
        run()
    print(f'mutations {caught}/{len(MUTATIONS)} caught')
