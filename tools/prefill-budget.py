#!/usr/bin/env python3
"""Prefill budget from an nsys sqlite export.

Window: from the first MMQ kernel (prompt pass) to the first 32-byte read-back,
which is where the decode loop starts. Everything printed is measured on the GPU
timeline; the expert staging appears as HtoD transfers of at least 128 KiB.

usage: prefill-budget.py <prof.sqlite>
"""
import bisect
import collections
import sqlite3
import sys

db = sqlite3.connect(sys.argv[1])
q = lambda s, *a: db.execute(s, a).fetchall()

start = q("select min(k.start) from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on s.id=k.shortName "
          "where s.value like 'goose_mmq%'")
lo = start[0][0] if start else None
if lo is None:
    print("no MMQ kernel found: this trace has no prefill")
    sys.exit(1)
end = q("select min(start) from CUPTI_ACTIVITY_KIND_MEMCPY where copyKind=2 and bytes=32 and start>?", lo)
hi = end[0][0] if end and end[0][0] else q("select max(end) from CUPTI_ACTIVITY_KIND_KERNEL")[0][0]
win = hi - lo

def intervals(table):
    return [(max(s, lo), min(e, hi)) for s, e in
            q(f"select start,end from {table} where end>{lo} and start<{hi}")]

def union(iv):
    if not iv:
        return 0
    iv.sort()
    total, cs, ce = 0, iv[0][0], iv[0][1]
    for a, b in iv[1:]:
        if a > ce:
            total += ce - cs
            cs, ce = a, b
        elif b > ce:
            ce = b
    return total + ce - cs

def gaps(iv):
    if not iv:
        return []
    iv.sort()
    out, cs, ce = [], iv[0][0], iv[0][1]
    for a, b in iv[1:]:
        if a > ce:
            out.append((ce, a))
            cs, ce = a, b
        elif b > ce:
            ce = b
    return out

kern, cpy = intervals("CUPTI_ACTIVITY_KIND_KERNEL"), intervals("CUPTI_ACTIVITY_KIND_MEMCPY")
kb, cb, ub = union(kern), union(cpy), union(kern + cpy)
print(f"prefill window {win/1e9:.3f} s")
print(f"  kernels        {kb/1e9:8.3f} s ({100*kb/win:5.1f}%)")
print(f"  memcpys        {cb/1e9:8.3f} s ({100*cb/win:5.1f}%)  sum, overlaps possible")
print(f"  union          {ub/1e9:8.3f} s ({100*ub/win:5.1f}%)")
print(f"  GPU idle       {(win-ub)/1e9:8.3f} s ({100*(win-ub)/win:5.1f}%)")

htod = q("select bytes, count(*), sum(end-start) from CUPTI_ACTIVITY_KIND_MEMCPY "
         "where copyKind=1 and start>=? and end<=? and bytes>=131072 group by (bytes>=1048576)", lo, hi)
big = q("select sum(bytes), sum(end-start) from CUPTI_ACTIVITY_KIND_MEMCPY "
        "where copyKind=1 and start>=? and end<=? and bytes>=131072", lo, hi)[0]
allh = q("select sum(bytes) from CUPTI_ACTIVITY_KIND_MEMCPY where copyKind=1 and start>=? and end<=?", lo, hi)[0][0] or 0
if big and big[0]:
    print(f"  staging HtoD   {big[0]/1e9:8.3f} GB in {big[1]/1e9:.3f} s of transfer "
          f"(all HtoD {allh/1e9:.3f} GB)")

print("\ntop kernels by time")
rows = q("""select s.value, count(*), sum(k.end-k.start)/1e6 from CUPTI_ACTIVITY_KIND_KERNEL k
            join StringIds s on s.id=k.shortName where k.start>=? and k.end<=?
            group by s.value order by 3 desc limit 12""", lo, hi)
for nm, n, ms in rows:
    print(f"  {nm[:42]:42s} {n:8d} {ms:9.1f} ms {100*ms/(kb/1e6):5.1f}%")

tid = q("select globalTid,count(*) c from CUPTI_ACTIVITY_KIND_RUNTIME where start>=? and end<=? "
        "group by globalTid order by c desc limit 1", lo, hi)[0][0]
api = q("select r.start,r.end,s.value from CUPTI_ACTIVITY_KIND_RUNTIME r join StringIds s on s.id=r.nameId "
        "where r.globalTid=? and r.end>? and r.start<? order by r.start", tid, lo, hi)
starts = [a[0] for a in api]
att, cnt = collections.Counter(), collections.Counter()
for s, e in (g for g in gaps(kern + cpy) if g[1] - g[0] > 50000):
    j = max(0, bisect.bisect_right(starts, s) - 1)
    best, ov = None, 0
    while j < len(api) and api[j][0] < e:
        a, b, nm = api[j]
        o = min(b, e) - max(a, s)
        if o > ov:
            ov, best = o, nm
        j += 1
    att[best or "<none>"] += e - s
    cnt[best or "<none>"] += 1
print("\nidle gaps > 50 us, attributed to the call in flight")
print(f"  {'call':34s} {'s':>8s} {'gaps':>7s}")
for nm, ns in att.most_common(8):
    print(f"  {nm:34s} {ns/1e9:8.3f} {cnt[nm]:7d}")

print("\nsubmission")
for nm in ("cuLaunchKernel", "cuGraphLaunch", "cuMemcpyHtoDAsync_v2", "cuMemcpyHtoD_v2",
           "cuMemcpyBatchAsync_v2", "cuEventRecord", "cuCtxSynchronize"):
    n, tot = q("select count(*), coalesce(sum(r.end-r.start),0) from CUPTI_ACTIVITY_KIND_RUNTIME r "
               "join StringIds s on s.id=r.nameId where s.value=? and r.start>=? and r.end<=?",
               nm, lo, hi)[0]
    if n:
        print(f"  {nm:26s} calls {n:8d}  host {tot/1e6:8.1f} ms")
