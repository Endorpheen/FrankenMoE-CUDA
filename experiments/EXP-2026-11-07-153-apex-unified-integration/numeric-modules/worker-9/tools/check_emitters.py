#!/usr/bin/env python3
"""Validate that the mechanical adapter bound each generated entry correctly."""
import glob, json, sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
ok = True
rows = []
for path in sorted(glob.glob(str(root / 'gen' / '*.emit.json'))):
    data = json.loads(Path(path).read_text())
    for mode in ('device', 'host'):
        e = data[mode]
        main, kernel, keep = e['main'], e['kernel'], e['keep']
        if main == kernel:
            print(f'FAIL {path} {mode}: main == kernel'); ok = False
        if kernel not in keep:
            print(f'FAIL {path} {mode}: kernel missing from closure'); ok = False
        rows.append((Path(path).stem, mode, main, kernel, len(keep)))
for r in rows:
    print('EMITTER %s %-6s main=%s kernel=%s closure=%d' % r)
print('EMITTER_IDS_OK' if ok else 'EMITTER_IDS_FAIL')
sys.exit(0 if ok else 1)
