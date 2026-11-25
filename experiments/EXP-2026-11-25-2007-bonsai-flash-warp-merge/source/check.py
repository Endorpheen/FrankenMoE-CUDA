"""Compare ordered Flash merge arithmetic on full scratch waves and tails."""
import ctypes as ct
import importlib.util
import json
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


def run_case(keys, queries, rng, kernels, events):
    tiles = (keys + 127) // 128
    per_head = (queries * 6 + 15) // 16
    groups = 4 * per_head
    active = min(groups, (32 * 1024 * 1024) // (tiles * 16 * 258 * 4))
    start = groups - active
    data = rng.standard_normal((active, tiles, 16, 258), dtype=np.float32)
    data[..., 1] = np.abs(data[..., 1]) + 1.0
    scratch = component.rec.Layer.stage(data.tobytes())
    out = gpu.allocate(queries * 24 * 256 * 4)
    try:
        gpu.call("cuMemsetD8_v2", out, U(0), gpu.S(queries * 24 * 256 * 4))
        args = scratch, out, U(queries), U(keys - queries + 1), U(tiles), U(start)

        def run(key):
            lanes = int(key[-1]) * 32 if key.startswith("warp") else 128
            component.dispatch(kernels[key], (active * 16, 1), lanes, args)

        run("control")
        a = bytes(gpu.download(out, queries * 24 * 256))
        assert np.isfinite(np.frombuffer(a, dtype=np.float32)).all()
        for k in kernels.keys() - {"control"}:
            run(k)
            b = bytes(gpu.download(out, queries * 24 * 256))
            assert a == b, (keys, queries, k)
        samples = {k: [] for k in kernels}
        for _ in range(5):
            for k in (*kernels, *reversed(kernels)):
                samples[k].append(component.elapsed(lambda: run(k), *events))
        return {"keys": keys, "queries": queries, "tiles": tiles, "active_groups": active,
                "group_start": start, "bit_exact": True,
                "median_ms": {k: median(x) for k, x in samples.items()}, "samples_ms": samples}
    finally:
        for ptr in (out, scratch):
            gpu.call("cuMemFree_v2", ptr)


device, ctx = I(), P()
gpu.call("cuInit", U(0))
gpu.call("cuDeviceGet", ct.byref(device), I(0))
gpu.call("cuCtxCreate_v2", ct.byref(ctx), U(0), device)
try:
    old, new = (component.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5])
    kernels = {}
    for label, name in [
        ("control", "bonsai_flash_compact_ordered_merge"),
        *((f"warp{w}", f"bonsai_flash_warp_merge{w}") for w in (1, 2, 4)),
        *((f"prefetch{n}", f"bonsai_flash_merge_prefetch{n}") for n in (2, 4, 8)),
    ]:
        fn = P()
        gpu.call("cuModuleGetFunction", ct.byref(fn), old if label == "control" else new, name.encode())
        kernels[label] = fn
    rng = np.random.default_rng(2007)
    events = component.cuda_event(), component.cuda_event()
    print(json.dumps([run_case(k, n, rng, kernels, events)
                      for k, n in ((2048, 33), (4097, 128), (8192, 1024), (16384, 1024),
                                   (32768, 1024), (33279, 127))], indent=2))
finally:
    gpu.call("cuCtxDestroy_v2", ctx)
