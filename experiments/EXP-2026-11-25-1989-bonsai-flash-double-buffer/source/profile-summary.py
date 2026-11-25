"""Partition the 4097/16 CUDA trace at the first vocabulary projection."""
import json
import sqlite3
import sys

db = sqlite3.connect(f'file:{sys.argv[1]}?mode=ro', uri=True)
rows = db.execute('''
    SELECT k.start, k.end, k.gridX, s.value
    FROM CUPTI_ACTIVITY_KIND_KERNEL k JOIN StringIds s ON s.id=k.shortName
    ORDER BY k.start
''').fetchall()
assert rows[0][3] == 'bonsai_embed_batch'
boundary = next(i for i, row in enumerate(rows) if row[2:] == (62080, 'bonsai_pq2_matvec_ordered'))
phases = {}
for phase, kernels in [('prefill', rows[:boundary+1]), ('decode', rows[boundary+1:])]:
    start, end = kernels[0][0], kernels[-1][1]
    by_kernel = {}
    for a, b, _, name in kernels:
        entry = by_kernel.setdefault(name, {'ns': 0, 'calls': 0})
        entry['ns'] += b - a
        entry['calls'] += 1
    copies = db.execute('''
        SELECT copyKind,count(*),sum(bytes),sum(end-start)
        FROM CUPTI_ACTIVITY_KIND_MEMCPY
        WHERE start >= ? AND end <= ? GROUP BY copyKind
    ''', (start, end)).fetchall()
    phases[phase] = {
        'gpu_span_ns': end-start, 'kernel_ns': sum(v['ns'] for v in by_kernel.values()),
        'kernels': dict(sorted(by_kernel.items(), key=lambda row: -row[1]['ns'])),
        'copies': [dict(kind=k, calls=n, bytes=size, ns=t) for k, n, size, t in copies],
    }
print(json.dumps(phases, indent=2))
