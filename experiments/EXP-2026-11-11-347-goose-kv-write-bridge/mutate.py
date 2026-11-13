#!/usr/bin/env python3
"""Mutations for the adapter: each must be caught by the local gate."""
from pathlib import Path
import subprocess

E = Path(__file__).resolve().parent
SRC = E / 'source/bridge.c'
orig = SRC.read_text()

MUTATIONS = [
    ('a refusal is reported as success', '    const int64_t refused = n == 0 ? 1 : 0;',
     '    const int64_t refused = 0;'),
    ('cell counts are not exported', '            if (out_counts && a >= 0 && a < n_stream) out_counts[a] = (int32_t) b;', ''),
    ('ranges are exported with swapped ends',
     '                out_ranges[n_ranges*2]     = (int32_t) a;\n                out_ranges[n_ranges*2 + 1] = (int32_t) b;',
     '                out_ranges[n_ranges*2]     = (int32_t) b;\n                out_ranges[n_ranges*2 + 1] = (int32_t) a;'),
    ('the sequence mask is dropped',
     '        mask64[i] = seqmask ? (int64_t) seqmask[i] : 0;', '        mask64[i] = 0;'),
]

rows = []
for name, old, new in MUTATIONS:
    assert old in orig, ('anchor missing', name)
    SRC.write_text(orig.replace(old, new, 1))
    try:
        q = subprocess.run(['python3', str(E/'test_local.py')], capture_output=True, text=True, timeout=1800)
        caught = q.returncode != 0
        print(name, 'failures>0' if caught else 'MISSED')
        rows.append(caught)
    finally:
        SRC.write_text(orig)

subprocess.run(['python3', str(E/'test_local.py')], capture_output=True)
print('ALL_MUTATIONS_CAUGHT' if all(rows) else 'MISSED', len(rows))
raise SystemExit(0 if all(rows) else 1)
