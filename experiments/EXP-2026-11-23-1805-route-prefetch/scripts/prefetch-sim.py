"""Timing/traffic model for cross-layer predicted expert prefetch (EXP-1786 trace + decode dump).

At the route readback of layer L-1 the runtime also gets top-k of router_L applied to
FFN input of L-1. Critical misses of L-1 are copied first; predicted, non-resident experts of
L are then queued on the same expert stream (FIFO). Prefetched experts enter SLRU probation;
evictions skip the live sets of L-1 and L-2 (and L's predicted set).
Model: one PCIe queue at BW bytes/us; per layer compute C us between FFN start and the next
route; FFN of L starts when its critical copies (and everything queued before them) finish.
"""
import sys, struct, json, importlib.util
from pathlib import Path
import numpy as np
sys.path.insert(0, sys.argv[3]); import gguf
here = Path(__file__).parent
spec = importlib.util.spec_from_file_location('slru', str(here / 'slru_core.py'))
slru = importlib.util.module_from_spec(spec); spec.loader.exec_module(slru)

dump = Path(sys.argv[1]); routes = Path(sys.argv[2])
W = 2048; REC = 8 + 32 + 8 * W; L = 40
raw = np.fromfile(dump, dtype=np.uint8); n = raw.size // REC; raw = raw.reshape(n, REC)
ids = raw[:, 8:40].view(np.int32); xin = raw[:, 40:40 + 4 * W].view(np.float32)
r = gguf.GGUFReader('/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf')
tens = {t.name: t for t in r.tensors}
router = np.stack([np.array(tens[f'blk.{l}.ffn_gate_inp.weight'].data, dtype=np.float32).reshape(256, W) for l in range(L)])
layer = np.arange(n) % L
nxt = (layer + 1) % L
plog = np.empty((n, 256), np.float32)   # prediction for record i+1 made at record i
for l in range(L):
    sel = layer == l
    plog[sel] = xin[sel] @ router[(l + 1) % L].T
order = np.argsort(-plog, axis=-1, kind='stable')
prob = np.exp(plog - plog.max(1, keepdims=True)); prob /= prob.sum(1, keepdims=True)
sprob = np.take_along_axis(prob, order, 1)

data = routes.read_bytes()
version, gw, ln, sw = struct.unpack_from('<4Q', data); off = 32
groups = struct.unpack_from(f'<{gw}q', data, off); groups = [groups[i:i+9] for i in range(0, gw, 9)]; off += 8 * gw
lg = struct.unpack_from(f'<{ln}q', data, off); off += 8 * ln
state = struct.unpack_from(f'<{sw}i', data, off); off += 4 * sw
cost = [sum(g[:3]) for g in groups]

def prefetch(c, lay, experts, protect, cold=False):
    fills = 0
    for e in experts:
        key = lay * c.width + e
        if key in c.active: continue
        for q in c.lists:
            slot = next((s for s in q if c.keys[s] not in protect), None)
            if slot is not None: q.pop(slot); break
        else: return fills
        old = c.keys[slot]
        if old >= 0: del c.active[old]
        c.keys[slot] = key; c.active[key] = slot; c.lists[0][slot] = None; fills += 1
        if cold: c.lists[0].move_to_end(slot, last=False)
    return fills

def run(k, BW=22.4e3, C=256.0, cross_token=False, tau=0.0, insert_protected=False, cold=False, preempt=False, touch=False):
    caches = [slru.CacheGroup(g, state, 80) for g in groups]
    t = 0.0; pcie = 0.0; crit_bytes = pre_bytes = 0; crit_n = 0; wait = 0.0
    live = [set(), set()]
    for i in range(n):
        lay = int(layer[i]); g = lg[lay]; c = caches[g]
        keys = [lay * c.width + int(e) for e in ids[i]]
        m = c.apply(lay, [int(e) for e in sorted(set(ids[i].tolist()))])[0]
        b = m * cost[g]; crit_bytes += b; crit_n += m
        start = t if preempt else max(t, pcie)
        if preempt: pre_busy = max(0.0, pcie - t); pcie = start + b / BW + pre_busy
        else: pcie = start + b / BW
        ffn = max(t, (start + b / BW) if preempt else pcie); wait += ffn - t
        # prefetch next layer
        if k and (lay < L - 1 or cross_token):
            nl = int(nxt[i]); ng = lg[nl]; nc = caches[ng]
            protect = set(keys) | live[0] | live[1]
            pk = [int(e) for e, pr in zip(order[i, :k], sprob[i, :k]) if pr >= tau]
            f = nc.apply(nl, sorted(set(pk)))[0] if touch else prefetch(nc, nl, pk, protect, cold)
            pre_bytes += f * cost[ng]; pcie = max(pcie, ffn) + f * cost[ng] / BW if f else pcie
        live = [set(keys), live[0]]
        t = ffn + C
    T = n // L
    return dict(touch=touch, cold=cold, pre=preempt, tau=tau, cross=cross_token, k=k, crit_misses=crit_n, crit_gb=crit_bytes / 1e9, prefetch_gb=pre_bytes / 1e9,
                total_gb=(crit_bytes + pre_bytes) / 1e9, wait_ms_per_token=wait / 1e3 / T,
                decode_s=t / 1e6, tok_s=T / (t / 1e6))

rows = [run(0)]
for k in (6, 8, 10, 12):
    for touch in (False, True):
        for pre in (False, True):
            rows.append(run(k, touch=touch, preempt=pre))
for x in rows: print(json.dumps(x))
(dump.parent / 'prefetch-sim.json').write_text(json.dumps(rows, indent=1) + '\n')
