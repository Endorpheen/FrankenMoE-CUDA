"""Report vocabulary-only changes on the same draft hidden/KV histories."""
import json
from pathlib import Path
import sys

import numpy as np


VOCAB = 248320
HIDDEN = 5120
control, candidate = [np.fromfile(path, dtype="<f4").reshape(-1, VOCAB + 2 * HIDDEN)
                      for path in map(Path, sys.argv[1:3])]
assert control.shape == candidate.shape
assert np.isfinite(control).all() and np.isfinite(candidate).all()
assert control[:, VOCAB:].tobytes() == candidate[:, VOCAB:].tobytes()
results = []
for position, (a, b) in enumerate(zip(control[:, :VOCAB], candidate[:, :VOCAB])):
    delta = a.astype(np.float64) - b
    results.append({
        "position": position, "logits": VOCAB,
        "max_absolute_difference": float(np.max(np.abs(delta))),
        "relative_l2": float(np.linalg.norm(delta) / np.linalg.norm(a.astype(np.float64))),
        "control_top_id": int(a.argmax()),
        "candidate_top_id": int(b.argmax()),
        "top10_overlap": len(set(np.argsort(a)[-10:]) & set(np.argsort(b)[-10:])),
    })
print(json.dumps(results, indent=2))
