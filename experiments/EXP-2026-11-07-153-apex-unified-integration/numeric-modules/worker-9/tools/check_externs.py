#!/usr/bin/env python3
"""Check every apex_* device call has a strict prototype in the primitives header."""
import glob, re, sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
hdr = (root / 'device' / 'apex_device_primitives.cuh').read_text()
declared = set(re.findall(r'\b(apex_[a-z0-9_]+)\s*\(', hdr))
ok = True
total = 0
special = ('apex_goose_', 'apex_launch_', 'apex_cuda_')
for path in sorted(glob.glob(str(root / 'gen' / 'device' / '*.cu'))):
    text = Path(path).read_text()
    defined = set(re.findall(r'\b(apex_[a-z0-9_]+)\s*\([^;{}]*\)\s*\{', text))
    calls = set(re.findall(r'\b(apex_[a-z0-9_]+)\s*\(', text))
    for name in sorted(calls):
        total += 1
        if name in declared or name in defined or name.startswith(special):
            continue
        print(f'FAIL {path}: undeclared device call {name}')
        ok = False
print(f'EXTERN_PROTOTYPES {total} call sites checked')
print('EXTERN_PROTOTYPES_OK' if ok else 'EXTERN_PROTOTYPES_FAIL')
sys.exit(0 if ok else 1)
