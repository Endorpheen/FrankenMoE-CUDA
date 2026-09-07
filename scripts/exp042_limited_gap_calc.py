# Reproducible offline analysis for limited-gap H2D coalescing (EXP-2026-09-07-042, PRELIMINARY).
# Input: EXP-032 saved H2D submission log (post-scheduler copies; byte size includes MMQ trailing padding).
# No server, no model run, no build. expert_size (nb[2]) comes from GGUF metadata (n_bytes/ne[2]), not from log GCD.
import math, json, glob, hashlib, collections, re
from functools import reduce
LOG="results/archive/EXP-2026-09-06-034/raw/exp032-h2d-ts/h2d.log"
MODEL_GLOB="models/qwen38/UD-IQ3_XXS/*.gguf"
SLOT=16*1024*1024
PAD=512
recs=[]
for ln in open(LOG):
    d=dict(p.split('=',1) for p in ln.strip().split('|')[1:])
    recs.append((d['tensor'],int(d['offset']),int(d['bytes'])))
tot=len(recs); tot_bytes=sum(r[2] for r in recs)
by=collections.defaultdict(list)
for t,o,b in recs: by[t].append((o,b))

def gguf_nb2():
    # authoritative per-tensor expert_size = n_bytes // ne[2]; returns {} if model/lib absent
    try:
        from gguf.gguf_reader import GGUFReader
    except Exception:
        return {}
    allt={}
    for p in sorted(glob.glob(MODEL_GLOB)):
        try: r=GGUFReader(p)
        except Exception: continue
        ts=r.tensors if isinstance(r.tensors,(list,tuple)) else list(r.tensors.values())
        for t in ts: allt[t.name]=t
    out={}
    for name,t in allt.items():
        m=re.match(r'(blk\.\d+\.ffn_(?:gate|up|down)_exps\.weight)$',name)
        if not m: continue
        shape=[int(x) for x in t.shape]
        if len(shape)<3: continue
        out[m.group(1)]={"nb2":t.n_bytes//shape[2],"dtype":int(t.tensor_type),"shape":tuple(shape)}
    return out

def log_to_gguf(t):  # CUDA0#blk.2.ffn_gate_exps.weight#0 -> blk.2.ffn_gate_exps.weight
    return re.sub(r'^CUDA\d+#','',t).rsplit('#',1)[0]
gg=gguf_nb2()
# per-tensor authoritative expert_size; if GGUF unavailable, fall back to log-offset GCD (upper bound, see caveat)
esize={}; esrc={}
for t in by:
    g=log_to_gguf(t)
    if g in gg: esize[t]=gg[g]["nb2"]; esrc[t]="gguf"
    else:
        offs=[o for o,_ in by[t] if o>0]
        esize[t]=reduce(math.gcd,offs) if offs else 0; esrc[t]="gcd_fallback(upper_bound)"
# cross-check GGUF sizes vs GCD to record the caveat quantitatively
gcd_ok=sum(1 for t in by if esize[t]==(reduce(math.gcd,[o for o,_ in by[t] if o>0]) if any(o>0 for o,_ in by[t]) else esize[t]))

def merge(max_skip=None, max_gap=None):
    copies=0; extra=0; hist=collections.Counter()
    for t,rs in by.items():
        es=esize[t]; rs.sort()
        groups=[[rs[0][0], rs[0][0]+rs[0][1]]]
        for o,b in rs[1:]:
            g=groups[-1]; gap=o-g[1]; ne=max(g[1],o+b)
            if (ne-g[0])>SLOT: groups.append([o,o+b]); continue
            ok=False
            if max_gap is not None:
                ok = 0<=gap<=max_gap
            if max_skip is not None:
                skip=(gap+PAD)//es if es else 0
                ok = 1<=skip<=max_skip and (gap-skip*es+PAD)==0
            if ok: hist[( (gap+PAD)//es if es else 0 )]+=1; extra+=gap; g[1]=ne
            else: groups.append([o,o+b])
        copies+=len(groups)
    return copies,extra,hist

def row(**kw):
    c,e,h=merge(**kw)
    return {"copies_after":c,"copies_reduction_percent":round((tot-c)/tot*100,2),
            "extra_bytes_MiB":round(e/1048576,2),"extra_percent":round(e/tot_bytes*100,2),
            "extra_MiB_per_removed_call":round(e/1048576/max(tot-c,1),3),
            "bridges_by_skip":{str(k):v for k,v in sorted(h.items())} if h else {}}

out={"log_sha256":hashlib.sha256(open(LOG,'rb').read()).hexdigest(),
     "input_records":tot,"input_bytes_MiB":round(tot_bytes/1048576,2),
     "slot_bytes_MiB":round(SLOT/1048576,2),"trailing_pad_bytes":PAD,
     "expert_size_source":{"gguf_available":bool(gg),"tensors_from_gguf":sum(1 for v in esrc.values() if v=="gguf"),
        "gcd_matches_gguf_or_fallback":gcd_ok,"note":"nb[2] taken from GGUF n_bytes/ne[2]; log-offset GCD is only an upper bound and is not used when GGUF is present"},
     "expert_size_by_role":collections.Counter((gg[log_to_gguf(t)]["dtype"],esize[t]) if esrc[t]=="gguf" else (None,esize[t]) for t in by) if False else None,
     "expert_size_table":[{"dtype":gg[log_to_gguf(t)]["dtype"],"nb2":esize[t],"layers_role":re.search(r'ffn_(gate|up|down)_exps',log_to_gguf(t)).group(1)} for t in sorted(by)][:0],
     "candidate_byte_gap":[
        {"gap_KiB":gb, **row(max_gap=gb)} for gb in (524288,1048576,2097152)],
     "candidate_max_skip":[
        {"max_skip":ms, **row(max_skip=ms)} for ms in (1,2,3)],
    }
# compact role/dtype/size summary from GGUF
role_sz=collections.Counter()
for t in by:
    g=log_to_gguf(t)
    role=re.search(r'ffn_(gate|up|down)_exps',g).group(1)
    dt=gg[g]["dtype"] if g in gg else None
    role_sz[(role,dt,esize[t])]+=1
out["expert_size_table"]=[{"role":r,"ggml_dtype":dt,"nb2_bytes":nb,"tensors":c} for (r,dt,nb),c in sorted(role_sz.items())]
json.dump(out,open("benchmarks/exp042-limited-gap-h2d-coalescing-offline.json","w"),indent=2)
print(json.dumps(out,indent=2))
