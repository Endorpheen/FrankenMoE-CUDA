"""Require bit-exact outputs after changing only cumulative weight unpack."""

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
    packed = gpu.allocate(tokens * width * 2)
    values = tokens*rows
    output = gpu.allocate(values*4)
    try:
        pairs = tokens * width // 2
        shape = U(width), U(rows), U(tokens)
        component.dispatch(kernels["pack"], ((pairs+255)//256, 1),
                           256, (source, packed, U(pairs)))
        grid = (rows+63)//64, (tokens+127)//128
        names = [f"reference_{width}", f"lut_{width}", f"half2_{width}"]
        if "--down-only-reverse" in sys.argv:
            names.reverse()
        outputs = {}
        for name in names:
            component.dispatch(kernels[name], grid, 256, (weight, packed, output, *shape))
            outputs[name] = bytes(gpu.download(output, values))
        expected = outputs[f"reference_{width}"]
        assert np.isfinite(np.frombuffer(expected, dtype=np.float32)).all()
        assert all(actual == expected for actual in outputs.values()), (width, rows, tokens)
        return {"width": width, "rows": rows, "tokens": tokens, "bit_exact": True}
    finally:
        for pointer in (output, packed, source, weight):
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
            **{f"reference_{w}": f"bonsai_pq2_half_full_{w}" for w in (5120, 6144, 17408)},
            **{f"{variant}_{w}": f"bonsai_pq2_half_unpack_{variant}_{w}"
               for variant in ("lut", "half2") for w in (5120, 6144, 17408)},
        }
        kernels = {}
        for label, name in names.items():
            fn = P()
            gpu.call("cuModuleGetFunction", ct.byref(fn), mod, name.encode())
            kernels[label] = fn
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        rng = np.random.default_rng(2071)
        result = []
        occupancy = {}
        for label in kernels.keys() - {"pack"}:
            blocks = I()
            gpu.call("cuOccupancyMaxActiveBlocksPerMultiprocessor",
                     ct.byref(blocks), kernels[label], I(256), gpu.S(0))
            occupancy[label] = blocks.value
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
            for name, tokens, trim in (
                ("blk.0.attn_qkv.weight", 1024, 0),
                ("blk.0.ssm_out.weight", 1024, 0),
                ("blk.0.ffn_up.weight", 1024, 0),
                ("blk.0.ffn_down.weight", 1024, 0),
            ):
                if "--down-only-reverse" in sys.argv and not name.endswith("ffn_down.weight"):
                    continue
                tensor = tensors[name]
                width, rows = tensor["shape"]
                assert tensor["type"] == 142
                rows -= trim
                offset = gpu.inventory["data_offset"] + tensor["offset"]
                raw = weights[offset:offset+rows*(width//128)*34]
                result.append({"tensor": name, **run_case(kernels, raw, width, rows, tokens, rng)})
        print(json.dumps({"max_active_blocks_per_sm": occupancy, "cases": result}, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
