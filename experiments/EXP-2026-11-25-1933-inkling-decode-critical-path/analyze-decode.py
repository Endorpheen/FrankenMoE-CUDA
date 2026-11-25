import importlib.util
import json
from collections import Counter
from pathlib import Path
import re
import sqlite3
import sys
from statistics import mean

root = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("timeline_partition", root / "timeline-partition.py")
partition_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(partition_module)
partition = partition_module.partition
directory = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "profile/current"
response = json.loads((directory / "cold.json").read_text())
count = response["timings"]["predicted_n"]
intervals = min(count - 1, 128)
pattern = r"^I2 decode step=(\d+) total=([\d.]+) attn=([\d.]+) bridge=([\d.]+) moe=([\d.]+) head=([\d.]+)$"
rows = re.findall(pattern, (directory / "server.log").read_text(), re.M)
host_means = {}
if rows:
    assert len(rows) == intervals * 2
    rows = rows[-intervals:]
    labels = ("total", "attention", "bridge", "moe", "head")
    host_means = {name: mean(float(row[i + 1]) for row in rows) for i, name in enumerate(labels)}
    host_means["other_backend"] = host_means["total"] - sum(host_means[k] for k in labels[1:])
with sqlite3.connect(f"file:{directory / 'inkling.sqlite'}?mode=ro", uri=True) as db:
    logits = [r[0] for r in db.execute("""select end from CUPTI_ACTIVITY_KIND_MEMCPY
        where copyKind=2 and bytes=? order by end""", (int(sys.argv[2]) if len(sys.argv) > 2 else 804096,))]
    assert len(logits) >= intervals + 1
    lo, hi = logits[-intervals - 1], logits[-1]
    reported = response["timings"]["predicted_ms"] / (count - 1)
    wall = (hi - lo) / 1e6 / intervals
    assert wall > 0
    spans = []
    kernels = Counter()
    for name, start, end in db.execute("""select s.value,k.start,k.end
            from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on s.id=k.shortName
            where k.start<? and k.end>?""", (hi, lo)):
        spans.append(("kernel", start, end))
        kernels[name] += min(end, hi) - max(start, lo)
    copy = Counter()
    for kind, size, start, end in db.execute("""select copyKind,bytes,start,end
            from CUPTI_ACTIVITY_KIND_MEMCPY where start<? and end>?""", (hi, lo)):
        label = partition_module.LABEL.get(kind, str(kind))
        spans.append((label, start, end))
        copy[label, size, "calls"] += 1
        copy[label, size, "ns"] += min(end, hi) - max(start, lo)
    api = []
    for name, start, end in db.execute("""select s.value,r.start,r.end
            from CUPTI_ACTIVITY_KIND_RUNTIME r join StringIds s on s.id=r.nameId
            where r.start<? and r.end>?""", (hi, lo)):
        if "Synchronize" in name:
            label = "sync API"
        elif "Memcpy" in name:
            label = "copy API"
        elif "Launch" in name:
            label = "launch API"
        else:
            label = "other CUDA API"
        api.append((label, start, end))
    gpu = {k: v / intervals for k, v in partition(lo, hi, spans).items()}
    with_api = partition(lo, hi, spans + api)
    none = ("sync API", "copy API", "launch API", "other CUDA API", "no GPU activity")
    excess = gpu["no GPU activity"] - sum(with_api.get(k, 0) / intervals for k in none)
    summary = {
        "intervals": intervals,
        "captured_logits_readbacks": len(logits),
        "trace_wall_ms_per_token": wall,
        "http_decode_ms_per_token": reported,
        "host_phases_ms_per_token": host_means,
        "gpu_exclusive_ms_per_token": gpu,
        "gpu_inactive_exclusive_api_ms_per_token": {
            **{k: with_api.get(k, 0) / intervals for k in none},
            "concurrent_api_or_other": excess,
        },
        "kernel_sum_ms_per_token": {k: t / 1e6 / intervals for k, t in kernels.most_common()},
        "copy_sizes": [
            {"direction": kind, "bytes_each": size, "calls_per_token": copy[kind, size, "calls"] / intervals,
             "ms_per_token": ns / 1e6 / intervals}
            for (kind, size, field), ns in copy.most_common() if field == "ns"
        ],
    }
    print(json.dumps(summary, indent=2))
