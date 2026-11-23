"""Offline cross-layer route prediction study on the EXP-1786 decode dump.

dump record: int32 width, int32 status, int32 ids[8], f32 input[width], f32 residual[width]
input = FFN input of layer L (router input), residual = post-attention residual of layer L.
Predictor P1(L): router_L applied to post_attention_norm_L * rms(residual_{L-1}).
Misses come from the exact SLRU replay of EXP-1772 (same answer, same ids).
"""
import sys, struct, json
from pathlib import Path
import numpy as np
sys.path.insert(0, sys.argv[3])
import gguf

dump = Path(sys.argv[1]); routes = Path(sys.argv[2])
W = 2048; REC = 8 + 32 + 8 * W
raw = np.fromfile(dump, dtype=np.uint8)
n = raw.size // REC; assert n * REC == raw.size
raw = raw.reshape(n, REC)
hdr = raw[:, :8].view(np.int32); assert (hdr[:, 0] == W).all() and (hdr[:, 1] == 0).all()
ids = raw[:, 8:40].view(np.int32)
xin = raw[:, 40:40 + 4 * W].view(np.float32)
res = raw[:, 40 + 4 * W:].view(np.float32)
L = 40; T = n // L; assert T * L == n
layer = np.arange(n) % L

r = gguf.GGUFReader('/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf')
tens = {t.name: t for t in r.tensors}
router = np.stack([np.array(tens[f'blk.{l}.ffn_gate_inp.weight'].data, dtype=np.float32).reshape(256, W) for l in range(L)])
pnorm = np.stack([np.array(tens[f'blk.{l}.post_attention_norm.weight'].data, dtype=np.float32) for l in range(L)])

def topk(x, k): return np.argsort(-x, axis=-1, kind='stable')[..., :k]
def rms(v, eps=1e-6): return v / np.sqrt((v.astype(np.float64) ** 2).mean(-1, keepdims=True) + eps).astype(np.float32)

# 1. exactness of router on recorded input
logits = np.einsum('nw,new->ne', xin, router[layer])
exact_sets = np.mean([set(a) == set(b) for a, b in zip(topk(logits, 8), ids)])
# 2. input == pnorm * rms(residual)?
recon = pnorm[layer] * rms(res)
rel = np.abs(recon - xin).max() / np.abs(xin).max()

# misses from exact SLRU replay
sys.path.insert(0, str(Path(__file__).parent))
data = routes.read_bytes()
version, gw, ln, sw = struct.unpack_from('<4Q', data); off = 32
groups = struct.unpack_from(f'<{gw}q', data, off); groups = [groups[i:i+9] for i in range(0, gw, 9)]; off += 8 * gw
lg = struct.unpack_from(f'<{ln}q', data, off); off += 8 * ln
state = struct.unpack_from(f'<{sw}i', data, off); off += 4 * sw
recs = list(struct.iter_unpack('<4I8B', data[off:]))[::3]
assert len(recs) == n
assert all(rc[0] == i % L and sorted(rc[4:]) == sorted(ids[i].tolist()) for i, rc in enumerate(recs)), "dump/trace mismatch"
import importlib.util
spec = importlib.util.spec_from_file_location('slru', str(Path(__file__).parent / 'slru_core.py'))
slru = importlib.util.module_from_spec(spec); spec.loader.exec_module(slru)
caches = [slru.CacheGroup(g, state, 80) for g in groups]
miss = []
for i, rc in enumerate(recs):
    c = caches[lg[rc[0]]]
    before = set(c.active)
    keys = [rc[0] * c.width + e for e in rc[4:]]
    f, _ = c.apply(rc[0], list(rc[4:]))
    m = [k % c.width for k in keys if k not in before]
    assert len(m) == f == rc[1]
    miss.append(m)
total_miss = sum(len(m) for m in miss)

out = {"records": n, "tokens": T, "router_exact_set_rate": float(exact_sets), "norm_recon_rel_err": float(rel), "misses": total_miss}
# predictors
prev = np.roll(np.arange(n), 1)  # record i-1
valid = layer > 0
for name, x in [("P1_prev_residual", pnorm[layer] * rms(res[prev])),
                ("P1b_prev_input", xin[prev])]:
    lg_ = np.einsum('nw,new->ne', x, router[layer])
    for k in (8, 10, 12, 16):
        pred = topk(lg_, k)
        hit = sum(len(set(m) & set(pred[i].tolist())) for i, m in enumerate(miss) if valid[i])
        tot = sum(len(m) for i, m in enumerate(miss) if valid[i])
        allhit = np.mean([len(set(ids[i].tolist()) & set(pred[i].tolist())) / 8 for i in range(n) if valid[i]])
        out[f"{name}_top{k}"] = {"miss_recall": hit / tot, "route_recall": float(allhit), "extra_per_layer": k}
out["layer0_misses"] = sum(len(m) for i, m in enumerate(miss) if not valid[i])
print(json.dumps(out, indent=1))
(dump.parent / 'route-predict.json').write_text(json.dumps(out, indent=1) + '\n')
