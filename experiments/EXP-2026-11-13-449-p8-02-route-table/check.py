#!/usr/bin/env python3
"""Cross-check the Goose route keys against an independent Python FNV-1a and
verify that the committed table still matches tools/server/server.cpp."""
import json
import sys
from pathlib import Path

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-449-p8-02-route-table'
sys.path.insert(0, str(E))
import gen_routes  # noqa: E402


def main():
    routes, dynamic = gen_routes.parse_routes()
    expected = json.loads((E / 'expected.json').read_text())
    committed = {(r['method'], r['path']): (r['category'], r['key']) for r in expected['routes']}
    if set(committed) != set(routes):
        missing = set(routes) ^ set(committed)
        print('ROUTE SET DRIFT vs server.cpp:', sorted(missing)[:5])
        return 1
    for (m, p), cat in routes.items():
        if committed[(m, p)][0] != cat:
            print(f'CATEGORY DRIFT {m} {p}: {committed[(m,p)][0]} vs {cat}')
            return 1

    # independent check of the compiled Goose keys
    got = {}
    for line in (E.parent.parent / 'runtime/exp449/build/keys.txt').read_text().splitlines():
        method, path, key, cat = line.split()
        got[(method, path)] = (int(key, 16), int(cat))
    name = {1: 'GET', 2: 'POST', 3: 'DELETE', 4: 'WS'}
    for (m, p), cat in routes.items():
        if (name[m], p) not in got:
            print('MISSING Goose key for', name[m], p)
            return 1
        key, gcat = got[(name[m], p)]
        want = gen_routes.fnv(m, p)
        if key != want:
            print(f'KEY MISMATCH {name[m]} {p}: goose={key:016x} py={want:016x}')
            return 1
        if gcat != cat:
            print(f'CATEGORY MISMATCH {name[m]} {p}: goose={gcat} native={cat}')
            return 1
    if dynamic != expected['dynamic']:
        print('DYNAMIC ROUTE LIST DRIFT')
        return 1
    print(f'FG449_ROUTES_CHECK PASS routes={len(routes)} keys_match_python=yes')
    return 0


if __name__ == '__main__':
    sys.exit(main())
