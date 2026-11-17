#!/usr/bin/env python3
"""Component-isolating probe: zero one trunk part, compare runtime vs oracle."""
import importlib.util
import json
import os
import subprocess
import sys
import tempfile

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(ROOT, "experiments", "EXP-2026-11-17-927-maxwell-quant-convert", "tests"))
sys.path.insert(0, os.path.join(ROOT, "experiments", "EXP-2026-11-17-929-maxwell-numeric-oracle", "tests"))

import test_convert as T
import numeric as N

CONVERT = os.path.join(ROOT, "tools", "maxwell-convert.py")
LOAD = "/home/end0/AI/LlamaServer/maxwell-build-20261117/load_forward"


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "none"
    rng = np.random.default_rng(11)
    tensors = T.build_tensors(rng)
    by = {}
    for name, arr in tensors:
        a = arr.copy()
        if mode == "attn" and ("self_attn." in name) and name.endswith((".q_proj.weight", ".k_proj.weight",
                                                                        ".v_proj.weight", ".o_proj.weight")):
            a[...] = 0.0
        if mode == "ffn" and (".block_sparse_moe." in name or ".shared_expert." in name):
            a[...] = 0.0
        if mode == "hc" and ("hyper_connection" in name or "hc_head" in name):
            a[...] = 0.0
        by[name] = a

    with tempfile.TemporaryDirectory() as td:
        json.dump(T.CFG, open(os.path.join(td, "config.json"), "w"))
        shards = {f"model-0000{i}.safetensors": [] for i in (1, 2, 3)}
        idx = {"weight_map": {}}
        for k, (name, arr) in enumerate(tensors):
            shards[list(shards)[k % 3]].append((name, by[name]))
            idx["weight_map"][name] = list(shards)[k % 3]
        for fn, items in shards.items():
            T.write_safetensors(os.path.join(td, fn), items)
        json.dump(idx, open(os.path.join(td, "model.safetensors.index.json"), "w"))
        gguf = os.path.join(td, "p.gguf")
        subprocess.run([sys.executable, CONVERT, td, gguf, "--stub-vocab", "--f32"],
                       check=True, capture_output=True)
        logits = os.path.join(td, "rt.tsv")
        r = subprocess.run([LOAD, gguf, logits], capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout[-500:], r.stderr[-500:])
            return 1
        rt = np.loadtxt(logits)
        oracle = N.Oracle(T.CFG, by)
        ref = oracle.forward([1, 2, 3, 4])[-1]
        d = np.abs(rt - ref)
        print(f"zero={mode:6s} max_abs={d.max():.6g} mean_abs={d.mean():.6g} "
              f"argmax rt={int(rt.argmax())} oracle={int(ref.argmax())}")
        return 0


if __name__ == "__main__":
    sys.exit(main())
