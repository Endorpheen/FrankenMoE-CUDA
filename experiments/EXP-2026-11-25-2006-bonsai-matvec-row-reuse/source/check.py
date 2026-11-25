"""Measure exact ordered Bonsai row reuse on actual PQ2 tensors."""
import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
from statistics import median
import sys

import numpy as np

experiments = Path(__file__).resolve().parents[2]
path = experiments / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py"
spec = importlib.util.spec_from_file_location("component", path)
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
I, P, U = gpu.I, gpu.P, gpu.U


def run_case(name, weights, width, rows, values, kernels, events, tail):
    weights_dev = component.rec.Layer.stage(weights)
    x = component.rec.Layer.stage(values.tobytes())
    out = gpu.allocate(rows * 4)
    try:
        args = weights_dev, x, out, U(width), U(rows)

        def run(n):
            group = n * 4
            component.dispatch(kernels[n], ((rows + group - 1) // group, 1), 128, args)

        run(1)
        reference = bytes(gpu.download(out, rows))
        assert np.isfinite(np.frombuffer(reference, dtype=np.float32)).all()
        for n in (2, 4):
            run(n)
            assert bytes(gpu.download(out, rows)) == reference, (name, n)
        samples = {n: [] for n in kernels}
        for _ in range(3):
            for n in (1, 2, 4, 4, 2, 1):
                def repeat():
                    for _ in range(12):
                        run(n)
                samples[n].append(component.elapsed(repeat, *events) / 12)
        return {"tensor": name, "width": width, "rows": rows, "tail": tail, "bit_exact": True,
                "median_ms": {n: median(times) for n, times in samples.items()},
                "samples_ms": samples}
    finally:
        for ptr in (out, x, weights_dev):
            gpu.call("cuMemFree_v2", ptr)


device, ctx = I(), P()
gpu.call("cuInit", U(0))
gpu.call("cuDeviceGet", ct.byref(device), I(0))
gpu.call("cuCtxCreate_v2", ct.byref(ctx), U(0), device)
try:
    control, trial = (component.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5])
    kernels = {}
    for n, module, suffix in ((1, control, "ordered"), (2, trial, "rows2"), (4, trial, "rows4")):
        fn = P()
        gpu.call("cuModuleGetFunction", ct.byref(fn), module, f"bonsai_pq2_matvec_{suffix}".encode())
        kernels[n] = fn
    rng = np.random.default_rng(2006)
    events = component.cuda_event(), component.cuda_event()
    tensors = {t["name"]: t for t in gpu.inventory["tensors"]}
    names = ("blk.0.attn_qkv.weight", "blk.0.ssm_out.weight", "blk.0.ffn_gate.weight",
             "blk.0.ffn_up.weight", "blk.0.ffn_down.weight", "blk.3.attn_q.weight",
             "blk.3.attn_k.weight", "output.weight")
    results = []
    with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as model:
        for name in names:
            tensor = tensors[name]
            assert tensor["type"] == 142, name
            width, rows = tensor["shape"]
            start = gpu.inventory["data_offset"] + tensor["offset"]
            stride = width // 128 * 34
            values = rng.normal(0, .7, width).astype(np.float32)
            for count in (rows, rows - 1) if name == names[0] else (rows,):
                results.append(run_case(name, model[start:start + count * stride], width,
                                        count, values, kernels, events, count != rows))
    print(json.dumps(results, indent=2))
finally:
    gpu.call("cuCtxDestroy_v2", ctx)
