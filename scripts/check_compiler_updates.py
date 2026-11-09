#!/usr/bin/env python3
"""Report whether a newer Goose stage3 compiler exists than the one this repo is pinned to.

The pin lives in runtime/compiler/current.json. Newer builds appear in the sibling directories
that host Goose compilers; this script only *reports*, it never adopts anything, because adopting
a compiler requires a fresh qualification run (four bootstrap C files, hash and byte-identical
emissions) before it can be used for the runtime work.

Usage:
    python3 scripts/check_compiler_updates.py [--adopt-sha SHA]

Exit codes: 0 = pinned compiler is newest, 1 = a newer candidate exists, 2 = error.
"""
import json
import sys
from pathlib import Path

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
PIN = F / 'runtime/compiler/current.json'
SEARCH_ROOTS = [
    Path('/home/end0/AI/LlamaServer'),
]


def pinned():
    d = json.loads(PIN.read_text())
    return Path(d['compiler']), d['sha256']


def candidates():
    """Every stage3-* binary under the search roots, newest first by mtime."""
    found = {}
    for root in SEARCH_ROOTS:
        for p in root.glob('*/compiler/stage3-*'):
            if p.is_file():
                found.setdefault(p.stat().st_mtime, []).append(p)
    out = []
    for mtime in sorted(found, reverse=True):
        for p in found[mtime]:
            out.append((mtime, p))
    return out


def main():
    cur, cur_sha = pinned()
    if not cur.exists():
        print(f'ERROR: pinned compiler missing: {cur}', file=sys.stderr)
        return 2
    print(f'pinned: {cur.name}')
    print(f'        sha256 {cur_sha[:16]}…')
    print(f'        mtime  {cur.stat().st_mtime:.0f}')

    newer = [(m, p) for m, p in candidates() if p.stat().st_mtime > cur.stat().st_mtime]
    # A different name with an older mtime is not an update; same name elsewhere is the same build.
    newer = [(m, p) for m, p in newer if p.name != cur.name]

    if not newer:
        print('no newer stage3 build found')
        return 0

    print(f'\n{len(newer)} newer build(s):')
    for mtime, p in newer:
        print(f'  {p.name}  ({p.parent.parent.name})  mtime {mtime:.0f}')
    print('\nAdopting one requires the qualification gate before any runtime work uses it:')
    print('  4 byte-identical bootstrap C files, hash check, and byte-identical emission of the')
    print('  current FA module C. Until that passes, the pinned compiler stays.')
    return 1


if __name__ == '__main__':
    sys.exit(main())
