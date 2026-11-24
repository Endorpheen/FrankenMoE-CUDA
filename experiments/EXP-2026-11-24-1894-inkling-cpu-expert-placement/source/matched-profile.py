import json
import sqlite3
from pathlib import Path


def connect(path):
    db = sqlite3.connect(f"{Path(path).resolve().as_uri()}?mode=ro&immutable=1", uri=True)
    db.execute("pragma temp_store=memory")
    return db


def union_ns(intervals):
    length = start = finish = 0
    for a, b in sorted(intervals):
        if a > finish:
            length += finish - start
            start = a
        finish = max(finish, b)
    return length + finish - start


def summarize(db, windows):
    kernels = """SELECT s.value, count(*), sum(k.end-k.start)/1e6 t
                  FROM CUPTI_ACTIVITY_KIND_KERNEL k JOIN StringIds s ON s.id=k.demangledName
                  WHERE k.start>=? AND k.start<? GROUP BY s.id ORDER BY t DESC"""
    calls = """SELECT s.value,count(*),sum(k.end-k.start)/1e6 t
             FROM CUPTI_ACTIVITY_KIND_RUNTIME k JOIN StringIds s ON s.id=k.nameId
             WHERE k.start>=? AND k.start<? GROUP BY s.id ORDER BY t DESC"""
    copies = """SELECT copyKind,count(*),sum(bytes),sum(end-start)/1e6
              FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE start>=? AND start<? GROUP BY copyKind"""
    result = {}
    for label, (start, end) in windows.items():
        active = []
        available = {r[0] for r in db.execute("select name from sqlite_master")}
        tables = ('KERNEL', 'MEMCPY', 'GRAPH_TRACE')
        graph = {}
        for table in tables:
            if f"CUPTI_ACTIVITY_KIND_{table}" not in available:
                continue
            if table == 'GRAPH_TRACE':
                graph["cuda_graph_intervals"] = db.execute("select count(*),sum(end-start)/1e6 from CUPTI_ACTIVITY_KIND_GRAPH_TRACE where start>=? and start<?",(start,end)).fetchone()
            active.extend((max(a,start),min(b,end)) for a,b in db.execute(
              f"SELECT start,end FROM CUPTI_ACTIVITY_KIND_{table} WHERE end>? AND start<?", (start,end)))
        result[label] = {"window_ms":(end-start)/1e6,"gpu_union_ms":union_ns(active)/1e6,
            "calls":db.execute(calls,(start,end)).fetchall(), "kernels":db.execute(kernels,(start,end)).fetchall(),
            "copies":db.execute(copies,(start,end)).fetchall(),**graph}
    return result


p = Path("upstream-matched/profile-near")
upstream = connect(p/"upstream.sqlite")
epoch = upstream.execute("select * from TARGET_INFO_SESSION_START_TIME").fetchone()[0]
start = int((p/"request-start-ns.txt").read_text()) - epoch
end = int((p/"request-end-ns.txt").read_text()) - epoch
timings = json.loads((p/"profile-measure.json").read_text())["timings"]
pivot = start + round(timings["prompt_ms"] * 1e6)
assert abs((end-start)/1e6-timings["prompt_ms"]-timings["predicted_ms"]) < 150
goose = connect("goose-profile/inkling.sqlite")
g = {name.removeprefix("inkling-"):(a,b) for a,b,name in goose.execute(
  "select start,end,text from NVTX_EVENTS where text in ('inkling-prefill','inkling-decode')")}
g["whole"] = (g["prefill"][0],g["decode"][1])
out = {"upstream":summarize(upstream,{"whole":(start,end),"prefill_approx":(start,pivot),"decode_approx":(pivot,end)}),
       "goose":summarize(goose,g)}
Path("matched-profile.json").write_text(json.dumps(out,indent=2)+"\n")
for engine, phases in out.items():
    for phase, data in phases.items():
        t=data["kernels"]
        print(engine,phase,round(data["window_ms"]),round(data["gpu_union_ms"]))
        print("copies",data["copies"])
        print("API",data["calls"][:7])
        print("kernels",[(k[:82],n,round(ms,2)) for k,n,ms in t[:7]])
