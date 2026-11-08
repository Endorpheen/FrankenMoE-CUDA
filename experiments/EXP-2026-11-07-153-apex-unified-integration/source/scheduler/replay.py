#!/usr/bin/env python3
"""Replay the isolated scheduler overlay only onto its exact audited base."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--base',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
s=Path(__file__).resolve().parent;expected=json.loads((s/'base.json').read_text())['sha256']
actual=hashlib.sha256(a.base.read_bytes()).hexdigest()
if actual!=expected:raise SystemExit('Base hash mismatch: expected '+expected+', got '+actual)
text=(s/'backend.cpp').read_text().replace('#include "bridge.h"','#include "'+str(s/'bridge.h')+'"').replace('#include "services.cpp"','#include "'+str(s/'services.cpp')+'"')
a.output.write_text(text)
print(json.dumps({'output':str(a.output),'sha256':hashlib.sha256(text.encode()).hexdigest(),'base_sha256':actual}))
