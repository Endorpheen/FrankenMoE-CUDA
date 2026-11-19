#!/usr/bin/env python3
"""Depth check: build a 45-layer synthetic Maxwell and run one forward."""
import json
import os
import subprocess
import sys
import tempfile

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "experiments", "EXP-2026-11-17-927-maxwell-quant-convert", "tests"))
import test_convert as T

CONVERT = os.path.join(ROOT, "tools", "maxwell-convert.py")
LOAD = "/home/end0/AI/LlamaServer/maxwell-build-20261117/load_forward"
N_LAYER = 45


def main():
    cfg = dict(T.CFG)
    cfg["num_hidden_layers"] = N_LAYER
    cfg["mlp_only_layers"] = [0, 1, 2]
    rng = np.random.default_rng(5)
    tensors = T.build_tensors(rng, n_layer=N_LAYER, dense=(0, 1, 2))
    print(f"tensors={len(tensors)}")
    with tempfile.TemporaryDirectory() as td:
        json.dump(cfg, open(os.path.join(td, "config.json"), "w"))
        shards = {f"model-{i:05d}.safetensors": [] for i in range(1, 4)}
        idx = {"weight_map": {}}
        for k, (n, a) in enumerate(tensors):
            key = list(shards)[k % 3]
            shards[key].append((n, a))
            idx["weight_map"][n] = key
        for fn, items in shards.items():
            T.write_safetensors(os.path.join(td, fn), items)
        json.dump(idx, open(os.path.join(td, "model.safetensors.index.json"), "w"))
        out = os.path.join(td, "deep.gguf")
        r = subprocess.run([sys.executable, CONVERT, td, out, "--stub-vocab"],
                           capture_output=True, text=True)
        print(r.stdout.strip(), r.stderr[-300:])
        if r.returncode != 0:
            return 1
        r = subprocess.run([LOAD, out], capture_output=True, text=True)
        print(r.stdout[-400:])
        print("STDERR tail:", r.stderr[-300:])
        ok = r.returncode == 0 and "decode rc=0" in r.stdout and "finite=1" in r.stdout
        print("MAXWELL-DEPTH OK" if ok else "MAXWELL-DEPTH FAILED")
        return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
