"""Require bit-exact results against the generic K64 kernel on real matrices."""

import ctypes as ct
import importlib.util
import mmap
from pathlib import Path
import sys

import numpy as np


path = Path(__file__).resolve().parents[2] / "EXP-2026-11-25-2000-bonsai-mmq-k64/source/check-k64.py"
spec = importlib.util.spec_from_file_location("check_k64", path)
reference = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reference)
gpu, component = reference.gpu, reference.component
I, P, U = gpu.I, gpu.P, gpu.U


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        mod = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())

        def bind(name):
            result = P()
            gpu.call("cuModuleGetFunction", ct.byref(result), mod, name.encode())
            return result

        pack = bind("bonsai_pack_half")
        control = bind("bonsai_pq2_cached_64x128_k64")
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        rng = np.random.default_rng(2048)
        tokens = 1024
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
            for name in ("blk.0.attn_qkv.weight", "blk.0.ffn_up.weight", "blk.0.ffn_down.weight"):
                tensor = tensors[name]
                width, rows = tensor["shape"]
                assert tensor["type"] == 142 and rows % 64 == 0
                candidate = bind(f"bonsai_pq2_full_64x128_k64_{width}")
                offset = gpu.inventory["data_offset"] + tensor["offset"]
                raw = weights[offset:offset + rows * (width // 128) * 34]
                expected, actual = compare(pack, control, candidate, raw, width, rows, tokens, rng)
                assert actual == expected, name
                print(f"{name}: {rows * tokens:,} exact FP32 values")
    finally:
        gpu.call("cuCtxDestroy_v2", context)


def compare(pack, control, candidate, raw, width, rows, tokens, rng):
    stage = component.rec.Layer.stage
    weights = stage(raw)
    values = stage(rng.normal(0, .7, (tokens, width)).astype(np.float32).tobytes())
    packed = gpu.allocate(tokens * width * 2)
    result = gpu.allocate(tokens * rows * 4)
    try:
        pairs = tokens * width // 2
        component.dispatch(pack, ((pairs + 255) // 256, 1), 256, (values, packed, U(pairs)))
        args = weights, packed, result, U(width), U(rows), U(tokens)
        grid = rows // 64, tokens // 128
        component.dispatch(control, grid, 256, args)
        expected = bytes(gpu.download(result, rows * tokens))
        assert np.isfinite(np.frombuffer(expected, dtype=np.float32)).all()
        component.dispatch(candidate, grid, 256, args)
        return expected, bytes(gpu.download(result, rows * tokens))
    finally:
        for pointer in (result, packed, values, weights):
            gpu.call("cuMemFree_v2", pointer)


if __name__ == "__main__":
    main()
