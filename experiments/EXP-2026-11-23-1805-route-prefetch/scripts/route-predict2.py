"""Predictor A: router_L(post_attention_norm_L * rms(out_{L-1})) from dump v2; consistency checks."""
import sys, json, importlib.util
from pathlib import Path
import numpy as np
sys.path.insert(0, sys.argv[2]); import gguf
dump = Path(sys.argv[1]); W = 2048; L = 40
REC = 8 + 32 + 16 * W
raw = np.fromfile(dump, dtype=np.uint8); n = raw.size // REC; raw = raw[:n * REC].reshape(n, REC)  # last record may be unflushed at SIGTERM
hdr = raw[:, :8].view(np.int32); ids = raw[:, 8:40].view(np.int32)
f = lambda j: raw[:, 40 + 4*W*j: 40 + 4*W*(j+1)].view(np.float32)
xin, res, prev, end = f(0), f(1), f(2), f(3)
layer = np.arange(n) % L
r = gguf.GGUFReader('/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf'); t = {x.name: x for x in r.tensors}
router = np.stack([np.array(t[f'blk.{l}.ffn_gate_inp.weight'].data, dtype=np.float32).reshape(256, W) for l in range(L)])
pnorm = np.stack([np.array(t[f'blk.{l}.post_attention_norm.weight'].data, dtype=np.float32) for l in range(L)])
def rms(v): return v / np.sqrt((v.astype(np.float64)**2).mean(-1, keepdims=True) + 1e-6).astype(np.float32)
out = {"status_ok": bool((hdr[:, 1] == 0).all())}
out["prev_eq_end_frac"] = float(np.mean(np.all(prev[1:] == end[1:], axis=1)))
out["prev_eq_res_frac"] = float(np.mean(np.all(prev[1:] == res[1:], axis=1)))
d = res[1:] - end[1:]
out["attn_norm_ratio_median"] = float(np.median(np.linalg.norm(d, axis=1) / np.linalg.norm(res[1:], axis=1)))
# ffn of record i-1 = end_i - res_{i-1}
x = pnorm[layer] * rms(end)
lg = np.empty((n, 256), np.float32)
for l in range(L):
    s = layer == l; lg[s] = x[s] @ router[l].T
order = np.argsort(-lg, 1, kind='stable')
np.save(dump.parent / 'predA-order.npy', order[:, :32].astype(np.int16))
valid = np.arange(n) > 0
for k in (8, 10, 12, 16):
    rec = np.mean([len(set(ids[i].tolist()) & set(order[i, :k].tolist())) / 8 for i in range(n) if valid[i]])
    out[f"A_route_recall_top{k}"] = float(rec)
print(json.dumps(out, indent=1))
