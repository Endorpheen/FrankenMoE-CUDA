"""Compare wider Bonsai K64 MMQ tiles using actual GGUF matrices."""
import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
from statistics import median
import sys

import numpy as np

experiments = Path(__file__).resolve().parents[2]
previous = experiments / "EXP-2026-11-25-2000-bonsai-mmq-k64/source/check-k64.py"
spec = importlib.util.spec_from_file_location("k64", previous)
k64 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k64)
gpu, component = k64.gpu, k64.component
I, P, U = gpu.I, gpu.P, gpu.U
k64.SHAPES = {
    "control": (64, 128, 256),
    "64x192_w8": (64, 192, 256),
    "64x256_w8": (64, 256, 256),
    "32x256_w8": (32, 256, 256),
    "64x256_w16": (64, 256, 512),
}

device, ctx = I(), P()
gpu.call("cuInit", U(0))
gpu.call("cuDeviceGet", ct.byref(device), I(0))
gpu.call("cuCtxCreate_v2", ct.byref(ctx), U(0), device)
try:
    old, new = (component.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5])
    kernels = {}
    for label in (*k64.SHAPES, "pack"):
        fn = P()
        name = {"control": "bonsai_pq2_mmq_64x128_k64", "pack": "bonsai_pack_half"}.get(
            label, f"bonsai_mmq_reuse_{label}")
        gpu.call("cuModuleGetFunction", ct.byref(fn), old if label in ("control", "pack") else new, name.encode())
        kernels[label] = fn
    events = component.cuda_event(), component.cuda_event()
    tensors = {t["name"]: t for t in gpu.inventory["tensors"]}
    rng = np.random.default_rng(2008)
    results = []
    with gpu.model.open("rb") as fd, mmap.mmap(fd.fileno(), 0, access=mmap.ACCESS_READ) as mm:
        for name, count, trim in (
            ("blk.0.attn_qkv.weight", 512, 0), ("blk.0.attn_qkv.weight", 1024, 0),
            ("blk.0.attn_qkv.weight", 1023, 1), ("blk.0.ssm_out.weight", 1024, 0),
            ("blk.3.attn_k.weight", 1024, 0), ("blk.0.ffn_gate.weight", 1024, 0),
            ("blk.0.ffn_up.weight", 1024, 0), ("blk.0.ffn_down.weight", 1024, 0),
            ("blk.42.ffn_down.weight", 1024, 0),
        ):
            t = tensors[name]
            assert t["type"] == 142
            width, rows = t["shape"]
            rows -= trim
            at = gpu.inventory["data_offset"] + t["offset"]
            data = mm[at:at + rows * (width // 128) * 34]
            result = dict(tensor=name, **k64.test(kernels, events, data, width, rows, count, rng))
            result["medians_ms"] = {k: median(v for arm, v in result["ordered_gpu_ms"] if arm == k)
                                     for k in k64.SHAPES}
            results.append(result)
    print(json.dumps(results, indent=2))
finally:
    gpu.call("cuCtxDestroy_v2", ctx)
