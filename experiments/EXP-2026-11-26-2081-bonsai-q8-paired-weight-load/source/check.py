"""Measure exact paired PQ2 weight loads on real full gate/up matrices."""

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
    "blk.0.ffn_gate.weight", "blk.0.ffn_up.weight",
    "blk.62.ffn_gate.weight", "blk.62.ffn_up.weight",
)
NAMES = {
    "pack": "bonsai_pack_q8_128",
    "scalar": "bonsai_pq2_mmq_int8_full_n128",
    "paired": "bonsai_pq2_mmq_int8_full_n128_pairload",
}


def run_case(kernels, raw, tokens, rng, events):
    width, rows = 5120, 17408
    data = rng.normal(0, .7, (tokens, width)).astype(np.float32)
    data[0] = 0
    stage = component.rec.Layer.stage
    weight, source = stage(raw), stage(data.tobytes())
    packed = gpu.allocate(tokens * width)
    groups = tokens * (width // 128)
    scales = gpu.allocate(groups * 4)
    output = gpu.allocate(tokens * rows * 4)
    grid = rows // 64, tokens // 128
    params = weight, packed, scales, output, U(width), U(rows), U(tokens)

    def launch(label):
        component.dispatch(kernels["pack"], ((groups + 3) // 4, 1),
                           128, (source, packed, scales, U(groups)))
        component.dispatch(kernels[label], grid, 256, params)

    try:
        launch("scalar")
        expected = np.ctypeslib.as_array(gpu.download(output, tokens * rows)).copy()
        launch("paired")
        actual = np.ctypeslib.as_array(gpu.download(output, tokens * rows))
        assert np.isfinite(expected).all() and not expected[:rows].any()
        assert actual.view(np.uint32).tobytes() == expected.view(np.uint32).tobytes()
        readings = {"scalar": [], "paired": []}
        for label in ("scalar", "paired", "paired", "scalar") * 4:
            readings[label].append(component.elapsed(lambda: launch(label), *events))
        before, after = map(median, (readings["scalar"], readings["paired"]))
        return {
            "width": width, "rows": rows, "tokens": tokens, "bit_exact": True,
            "raw_ms": readings, "scalar_median_ms": before, "paired_median_ms": after,
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
        kernels = {}
        for label, name in NAMES.items():
            function = P()
            gpu.call("cuModuleGetFunction", ct.byref(function), module, name.encode())
            kernels[label] = function
        events = component.cuda_event(), component.cuda_event()
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        rng = np.random.default_rng(2081)
        results = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
            for name in TENSORS:
                tensor = tensors[name]
                width, rows = tensor["shape"]
                assert tensor["type"] == 142 and (width, rows) == (5120, 17408)
                start = gpu.inventory["data_offset"] + tensor["offset"]
                raw = weights[start:start + rows * (width // 128) * 34]
                results.append({"tensor": name, **run_case(kernels, raw, 1024, rng, events)})
        print(json.dumps(results, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
