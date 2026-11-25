"""Require bit-exact results for specialized Q8 gate/up full tiles."""

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
    data = rng.normal(0, .7, (tokens, width)).astype(np.float32)
    data[0] = 0
    weight, source = stage(raw), stage(data.tobytes())
    packed = gpu.allocate(tokens * width)
    scales = gpu.allocate(tokens * (width // 128) * 4)
    values = tokens*rows
    output = gpu.allocate(values*4)
    try:
        groups = tokens * (width // 128)
        shape = U(width), U(rows), U(tokens)
        component.dispatch(kernels["pack"], ((groups+3)//4, 1),
                           128, (source, packed, scales, U(groups)))
        grid = (rows+63)//64, (tokens+63)//64
        component.dispatch(kernels["reference"], grid, 256, (weight, packed, scales, output, *shape))
        expected = np.ctypeslib.as_array(gpu.download(output, values)).copy()
        assert np.isfinite(expected).all()
        for tile in (64, 128):
            component.dispatch(kernels[tile], (grid[0], (tokens+tile-1)//tile),
                               256, (weight, packed, scales, output, *shape))
            actual = np.ctypeslib.as_array(gpu.download(output, values))
            assert actual.tobytes() == expected.tobytes(), (width, rows, tokens, tile)
        return {"width": width, "rows": rows, "tokens": tokens, "bit_exact": True}
    finally:
        for pointer in (output, scales, packed, source, weight):
            gpu.call("cuMemFree_v2", pointer)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        mod = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        names = {
            "pack": "bonsai_pack_q8_128",
            "reference": "bonsai_pq2_mmq_int8",
            64: "bonsai_pq2_mmq_int8_full_n64",
            128: "bonsai_pq2_mmq_int8_full_n128",
        }
        kernels = {}
        for label, name in names.items():
            fn = P()
            gpu.call("cuModuleGetFunction", ct.byref(fn), mod, name.encode())
            kernels[label] = fn
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        rng = np.random.default_rng(2073)
        result = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
            for name, tokens, trim in (
                ("blk.48.ffn_gate.weight", 1024, 0),
                ("blk.48.ffn_up.weight", 1024, 0),
                ("blk.63.ffn_gate.weight", 1024, 0),
                ("blk.63.ffn_up.weight", 1024, 0),
            ):
                tensor = tensors[name]
                width, rows = tensor["shape"]
                assert tensor["type"] == 142 and (width, rows) == (5120, 17408)
                rows -= trim
                offset = gpu.inventory["data_offset"] + tensor["offset"]
                raw = weights[offset:offset+rows*(width//128)*34]
                result.append({"tensor": name, **run_case(kernels, raw, width, rows, tokens, rng)})
        print(json.dumps(result, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
