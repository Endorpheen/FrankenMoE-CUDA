"""Check packed-fragment MMA against the exact cached K64 control."""
import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
from statistics import median
import sys

import numpy as np


experiments = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "check_k64", experiments / "EXP-2026-11-25-2000-bonsai-mmq-k64/source/check-k64.py")
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)
gpu, component = probe.gpu, probe.component
I, P, U = gpu.I, gpu.P, gpu.U


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        module = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        kernels = {}
        for label, symbol in {
            "pack": "bonsai_pack_half",
            "cached_k64": "bonsai_pq2_cached_64x128_k64",
            "fragment_k64": "bonsai_pq2_fragment_k64",
            "fragment_k128": "bonsai_pq2_fragment_k128",
            "fragment_k64_loop": "bonsai_pq2_fragment_k64_loop",
            "fragment_k128_loop": "bonsai_pq2_fragment_k128_loop",
            "fragment_k128_n64": "bonsai_pq2_fragment_k128_n64",
            "fragment_k128_m32": "bonsai_pq2_fragment_k128_m32",
        }.items():
            function = P()
            gpu.call("cuModuleGetFunction", ct.byref(function), module, symbol.encode())
            kernels[label] = function
        probe.SHAPES = {label: (64, 128, 256) for label in kernels if label != "pack"}
        probe.SHAPES["fragment_k128_n64"] = (64, 64, 256)
        probe.SHAPES["fragment_k128_m32"] = (32, 128, 256)
        events = component.cuda_event(), component.cuda_event()
        rng = np.random.default_rng(2037)
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        results = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
            for name, tokens, trim in (
                ("blk.0.ffn_down.weight", 1024, 0), ("blk.0.ffn_down.weight", 1023, 1),
                ("blk.42.ffn_down.weight", 1024, 0), ("blk.63.ffn_down.weight", 1024, 0),
                ("blk.0.ffn_up.weight", 1024, 0), ("blk.0.attn_qkv.weight", 1024, 0),
                ("blk.3.attn_output.weight", 512, 0),
            ):
                tensor = tensors[name]
                width, rows = tensor["shape"]
                assert tensor["type"] == 142
                rows -= trim
                offset = gpu.inventory["data_offset"] + tensor["offset"]
                raw = mapped[offset:offset + rows * (width // 128) * 34]
                row = probe.test(kernels, events, raw, width, rows, tokens, rng)
                row["median_ms"] = {
                    label: median(v for key, v in row["ordered_gpu_ms"] if key == label)
                    for label in probe.SHAPES
                }
                results.append(dict(tensor=name, **row))
        print(json.dumps(results, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
