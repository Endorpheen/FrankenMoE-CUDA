import json
import sqlite3
import sys
from pathlib import Path

def union_ns(intervals):
    start = end = count = 0
    for a,b in sorted(intervals):
        if a > end:
            count += end - start
            start = a
        end = max(end,b)
    return count + end - start

def measure(path):
    db = sqlite3.connect(f"{Path(path).resolve().as_uri()}?mode=ro&immutable=1", uri=True)
    db.execute("pragma temp_store=memory")
    result = {}
    for start, end, name in db.execute("""
        SELECT n.start,n.end,coalesce(n.text,s.value)
        FROM NVTX_EVENTS n LEFT JOIN StringIds s ON s.id=n.textId
        WHERE n.end IS NOT NULL"""):
        if not name or not any(word in name.lower() for word in ("prefill", "decode")):
            continue
        window = (start,end)
        calls = db.execute("""SELECT s.value,count(*),sum(k.end-k.start)/1e6 ms
            FROM CUPTI_ACTIVITY_KIND_RUNTIME k JOIN StringIds s ON s.id=k.nameId
            WHERE k.start>=? AND k.start<? GROUP BY s.id ORDER BY ms DESC""",window).fetchall()
        kernels = db.execute("""SELECT s.value,count(*),sum(k.end-k.start)/1e6 ms
            FROM CUPTI_ACTIVITY_KIND_KERNEL k JOIN StringIds s ON s.id=k.demangledName
            WHERE k.start>=? AND k.start<? GROUP BY s.id ORDER BY ms DESC""",window).fetchall()
        transfers = db.execute("""SELECT copyKind,count(*),sum(bytes),sum(end-start)/1e6
            FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE start>=? AND start<? GROUP BY copyKind""",window).fetchall()
        active = []
        for kind in ("KERNEL","MEMCPY"):
            active += [(max(a,start),min(b,end)) for a,b in db.execute(
                f"SELECT start,end FROM CUPTI_ACTIVITY_KIND_{kind} WHERE end>? AND start<?",window)]
        result[name] = {"wall_ms":(end-start)/1e6,"visible_gpu_union_ms":union_ns(active)/1e6,
                        "calls_cuda_api":calls,"kernels":kernels,"copies":transfers}
    return result

result = {key:measure(path) for key,path in zip(("inkling","apex"),sys.argv[1:])}
Path("trace-summary.json").write_text(json.dumps(result,indent=2)+"\n")
for model, phases in result.items():
    for phase, data in phases.items():
        print(model,phase, "wall", round(data["wall_ms"]), "gpu", round(data["visible_gpu_union_ms"]))
        print("calls",data["calls_cuda_api"][:5])
        print("kernels",data["kernels"][:5])
        print("copies",data["copies"])
