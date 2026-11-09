#!/usr/bin/env python3
"""Report whether a runtime-approved Goose compiler exists that differs from the pinned one.

Authoritative source for new builds:
    /home/end0/AI/Fried-Goose/.cache/selfhosted-builds/catalog.json

Rules, taken from the catalog's own publication_rule and from Igor:

  * Only `channels.llm_runtime.current` qualifies for LLM runtime work.
  * It must be non-empty, have `runtime_approved: true`, and its `sha256` must match the binary
    before anything uses it.
  * `channels.selfhosted_subset` is a *supported-subset* build, not a runtime build. A newer
    mtime or a new commit does not make it usable for the runtime.
  * Version directories under `versions/` are retained for rollback; the previous pin stays
    recorded when a new one is adopted.

This script only reports. Adopting a compiler also requires the repo's own qualification gate
(byte-identical bootstrap C, hash match, byte-identical emission of the current FA module) and a
correctness + performance run before the pin moves.

Exit codes: 0 = no usable new build, pinned compiler stays; 1 = a runtime-approved build exists
and differs from the pin; 2 = error reading the catalog or the pin.
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
        actual = sha256(pinned_path)
        print(f'          on disk {actual[:16]}…  {"OK" if actual == pinned_sha else "MISMATCH"}')
    else:
        print('          MISSING on disk')

    ch = cat.get('channels', {})
    lr = ch.get('llm_runtime') or {}
    status = lr.get('status')
    cur = lr.get('current')
    print(f'\nllm_runtime: status={status} current={"set" if cur else "null"}')

    if not cur:
        print('runtime channel is empty; the pinned compiler stays.')
        sub = (ch.get('selfhosted_subset') or {}).get('current') or {}
        if sub:
            print(f'note: subset build {sub.get("version")} exists but is not runtime-approved —')
            print('      it covers a supported language subset only and must not be adopted.')
        return 0

    if not cur.get('runtime_approved'):
        print('current entry is NOT runtime_approved; it must not be used for runtime work.')
        return 0

    cand = Path(cur['compiler'])
    want = cur['sha256']
    if not cand.exists():
        print(f'ERROR: runtime_approved entry points at a missing file: {cand}', file=sys.stderr)
        return 2
    got = sha256(cand)
    if got != want:
        print(f'ERROR: sha256 mismatch for {cand.name}: catalog {want[:16]}… disk {got[:16]}…',
              file=sys.stderr)
        return 2

    if got == pinned_sha:
        print('runtime-approved build is the one already pinned.')
        return 0

    print(f'\nNEW runtime-approved build: {cur.get("version")}  sha256 {want[:16]}…')
    print('Before adopting: run the repo qualification gate, then correctness and performance')
    print('checks, then move runtime/compiler/current.json and keep the old pin as rollback.')
    return 1


if __name__ == '__main__':
    sys.exit(main())
