"""Compare the resident code planes against the accepted packed K64 kernel."""

import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
import sys

import numpy as np


path = Path(__file__).resolve().parents[2] / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py"
spec = importlib.util.spec_from_file_location("component", path)
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
I, P, U = gpu.I, gpu.P, gpu.U


def run_case(kernels, raw, width, rows, tokens, rng):
    stage = component.rec.Layer.stage
    values = rng.normal(0, .7, (tokens, width)).astype(np.float32)
    values[0] = 0
    weight, source = stage(raw), stage(values.tobytes())
    packed = gpu.allocate(tokens * width * 2)
    entries = rows * (width // 128)
    codes, scales = gpu.allocate(entries*32), gpu.allocate(entries*2)
    output = gpu.allocate(tokens*rows*4)
    try:
        pairs = tokens * width // 2
        component.dispatch(kernels["pack"], ((pairs+255)//256, 1),
                           256, (source, packed, U(pairs)))
        grid = (rows+63)//64, (tokens+127)//128
        args = packed, output, U(width), U(rows), U(tokens)
        component.dispatch(kernels["reference"], grid, 256, (weight, *args))
        expected = bytes(gpu.download(output, tokens*rows))
        component.dispatch(kernels["repack"], ((entries*16+255)//256, 1), 256,
                           (weight, codes, scales, U(rows), U(width//128), U(entries)))
        component.dispatch(kernels["planes"], grid, 256, (codes, scales, *args))
        actual = bytes(gpu.download(output, tokens*rows))
        assert actual == expected
        assert np.isfinite(np.frombuffer(actual, dtype=np.float32)).all()
        return {"width": width, "rows": rows, "tokens": tokens,
                "resident_bytes": entries*34, "bit_exact": True}
    finally:
        for pointer in (output, scales, codes, packed, source, weight):
            gpu.call("cuMemFree_v2", pointer)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        mod = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        names = {
            "pack": "bonsai_pack_half",
            "reference": "bonsai_pq2_cached_64x128_k64",
            "repack": "bonsai_pack_pq2_resident_planes",
            "planes": "bonsai_pq2_cached_64x128_planes",
        }
        kernels = {}
        for label, name in names.items():
            fn = P()
            gpu.call("cuModuleGetFunction", ct.byref(fn), mod, name.encode())
            kernels[label] = fn
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        rng = np.random.default_rng(2055)
        result = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
            for name, tokens, trim in (
                ("blk.0.attn_qkv.weight", 129, 1),
                ("blk.0.attn_qkv.weight", 1024, 0),
                ("blk.0.ffn_up.weight", 1024, 0),
                ("blk.0.ffn_down.weight", 1024, 0),
            ):
                tensor = tensors[name]
                width, rows = tensor["shape"]
                assert tensor["type"] == 142
                rows -= trim
                offset = gpu.inventory["data_offset"] + tensor["offset"]
                raw = weights[offset:offset+rows*(width//128)*34]
                result.append({"tensor": name, **run_case(kernels, raw, width, rows, tokens, rng)})
        print(json.dumps(result, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
