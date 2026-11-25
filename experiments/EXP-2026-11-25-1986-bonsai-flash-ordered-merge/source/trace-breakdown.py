"""Attribute the same stages for both merge modes at 32768 KV rows."""
import json
import sqlite3
import sys

db = sqlite3.connect(f'file:{sys.argv[1]}?mode=ro', uri=True)
rows = db.execute('''
  SELECT k.start,k.end,k.gridX,k.gridY,s.value
  FROM CUPTI_ACTIVITY_KIND_KERNEL AS k JOIN StringIds AS s ON k.shortName=s.id
  ORDER BY k.start
''').fetchall()
stage = 'bonsai_flash_stage_kv'
tile = 'bonsai_flash_staged_tile'
merge = ('bonsai_flash_f32_merge','bonsai_flash_ordered_merge')
out = {'control': [], 'ordered': []}
for i, (first, _, gx, gy, mode) in enumerate(rows):
    if (mode, gx, gy) != (stage, 1024, 4):
        continue
    totals = {}
    stop = first
    for a,b,_,_,kind in rows[i:]:
        if kind not in (stage,tile,*merge) or kind == stage and a != first:
            break
        totals[kind] = totals.get(kind,0) + b-a
        stop = b
    mode = 'ordered' if merge[1] in totals else 'control'
    assert len(totals)==3
    out[mode].append(dict(span_ns=stop-first, kernel_ns=totals))
assert len(out['control'])==len(out['ordered'])==3
print(json.dumps(out,indent=2))
