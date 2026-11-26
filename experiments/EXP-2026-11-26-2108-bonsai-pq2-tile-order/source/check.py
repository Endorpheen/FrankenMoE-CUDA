"""Compare the active target image and equivalent CTA orders on real FFN weights."""

import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
from statistics import median
import sys

import numpy as np


experiment_dir = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "pq2", experiment_dir / "EXP-2026-11-26-2081-bonsai-q8-paired-weight-load/source/check.py")
pq2 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pq2)
mmq, gpu = pq2.component, pq2.gpu
I, P, U = mmq.I, mmq.P, mmq.U


def function(module, name):
    kernel = P()
    gpu.call("cuModuleGetFunction", ct.byref(kernel), module, name.encode())
    return kernel


def check(matrix, rng, pack, kernels, events):
    width, rows, tokens = 5120, 17408, 1024
    x = rng.normal(0, 0.7, (tokens, width)).astype(np.float32)
    x[0] = 0
    stage = mmq.rec.Layer.stage
    w, data = stage(matrix), stage(x.tobytes())
    packed = gpu.allocate(tokens * width)
    groups = tokens * (width // 128)
    scales = gpu.allocate(groups * 4)
    output = gpu.allocate(tokens * rows * 4)
    args = w, packed, scales, output, U(width), U(rows), U(tokens)

    def launch(group):
        mmq.dispatch(kernels[group], (272, 8), 256, args)

    try:
        mmq.dispatch(pack, ((groups + 3) // 4, 1), 128, (data, packed, scales, U(groups)))
        launch(1)
        expected = np.ctypeslib.as_array(gpu.download(output, tokens * rows)).view(np.uint32).copy()
        assert np.isfinite(expected.view(np.float32)).all()
        for g in (2, 4, 8):
            launch(g)
            actual = np.ctypeslib.as_array(gpu.download(output, tokens * rows)).view(np.uint32)
            assert np.array_equal(expected, actual), g

        samples = {g: [] for g in kernels}
        for g in (1, 2, 4, 8, 8, 4, 2, 1) * 6:
            samples[g].append(mmq.elapsed(lambda: launch(g), *events))
        return {"matched_f32_words": tokens * rows, "samples_ms": samples,
                "median_ms": {g: median(readings) for g, readings in samples.items()}}
    finally:
        for ptr in (output, scales, packed, data, w):
            gpu.call("cuMemFree_v2", ptr)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        base, candidate = [mmq.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5]]
        name = pq2.NAMES["spread"]
        kernels = {1: function(base, name)}
        for g in (2, 4, 8):
            kernels[g] = function(candidate, f"{name}_order{g}")
        pack = function(base, pq2.NAMES["panel_pack"])
        events = mmq.cuda_event(), mmq.cuda_event()
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        rng = np.random.default_rng(2108)
        reports = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
            for name in pq2.TENSORS:
                t = tensors[name]
                assert t["shape"] == [5120, 17408] and t["type"] == 142
                start = gpu.inventory["data_offset"] + t["offset"]
                matrix = mapped[start:start + 17408 * 40 * 34]
                reports.append({"tensor": name, **check(matrix, rng, pack, kernels, events)})
        print(json.dumps({"cases": reports}, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
