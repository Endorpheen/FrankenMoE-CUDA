"""Measure pack plus projection on real Bonsai GDN PQ2 weights."""

import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
from statistics import median
import sys

import numpy as np


source = Path(__file__).resolve().parents[2] / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py"
spec = importlib.util.spec_from_file_location("component", source)
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
I, P, U = gpu.I, gpu.P, gpu.U
TENSORS = (
    "blk.0.attn_qkv.weight", "blk.0.attn_gate.weight",
    "blk.62.attn_qkv.weight", "blk.62.attn_gate.weight",
)
NAMES = (
    "bonsai_pack_half", "bonsai_pq2_half_full_5120",
    "bonsai_pack_q8_128",
)


def run_case(kernels, raw, rows, rng, events):
    tokens, width = 1024, 5120
    data = rng.normal(0, .7, (tokens, width)).astype(np.float32)
    data[0] = 0
    stage = component.rec.Layer.stage
    weight, source = stage(raw), stage(data.tobytes())
    packed = gpu.allocate(tokens * width * 2)
    scales = gpu.allocate(tokens * (width // 128) * 4)
    output = gpu.allocate(tokens * rows * 4)
    pairs, groups = tokens * width // 2, tokens * (width // 128)
    grid = rows // 64, tokens // 128
    shape = U(width), U(rows), U(tokens)

    def fp16():
        component.dispatch(kernels[0], ((pairs + 255) // 256, 1),
                           256, (source, packed, U(pairs)))
        component.dispatch(kernels[1], grid, 256, (weight, packed, output, *shape))

    def int8():
        component.dispatch(kernels[2], ((groups + 3) // 4, 1),
                           128, (source, packed, scales, U(groups)))
        component.dispatch(kernels[rows], grid, 256, (weight, packed, scales, output, *shape))

    try:
        fp16()
        expected = np.ctypeslib.as_array(gpu.download(output, tokens * rows)).copy()
        int8()
        actual = np.ctypeslib.as_array(gpu.download(output, tokens * rows))
        assert np.isfinite(expected).all() and np.isfinite(actual).all()
        assert not expected[:rows].any() and not actual[:rows].any()
        delta = actual - expected
        rel_l2 = float(np.linalg.norm(delta) / np.linalg.norm(expected))
        max_abs = float(np.max(np.abs(delta)))
        readings = {"fp16": [], "int8": []}
        runners = {"fp16": fp16, "int8": int8}
        for label in ("fp16", "int8", "int8", "fp16") * 4:
            readings[label].append(component.elapsed(runners[label], *events))
        before, after = map(median, (readings["fp16"], readings["int8"]))
        return {
            "width": width, "rows": rows, "tokens": tokens,
            "relative_l2": rel_l2, "max_abs": max_abs, "zero_row_exact": True,
            "raw_ms": readings, "fp16_median_ms": before, "int8_median_ms": after,
            "elapsed_reduction_pct": (1 - after / before) * 100,
        }
    finally:
        for pointer in (output, scales, packed, source, weight):
            gpu.call("cuMemFree_v2", pointer)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        module = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        names = dict(enumerate(NAMES))
        names.update({rows: f"bonsai_pq2_mmq_int8_full_n128_{rows}" for rows in (6144, 10240)})
        kernels = {}
        for label, name in names.items():
            function = P()
            gpu.call("cuModuleGetFunction", ct.byref(function), module, name.encode())
            kernels[label] = function
        events = component.cuda_event(), component.cuda_event()
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        rng = np.random.default_rng(2080)
        results = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
            for name in TENSORS:
                tensor = tensors[name]
                width, rows = tensor["shape"]
                assert tensor["type"] == 142 and width == 5120 and rows in (6144, 10240)
                start = gpu.inventory["data_offset"] + tensor["offset"]
                raw = weights[start:start + rows * (width // 128) * 34]
                results.append({"tensor": name, **run_case(kernels, raw, rows, rng, events)})
        print(json.dumps(results, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
