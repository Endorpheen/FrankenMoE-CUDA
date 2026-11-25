from collections import Counter, defaultdict
import importlib.util
import json
from pathlib import Path
import re
import sqlite3
import sys

root = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("timeline", root / "timeline-partition.py")
timeline = importlib.util.module_from_spec(spec)
spec.loader.exec_module(timeline)
directory = Path(sys.argv[1]) if len(sys.argv) > 1 else root
log = (directory / "server.log").read_text()
rows = [{k: float(v) for k, v in re.findall(r"(\w+)=([\d.]+)", line)}
        for line in log.splitlines() if line.startswith("I2 prefill phases")]
assert [int(row["first"]) for row in rows] == list(range(0, 32768, 4096))
with sqlite3.connect(f"file:{directory / 'inkling.sqlite'}?mode=ro", uri=True) as db:
    lo, hi = db.execute("select start,end from NVTX_EVENTS where text='Inkling late prefill'").fetchone()
    lanes = defaultdict(list)
    for name, start, end in db.execute("select text,start,end from NVTX_EVENTS where start>=? and end<=? "
                                       "and text like 'Inkling %' and text!='Inkling late prefill'", (lo, hi)):
        lanes[name].append((start,end))
    assert sorted(map(len, lanes.values())) == [48]*5, dict((k,len(v)) for k,v in lanes.items())
    activity = []
    kernels = Counter()
    for name, start, end in db.execute("""select s.value,k.start,k.end from CUPTI_ACTIVITY_KIND_KERNEL k
          join StringIds s on s.id=k.shortName where k.start<? and k.end>?""", (hi,lo)):
        activity.append(("kernel",start,end))
        kernels[name] += min(end,hi)-max(start,lo)
    copies = Counter()
    for kind, size, start, end in db.execute("""select copyKind,bytes,start,end from CUPTI_ACTIVITY_KIND_MEMCPY
          where start<? and end>?""", (hi,lo)):
        name = timeline.LABEL.get(kind, f"copy {kind}")
        activity.append((name,start,end))
        copies[name,size] += 1
    result = {
        "phase_timings": rows,
        "phase_totals_ms": {k: sum(r[k] for r in rows) for k in rows[0] if k not in ("first","count")},
        "final_ubatch_range_ms": (hi-lo)/1e6,
        "gpu_exclusive_ms": timeline.partition(lo,hi,activity),
        "kernel_sum_ms": {k:t/1e6 for k,t in kernels.most_common()},
        "copies": [{"direction":k,"bytes_each":s,"count":n} for (k,s),n in copies.most_common()],
        "nvtx_phase_gpu_ms": {},
    }
    for name, windows in lanes.items():
        phase = Counter()
        for a,b in windows:
            phase.update(timeline.partition(a,b,activity))
        result["nvtx_phase_gpu_ms"][name] = dict(phase)
print(json.dumps(result, indent=2))
