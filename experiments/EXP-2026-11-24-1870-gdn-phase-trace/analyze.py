import csv
from collections import defaultdict
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
from statistics import median
import sqlite3
import sys

def union(intervals):
    duration = 0
    end = 0
    for start, stop in sorted(intervals):
        if stop > end:
            duration += stop - max(start, end)
            end = stop
    return duration

def ms(ns):
    return ns / 1e6

def grouped(rows):
    groups = defaultdict(lambda: [0, 0, 0])
    for start, end, name, size in rows:
        values = groups[name]
        values[0] += 1
        values[1] += end - start
        values[2] += size
    return {name: {"count": n, "sum_ms": ms(duration), "bytes": size} for name, (n, duration, size) in groups.items()}

def report(path, expected):
    db = sqlite3.connect(f"{(path / 'prof.sqlite').as_uri()}?mode=ro", uri=True)
    markers = dict((name, (start, end)) for name, start, end in db.execute(
        "SELECT s.value,n.start,n.end FROM NVTX_EVENTS n JOIN StringIds s ON s.id=n.textId WHERE n.end IS NOT NULL"))
    assert {"apex-request", "apex-prefill", "apex-decode"} <= markers.keys()
    logs = (path/"server.log").read_text()
    assert "steps=511" in logs
    wall = {label: float(t) for label,t in re.findall(r"Goose phase (\w+) (\d+\.\d+)", logs)}
    sensors = list(csv.reader((path/"gpu-2-samples.csv").read_text().splitlines()))
    output = {"run":path.name,"kernel_selected":"SM89 full-block GDN V32 selected" in logs,"phases":{}}
    for phase, stop in (("prefill","decode"),("decode","end")):
        a,b=markers["apex-"+phase]
        clock_rows = [row for row in sensors if wall[phase] <= datetime.strptime(row[0].strip(),"%Y/%m/%d %H:%M:%S.%f").timestamp() < wall[stop]]
        fields = ("graphics_mhz","sm_mhz","memory_mhz","temperature_c","power_w","gpu_util_pct","device_vram_mib")
        clocks = {field:{"median":median(values),"min":min(values),"max":max(values)} for i,field in enumerate(fields,1)
                  if (values:=[float(x[i]) for x in clock_rows])}
        kernels = db.execute("""SELECT k.start,k.end,s.value,0 FROM CUPTI_ACTIVITY_KIND_KERNEL k JOIN StringIds s ON s.id=k.shortName
             WHERE k.start>=? AND k.end<=?""",(a,b)).fetchall()
        copies = db.execute("""SELECT start,end,CAST(copyKind AS TEXT),bytes FROM CUPTI_ACTIVITY_KIND_MEMCPY
             WHERE start>=? AND end<=?""",(a,b)).fetchall()
        runtime = db.execute("""SELECT k.start,k.end,s.value,0 FROM CUPTI_ACTIVITY_KIND_RUNTIME k JOIN StringIds s ON s.id=k.nameId
             WHERE k.start>=? AND k.end<=?""",(a,b)).fetchall()
        gpu = [row[:2] for row in kernels+copies]
        info = {"range_ms":ms(b-a),"kernel_union_ms":ms(union(row[:2] for row in kernels)),
                "memcpy_union_ms":ms(union(row[:2] for row in copies)),
                "gpu_union_ms":ms(union(gpu)),"visible_gpu_gap_ms":ms(b-a-union(gpu)),
                "sample_count":len(clock_rows),"clock_and_power":clocks,"kernels":grouped(kernels),
                "copies":grouped(copies),"host_runtime":grouped(runtime)}
        output["phases"][phase]=info
    ref=expected["reply"]
    for n in (1,2):
        reply=json.loads((path/f"r{n}.json").read_bytes())
        assert reply["tokens"]==ref["tokens"] and reply["content"]==ref["content"] and len(reply["tokens"])==512
    assert (path/"props-after.json").read_bytes()==expected["props"]
    p = [next(int(s.split()[1]) for s in (path/f"vmstat-{k}.txt").read_text().splitlines() if s.startswith("pswpout ")) for k in ("start","after")]
    output["system_swapout_pages_delta"]=p[1]-p[0]
    output["trace_sha256"]=hashlib.sha256((path/"prof.nsys-rep").read_bytes()).hexdigest()
    output["exact_outputs_and_props"]=True
    return output

a,b,ref=map(lambda s:Path(s).resolve(),sys.argv[1:])
expected={"reply":json.loads((ref/"r2.json").read_bytes()),"props":(ref/"props-after.json").read_bytes()}
results={name:report(path,expected) for name,path in (("a",a),("b",b))}
assert not results["a"]["kernel_selected"] and results["b"]["kernel_selected"]
Path("results.json").write_text(json.dumps(results,indent=2)+"\n")
for phase in ("prefill","decode"):
    x,y=(results[n]["phases"][phase] for n in "ab")
    print(phase)
    for k in ("range_ms","kernel_union_ms","memcpy_union_ms","gpu_union_ms","visible_gpu_gap_ms","sample_count"):
        print(k,round(x[k],3),round(y[k],3),round(y[k]-x[k],3))
    for sensor in ("graphics_mhz","memory_mhz","temperature_c","power_w"):
        print(sensor,x["clock_and_power"][sensor],y["clock_and_power"][sensor])
    for key in ("kernels","copies","host_runtime"):
        for n in sorted(set(x[key])|set(y[key]),key=lambda n:max(t[key].get(n,{}).get("sum_ms",0) for t in (x,y)),reverse=True)[:9]:
            print(key,n,x[key].get(n),y[key].get(n))
