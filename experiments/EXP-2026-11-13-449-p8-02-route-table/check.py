#!/usr/bin/env python3
"""Cross-check the Goose route table against the native registrations.

The fixture classifies every native route through the compiled Goose module and
writes "METHOD<TAB>path<TAB>category" to routes.txt. This script re-parses
tools/server/server.cpp, compares the committed expected.json with the current
source, and then compares the compiled Goose classification with the source.
"""
import json
import sys
from pathlib import Path

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-449-p8-02-route-table'
sys.path.insert(0, str(E))
import gen_routes  # noqa: E402

NAME = {1: 'GET', 2: 'POST', 3: 'DELETE', 4: 'WS'}


def main():
    routes, dynamic = gen_routes.parse_routes()
    expected = json.loads((E / 'expected.json').read_text())
    committed = {(r['method'], r['path']): r['category'] for r in expected['routes']}

    if set(committed) != set(routes):
        print('ROUTE SET DRIFT vs server.cpp:', sorted(set(routes) ^ set(committed))[:5])
        return 1
    for (m, p), cat in routes.items():
        if committed[(m, p)] != cat:
            print(f'CATEGORY DRIFT {m} {p}: {committed[(m,p)]} vs {cat}')
            return 1

    got = {}
    for line in (F / 'runtime/exp449/build/routes.txt').read_text().splitlines():
        method, path, cat = line.split('\t')
        got[(method, path)] = int(cat)
    for (m, p), cat in routes.items():
        if got.get((NAME[m], p)) != cat:
            print(f'ROUTE MISMATCH {NAME[m]} {p}: goose={got.get((NAME[m], p))} native={cat}')
            return 1

    if dynamic != expected['dynamic']:
        print('DYNAMIC ROUTE LIST DRIFT')
        return 1

    print(f'FG449_ROUTES_CHECK PASS routes={len(routes)} categories_match_source=yes')
    return 0


if __name__ == '__main__':
    sys.exit(main())
