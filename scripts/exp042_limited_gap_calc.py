# Reproducible offline analysis for limited-gap H2D coalescing (pre-analysis, EXP-2026-09-07-042).
# Input: EXP-032 saved H2D submission log (post-scheduler copies, byte sizes include MMQ trailing padding).
# No server, no build, no model. Regenerates the merge table from raw log records only.
import math, json, hashlib, collections
from functools import reduce
LOG="results/archive/EXP-2026-09-06-034/raw/exp032-h2d-ts/h2d.log"
SLOT=16*1024*1024
PAD=512
recs=[]
for ln in open(LOG):
    d=dict(p.split('=',1) for p in ln.strip().split('|')[1:])
    recs.append((d['tensor'],int(d['offset']),int(d['bytes'])))
tot=len(recs); tot_bytes=sum(r[2] for r in recs)
by=collections.defaultdict(list)
for t,o,b in recs: by[t].append((o,b))
esize={t:(reduce(math.gcd,[o for o,_ in rs if o>0]) if any(o>0 for o,_ in rs) else 0) for t,rs in by.items()}

def merge_by_skip(max_skip):
    copies=0; extra=0; hist=collections.Counter()
    for t,rs in by.items():
        es=esize[t]; rs.sort()
        groups=[[rs[0][0], rs[0][0]+rs[0][1]]]   # [span_start, span_end] in bytes
        for o,b in rs[1:]:
            g=groups[-1]
            gap=o-g[1]
            skip=(gap+PAD)//es if es else 0
            newend=max(g[1], o+b)
            # bridge only if the gap is an exact run of `skip` untouched experts and span fits one slot
            if 1<=skip<=max_skip and (gap-skip*es+PAD)==0 and (newend-g[0])<=SLOT:
                hist[skip]+=1; extra+=gap; g[1]=newend
            else:
                groups.append([o,o+b])
        copies+=len(groups)
    return copies,extra,hist

out={"log_sha256":hashlib.sha256(open(LOG,'rb').read()).hexdigest(),
     "input_records":tot,"input_bytes_MiB":round(tot_bytes/1048576,2),
     "slot_bytes_MiB":round(SLOT/1048576,2),"trailing_pad_bytes":PAD,
     "expert_size_bytes":sorted(set(esize.values())),
     "merge_by_skipped_experts":[],"merge_by_fixed_byte_gap":[]}
for ms in (1,2,3):
    c,e,h=merge_by_skip(ms)
    out["merge_by_skipped_experts"].append({
        "max_skip":ms,"copies_after":c,"copies_reduction_percent":round((tot-c)/tot*100,2),
        "extra_bytes_MiB":round(e/1048576,2),"extra_percent":round(e/tot_bytes*100,2),
        "bridges_by_skip":{str(k):v for k,v in sorted(h.items())}})
# reference: fixed byte-gap thresholds (reproduces the proposal table, but projection-blind)
def merge_bytegap(gmax):
    copies=0; span_bytes=0
    for t,rs in by.items():
        rs.sort(); groups=[[rs[0][0],rs[0][0]+rs[0][1]]]
        for o,b in rs[1:]:
            g=groups[-1]; gap=o-g[1]; ne=max(g[1],o+b)
            if 0<=gap<=gmax and (ne-g[0])<=SLOT: g[1]=ne
            else: groups.append([o,o+b])
        copies+=len(groups); span_bytes+=sum(e-s for s,e in groups)
    return copies,span_bytes-tot_bytes
for gb in (524288,1048576,2097152):
    c,e=merge_bytegap(gb)
    out["merge_by_fixed_byte_gap"].append({"gap_KiB":gb//1024,"copies_after":c,
        "copies_reduction_percent":round((tot-c)/tot*100,2),"extra_bytes_MiB":round(e/1048576,2),
        "extra_percent":round(e/tot_bytes*100,2)})
json.dump(out,open("benchmarks/exp042-limited-gap-h2d-coalescing-offline.json","w"),indent=2)
print(json.dumps(out,indent=2))
