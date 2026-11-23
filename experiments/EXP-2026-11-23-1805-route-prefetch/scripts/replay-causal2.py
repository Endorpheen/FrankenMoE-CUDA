"""Offline replay of additional causal expert-cache policies on the EXP-1772 route trace.

Same trace, same per-group capacities and initial post-prefill state as replay-policy.py.
Bytes = misses * per-expert cost of the group. 'bypass' policies may refuse admission:
the missed expert is still uploaded (counted) but does not replace a resident one.
Split: first 128 tokens = fit, later 383 = holdout.
"""
from collections import OrderedDict, defaultdict, Counter
from pathlib import Path
import heapq, json, struct, sys, math

root = Path(sys.argv[1])
data = (root / "routes.bin").read_bytes()
version, words, layers_n, state_n = struct.unpack_from("<4Q", data)
off = 32
groups = struct.unpack_from(f"<{words}q", data, off); groups = [groups[i:i+9] for i in range(0, words, 9)]
off += 8 * words
layer_group = struct.unpack_from(f"<{layers_n}q", data, off); off += 8 * layers_n
state = struct.unpack_from(f"<{state_n}i", data, off); off += 4 * state_n
records = list(struct.iter_unpack("<4I8B", data[off:]))
split = 128 * layers_n
tasks = [[] for _ in groups]
for i in range(0, len(records), 3):
    a = records[i]; layer = a[0]; g = layer_group[layer]
    tasks[g].append((i // 3 >= split, tuple(layer * groups[g][3] + e for e in a[4:]), a[1]))

def initial_order(layout):  # coldest first (probation head .. protected tail)
    start, slots = layout[7], layout[5]
    keys = state[start:start+slots]; nxt = state[start+2*slots:start+3*slots]
    order = []
    for seg in (0, 1):
        s = state[start + 4*slots + seg]
        while s != -1:
            if keys[s] >= 0: order.append(keys[s])
            s = nxt[s]
    return order

def cost(layout): return sum(layout[:3])

# ---------- policies: each returns [misses_fit, misses_holdout] ----------
def run_lfu(layout, work, decay_tokens=None, bypass=False, layers_per_group=None):
    """Priority = (decayed count, recency). bypass: admit a miss only if its score
    after this access exceeds the victim's score (TinyLFU-style)."""
    cap = layout[5]; init = initial_order(layout)
    lam = 0.0 if decay_tokens is None else math.log(2) / (decay_tokens * layout[4])
    score = {}; last = {}
    for i, k in enumerate(init): score[k] = 1.0; last[k] = i - len(init)
    def val(k, t):  # decayed score at time t
        return score.get(k, 0.0) * math.exp(-lam * (t - last.get(k, t)))
    resident = set(init); counts = [0, 0]
    for t, (hold, keys, _) in enumerate(work):
        live = set(keys)
        for k in keys:
            score[k] = val(k, t) + 1.0; last[k] = t
        miss = [k for k in keys if k not in resident]
        counts[hold] += len(miss)
        need = len(resident) + len(miss) - cap
        if need > 0:
            cand = sorted(((val(k, t), last[k], k) for k in resident if k not in live))
            if not bypass:
                for _, _, k in cand[:need]: resident.discard(k)
                resident.update(miss)
            else:
                free = cap - len(resident)
                miss.sort(key=lambda k: -val(k, t))
                j = 0
                for k in miss:
                    if free > 0: resident.add(k); free -= 1; continue
                    if j < len(cand) and val(k, t) > cand[j][0]:
                        resident.discard(cand[j][2]); j += 1; resident.add(k)
                    # else: bypass, computed from staging, not cached
        else:
            resident.update(miss)
    return counts

def run_arc(layout, work):
    c = layout[5]; init = initial_order(layout)
    T1 = OrderedDict(); T2 = OrderedDict(); B1 = OrderedDict(); B2 = OrderedDict(); p = 0
    for k in init: T2[k] = None          # post-prefill residents treated as frequent
    counts = [0, 0]
    for hold, keys, _ in work:
        live = set(keys)
        def replace(x_in_b2):
            nonlocal p
            # evict from T1 or T2 LRU, skipping keys live in this call
            def pop_lru(T):
                for k in T:
                    if k not in live: T.pop(k); return k
                return None
            if T1 and (len(T1) > p or (x_in_b2 and len(T1) == p)):
                v = pop_lru(T1)
                if v is not None: B1[v] = None; return
                v = pop_lru(T2); B2[v] = None
            else:
                v = pop_lru(T2)
                if v is not None: B2[v] = None; return
                v = pop_lru(T1); B1[v] = None
        for x in keys:
            if x in T1: T1.pop(x); T2[x] = None; continue
            if x in T2: T2.move_to_end(x); continue
            counts[hold] += 1
            if x in B1:
                p = min(c, p + max(len(B2) // max(len(B1), 1), 1)); replace(False); B1.pop(x); T2[x] = None; continue
            if x in B2:
                p = max(0, p - max(len(B1) // max(len(B2), 1), 1)); replace(True); B2.pop(x); T2[x] = None; continue
            if len(T1) + len(B1) == c:
                if len(T1) < c: B1.popitem(last=False); replace(False)
                else:
                    for k in T1:
                        if k not in live: T1.pop(k); break
            elif len(T1) + len(B1) < c and len(T1) + len(T2) + len(B1) + len(B2) >= c:
                if len(T1) + len(T2) + len(B1) + len(B2) >= 2 * c:
                    (B2 if B2 else B1).popitem(last=False)
                replace(False)
            T1[x] = None
            while len(B1) + len(B2) > c:  # keep ghosts bounded
                (B1 if len(B1) > len(B2) else B2).popitem(last=False)
    return counts

def run_s3fifo(layout, work, small_frac=0.1):
    c = layout[5]; init = initial_order(layout)
    ns = max(1, int(c * small_frac)); nm = c - ns
    S = OrderedDict(); M = OrderedDict(); G = OrderedDict(); freq = Counter()
    for k in init[-nm:]: M[k] = None
    for k in init[:-nm][-ns:]: S[k] = None
    counts = [0, 0]
    for hold, keys, _ in work:
        live = set(keys)
        def evict_m():
            while True:
                for k in M:
                    if k in live: continue
                    if freq[k] > 0: freq[k] -= 1; M.move_to_end(k); break
                    M.pop(k); return
                else: return  # all live (cannot happen at these sizes)
        def evict_s():
            for k in S:
                if k in live: continue
                S.pop(k)
                if freq[k] > 1:
                    if len(M) >= nm: evict_m()
                    M[k] = None
                else:
                    G[k] = None
                    if len(G) > nm: G.popitem(last=False)
                freq[k] = 0
                return True
            return False
        for x in keys:
            if x in S or x in M: freq[x] = min(3, freq[x] + 1); continue
            counts[hold] += 1
            while len(S) + len(M) >= c:
                if len(S) >= ns and evict_s(): continue
                evict_m()
            if x in G:
                G.pop(x)
                if len(M) >= nm: evict_m()
                M[x] = None
            else: S[x] = None
            freq[x] = 0
    return counts

def total(fn):
    per = [fn(layout, work) for layout, work in zip(groups, tasks)]
    return [sum(c[i] * cost(l) for l, c in zip(groups, per)) for i in (0, 1)]

ref = [sum(sum(n for h, _, n in w if h == part) * cost(l) for part in ()) for l, w in zip(groups, tasks)]
reference = [sum(sum(n for h, _, n in w if h == part) * cost(l) for l, w in zip(groups, tasks)) for part in (0, 1)]
assert sum(reference) == 31474040832, sum(reference)
rows = {"exact-slru": reference}
rows["arc"] = total(run_arc)
for f in (0.05, 0.1, 0.2):
    rows[f"s3fifo-{f}"] = total(lambda l, w: run_s3fifo(l, w, f))
rows["lfu"] = total(lambda l, w: run_lfu(l, w))
for h in (8, 16, 32, 64):
    rows[f"lfu-decay{h}"] = total(lambda l, w: run_lfu(l, w, h))
rows["tinylfu-bypass"] = total(lambda l, w: run_lfu(l, w, None, True))
for h in (16, 32, 64, 128):
    rows[f"tinylfu-bypass-decay{h}"] = total(lambda l, w: run_lfu(l, w, h, True))
for label, b in rows.items():
    print(f"{label:24s} all={sum(b)/1e9:7.3f} GB ({sum(b)/sum(reference)-1:+.2%}) "
          f"fit128={b[0]/reference[0]-1:+.2%} hold383={b[1]/reference[1]-1:+.2%}")
(root / "causal2-replay.json").write_text(json.dumps({"split_in_tokens": 128, "bytes_by_segment": rows}, indent=2) + "\n")
