"""Attribute staged FP32 attention at 32768 KV rows in the component trace."""
import json
import sqlite3
import sys

db = sqlite3.connect(f'file:{sys.argv[1]}?mode=ro', uri=True)
rows = db.execute('''
  SELECT k.start,k.end,k.gridX,k.gridY,s.value
  FROM CUPTI_ACTIVITY_KIND_KERNEL AS k JOIN StringIds AS s ON k.shortName=s.id
  ORDER BY k.start
''').fetchall()
expected = {'bonsai_flash_stage_kv','bonsai_flash_staged_tile','bonsai_flash_f32_merge'}
out = []
for i, (first, _, gx, gy, mode) in enumerate(rows):
    if (mode,gx,gy) != ('bonsai_flash_stage_kv',1024,4):
        continue
    totals = {}
    stop = first
    for a,b,_,_,kind in rows[i:]:
        if kind not in expected or kind == 'bonsai_flash_stage_kv' and a != first:
            break
        r = totals.setdefault(kind, {'calls':0,'duration_ns':0})
        r['calls'] += 1
        r['duration_ns'] += b-a
        stop = b
    assert set(totals)==expected
    out.append({'span_ns':stop-first,'kernels':totals})
assert out
print(json.dumps(out,indent=2))
