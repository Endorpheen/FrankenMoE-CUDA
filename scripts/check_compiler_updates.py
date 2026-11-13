#!/usr/bin/env python3
"""Report which Goose compiler build is the best qualified candidate to run against.

Authoritative catalogue:
    /home/end0/AI/Fried-Goose/.cache/selfhosted-builds/catalog.json

Policy (simplified 2026-11-09): the channel name does not decide eligibility. Any build the
catalogue marks as qualified is a candidate, including `selfhosted_subset`. What decides is
whether it actually works for this runtime, and that is settled by the existing checks -- not by a
label:

  1. sha256 in the catalogue must match the binary on disk.
  2. The repo's existing gates must pass (the per-experiment local gates and one Apex correctness
     run: tokens, text and all 128 top-5 probabilities equal).
  3. If correctness holds, one standard A/B -> B/A performance pair decides whether it is kept.

No extra mass compiler test suite is run, and no check already done is repeated.

Exit codes: 0 = nothing new to try, pinned compiler stays; 1 = a candidate exists and should be
tried through the existing checks; 2 = catalogue or pin unreadable, or a sha256 mismatch.
"""
import hashlib
import json
import sys
from pathlib import Path

CATALOG = Path('/home/end0/AI/Fried-Goose/.cache/selfhosted-builds/catalog.json')
PIN = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/compiler/current.json')


def sha256(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    try:
        cat = json.loads(CATALOG.read_text())
        pin = json.loads(PIN.read_text())
    except Exception as e:
        print(f'ERROR: {e}', file=sys.stderr)
        return 2

    pinned_path = Path(pin['compiler'])
    pinned_sha = pin['sha256']
    print(f'pinned:   {pinned_path.name}')
    print(f'          sha256 {pinned_sha[:16]}…')
    if pinned_path.exists():
        ok = sha256(pinned_path) == pinned_sha
        print(f'          on disk {"OK" if ok else "MISMATCH"}')
    else:
        print('          MISSING on disk')

    # Every qualified build is a candidate; the channel name is not a filter.
    cands = []
    for name, ch in (cat.get('channels') or {}).items():
        cur = (ch or {}).get('current')
        if not cur:
            continue
        cands.append((name, (ch or {}).get('status'), cur))

    if not cands:
        print('\nno qualified build in any channel; the pin stays.')
        return 0

    new = []
    for name, status, cur in cands:
        p = Path(cur.get('compiler', ''))
        want = cur.get('sha256', '')
        if not p.exists():
            print(f'\n{name}: {cur.get("version")} — binary missing, skipped')
            continue
        got = sha256(p)
        if got != want:
            print(f'\n{name}: {cur.get("version")} — sha256 MISMATCH, skipped')
            continue
        if got == pinned_sha:
            print(f'\n{name}: {cur.get("version")} — already the pin')
            continue
        new.append((name, status, cur, p, got))
        print(f'\n{name}: {cur.get("version")}  status={status}')
        print(f'  sha256 {got[:16]}… verified')
        print(f'  path   {p}')

    if not new:
        print('\nnothing new to try; the pin stays.')
        return 0

    print('\nCandidate(s) above should go through the existing checks, in this order:')
    print('  1. repo gates already in place (per-experiment local gates)')
    print('  2. one Apex correctness run: tokens, text, all 128 top-5 probabilities')
    print('  3. one A/B -> B/A performance pair')
    print('On success move runtime/compiler/current.json and keep the old path as rollback.')
    return 1


if __name__ == '__main__':
    sys.exit(main())
