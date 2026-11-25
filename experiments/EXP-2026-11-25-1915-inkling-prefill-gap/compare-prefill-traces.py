from collections import Counter, defaultdict
from pathlib import Path
import json
import sqlite3

ROOT = Path(__file__).resolve().parent
LABEL = {1: "H2D", 2: "D2H", 8: "D2D"}
LOGITS_BYTES = {"goose": 804096, "upstream": 800232}
PROFILES = {"goose": "ub512-baseline", "upstream": "upstream-ub512"}


def partition(a, b, spans):
    events = defaultdict(Counter)
    for kind, lo, hi in spans:
        lo, hi = max(a, lo), min(b, hi)
        if lo < hi:
            events[lo][kind] += 1
            events[hi][kind] -= 1
    active = Counter()
    output = Counter()
    for t0, t1 in zip(sorted({a, b, *events}), sorted({a, b, *events})[1:]):
        active.update(events[t0])
        live = tuple(sorted(k for k, n in active.items() if n > 0))
        key = live[0] if len(live) == 1 else "overlap" if live else "no GPU activity"
        output[key] += t1 - t0
    return {k: ns / 1e6 for k, ns in output.items()}


def analyze(mode, profile):
    directory = ROOT / "profile" / profile
    with sqlite3.connect(f"file:{directory / 'inkling.sqlite'}?mode=ro", uri=True) as db:
        logits = db.execute("""select start,end from CUPTI_ACTIVITY_KIND_MEMCPY
            where copyKind=2 and bytes=? order by end""", (LOGITS_BYTES[mode],)).fetchall()
        if len(logits) not in (128, 129, 130):
            raise ValueError(f"{mode}: unexpected logits readback count {len(logits)}")
        last_cold, first_warm = logits[-65], logits[-64]
        measured=json.loads((directory/"warm.json").read_text())["timings"]["prompt_ms"]
        b=first_warm[1]
        a=b-round(measured*1e6)
        if a<last_cold[1]:
            raise ValueError(f"{mode}: measured prefill overlaps the previous response")
        spans = []
        label_totals = Counter()
        for name, lo, hi in db.execute("""select s.value,k.start,k.end
            from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on s.id=k.shortName
            where k.start<? and k.end>?""", (b, a)):
            spans.append(("kernel",lo,hi))
            label_totals[name] += min(hi,b)-max(lo,a)

        volumes=Counter()
        for kind,size,lo,hi in db.execute("""select copyKind,bytes,start,end from CUPTI_ACTIVITY_KIND_MEMCPY
            where start<? and end>?""",(b,a)):
            name=LABEL.get(kind,f"copy {kind}")
            spans.append((name,lo,hi))
            volumes[name]+=size

        elapsed=partition(a,b,spans)
        assert abs(sum(elapsed.values())-(b-a)/1e6) < 1e-6

        api = []
        for name,count,ns in db.execute("""select s.value,count(*),sum(r.end-r.start)
            from CUPTI_ACTIVITY_KIND_RUNTIME r join StringIds s on s.id=r.nameId
            where r.start>=? and r.end<=? group by 1 order by 3 desc""",(a,b)):
            api.append({"name":name,"calls":count,"api_wall_ms":ns/1e6})

        api_spans=[]
        for name,lo,hi in db.execute("""select s.value,r.start,r.end
                from CUPTI_ACTIVITY_KIND_RUNTIME r join StringIds s on s.id=r.nameId
                where r.start<? and r.end>?""",(b,a)):
            if "Synchronize" in name:
                label="explicit sync"
            elif "Memcpy" in name:
                label="copy API"
            elif "Launch" in name:
                label="launch API"
            else:
                label="other CUDA API"
            api_spans.append((label,lo,hi))
        with_api=partition(a,b,spans+api_spans)
        categories=["explicit sync","copy API","launch API","other CUDA API","no GPU activity"]
        exclusive_idle={label:with_api.get(label,0) for label in categories}
        return {"anchor_logits_readbacks":len(logits),"window_ms":(b-a)/1e6,
                "reported_warm_prefill_ms":measured,"exclusive_ms":elapsed,
                "idle_and_api_exclusive_ms":exclusive_idle,
                "copy_bytes":volumes,"api":api,
                "summed_kernel_ms": {k:ns/1e6 for k,ns in label_totals.most_common()}}
if __name__=="__main__":
    result={mode:analyze(mode,profile) for mode,profile in PROFILES.items()}
    a,b=(result[mode]["exclusive_ms"] for mode in ("goose","upstream"))
    result["goose_minus_upstream_ms"]={key:a.get(key,0)-b.get(key,0) for key in sorted(set(a)|set(b))}
    print(json.dumps(result,indent=2))
