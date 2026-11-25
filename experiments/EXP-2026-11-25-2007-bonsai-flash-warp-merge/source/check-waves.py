"""Measure the exact Flash tile/merge path at several scratch wave budgets."""
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
BUDGETS = tuple(map(int, sys.argv[4:])) or (32, 64, 96, 128)
assert 32 in BUDGETS and min(BUDGETS) >= 8


def run_case(keys, queries, rng, tile, merge, events):
    count = (keys + 127) // 128
    chunks = (keys + 31) // 32
    groups = 4 * ((queries * 6 + 15) // 16)
    group_bytes = count * 16 * 258 * 4
    first = keys - queries + 1
    values = queries * 24 * 256
    storage = []

    def reserve(x):
        p = gpu.allocate(x)
        storage.append(p)
        return p

    def stage(x):
        p = component.rec.Layer.stage(x)
        storage.append(p)
        return p

    try:
        # Staged K and V are finite FP16. The scheduling check needs identical
        # tile arithmetic for both arms, regardless of the upstream packing.
        q = stage(rng.standard_normal(values, dtype=np.float32).tobytes())
        packed = (rng.standard_normal(chunks * 4 * 8192, dtype=np.float32) * .3).astype(np.float16).tobytes()
        k, v = stage(packed), stage(packed)
        scratch, out = reserve(max(BUDGETS) << 20), reserve(values * 4)
        waves = {b: (b << 20) // group_bytes for b in BUDGETS}

        def run(budget):
            wave = waves[budget]
            for start in range(0, groups, wave):
                active = min(wave, groups - start)
                common = (U(queries), U(first), U(count), U(start))
                component.dispatch(tile, (active, count), 128,
                                   (q, k, v, scratch, common[0], U(keys), *common[1:], U(chunks)))
                component.dispatch(merge, (active * 16, 1), 128, (scratch, out, *common))

        run(32)
        reference = bytes(gpu.download(out, values))
        assert np.isfinite(np.frombuffer(reference, dtype=np.float32)).all()
        for budget in BUDGETS:
            if budget == 32:
                continue
            run(budget)
            assert bytes(gpu.download(out, values)) == reference, (keys, queries, budget)
        samples = {b: [] for b in BUDGETS}
        for _ in range(3):
            for budget in (*BUDGETS, *reversed(BUDGETS)):
                samples[budget].append(component.elapsed(lambda: run(budget), *events))
        return dict(keys=keys, queries=queries, bit_exact=True,
                    waves={b:(groups+w-1)//w for b,w in waves.items()},
                    median_ms={b:median(v) for b,v in samples.items()}, samples_ms=samples)
    finally:
        for ptr in reversed(storage):
            gpu.call("cuMemFree_v2", ptr)


device, ctx = I(), P()
gpu.call("cuInit", U(0))
gpu.call("cuDeviceGet", ct.byref(device), I(0))
gpu.call("cuCtxCreate_v2", ct.byref(ctx), U(0), device)
try:
    module = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
    tile, merge = P(), P()
    gpu.call("cuModuleGetFunction", ct.byref(tile), module, b"bonsai_flash_async_tile")
    gpu.call("cuModuleGetFunction", ct.byref(merge), module, b"bonsai_flash_compact_ordered_merge")
    rng = np.random.default_rng(2027)
    events = component.cuda_event(), component.cuda_event()
    print(json.dumps([run_case(k, n, rng, tile, merge, events)
                      for k, n in ((4096, 1024), (8192, 1024), (16384, 1024),
                                   (32768, 1024), (32767, 1023))], indent=2))
finally:
    gpu.call("cuCtxDestroy_v2", ctx)
