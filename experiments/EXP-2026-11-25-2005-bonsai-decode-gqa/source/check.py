"""Compare grouped Bonsai decode against the frozen Q4_0 attention image."""
import ctypes as ct
import importlib.util
import json
from pathlib import Path
from statistics import median
import sys

import numpy as np

experiments = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "attention", experiments / "EXP-2026-11-25-1981-bonsai-flash-mma/source/check-attention.py")
attention = importlib.util.module_from_spec(spec)
spec.loader.exec_module(attention)
component, gpu = attention.component, attention.gpu
D, F, I, L, P, U = gpu.D, gpu.F, gpu.I, gpu.L, gpu.P, gpu.U


def function(module, name):
    fn = P()
    gpu.call("cuModuleGetFunction", ct.byref(fn), module, name.encode())
    return fn


def check_case(keys, functions, event_pair, rng):
    tiles = (keys + 255) // 256
    partial_count = 24 * tiles * 258
    config = (L * 22)(256, 256, 1, 24, 4, 4, keys, 24576, 1024, 576, 144, 576, 144,
                      0, 1024, 24576, 256, tiles, 24, 24, partial_count * 4, keys)
    storage = []

    def reserve(n):
        ptr = gpu.allocate(n)
        storage.append(ptr)
        return ptr

    def record(data):
        ptr = component.rec.Layer.stage(data)
        storage.append(ptr)
        return ptr

    try:
        cfg = record(bytes(config))
        q = record(rng.normal(0, .7, (24, 256)).astype(np.float32).tobytes())
        k, v, source = reserve(keys * 576), reserve(keys * 576), reserve(keys * 4096)
        for dst in (k, v):
            data = rng.normal(0, .7, (keys, 4, 256)).astype(np.float32).tobytes()
            gpu.upload(source, (ct.c_uint8 * len(data)).from_buffer_copy(data))
            component.dispatch(functions["rows"], ((keys * 32 + 127) // 128, 1), 128,
                               (source, dst, D(4096), U(32), D(keys * 32)))
        scratch, out = reserve(partial_count * 4), reserve(24576)

        def run(heads):
            if heads == 0:
                component.dispatch(functions[0], (24, tiles), 32,
                                   (q, k, v, D(0), scratch, cfg, F(1 / 16), I(0)))
            else:
                component.dispatch(functions[heads], (24 // heads, tiles), 32,
                                   (q, k, v, scratch, U(keys), U(tiles), F(1 / 16)))
            component.dispatch(functions["merge"], (24, 1), 32, (scratch, out, cfg, I(0)))

        def read(ptr, count):
            return np.ctypeslib.as_array(gpu.download(ptr, count)).copy()

        run(0)
        partial_ref, result_ref = read(scratch, partial_count), read(out, 24 * 256)
        assert np.isfinite(result_ref).all()
        report = {}
        for heads in (1, 2, 3):
            run(heads)
            partial, result = read(scratch, partial_count), read(out, 24 * 256)
            assert np.isfinite(result).all()
            report[heads] = {
                "partials_exact": partial_ref.tobytes() == partial.tobytes(),
                "output_exact": result_ref.tobytes() == result.tobytes(),
                "max_output_error": float(np.max(np.abs(result_ref - result))),
            }
        times = {heads: [] for heads in (0, 1, 2, 3)}
        for _ in range(5):
            for heads in (0, 1, 2, 3, 3, 2, 1, 0):
                times[heads].append(component.elapsed(lambda: run(heads), *event_pair))
        return {"keys": keys, "variants": report,
                "median_ms": {heads: median(samples) for heads, samples in times.items()},
                "samples_ms": times}
    finally:
        for ptr in reversed(storage):
            gpu.call("cuMemFree_v2", ptr)


device, ctx = I(), P()
gpu.call("cuInit", U(0))
gpu.call("cuDeviceGet", ct.byref(device), I(0))
gpu.call("cuCtxCreate_v2", ct.byref(ctx), U(0), device)
try:
    frozen, trial = (component.rec.Layer.load(Path(path).read_bytes()) for path in sys.argv[3:5])
    funcs = {0: function(frozen, "goose_q4_attention_tiles"),
             "merge": function(frozen, "goose_q4_attention_merge"),
             "rows": function(frozen, "goose_set_q4_rows")}
    funcs.update({h: function(trial, f"bonsai_decode_gqa{h}") for h in (1, 2, 3)})
    events = component.cuda_event(), component.cuda_event()
    rng = np.random.default_rng(2005)
    print(json.dumps([check_case(n, funcs, events, rng)
                      for n in (65, 257, 1024, 8192, 32768, 33279)], indent=2))
finally:
    gpu.call("cuCtxDestroy_v2", ctx)
