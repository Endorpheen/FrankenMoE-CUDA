"""Check ordered attention merge arithmetic on three independent Q4 query rows."""
import ctypes as ct
import importlib.util
import json
from pathlib import Path
from statistics import median
import sys

import numpy as np


experiments = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "pq2_mmq", experiments / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py")
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu, dispatch = component.gpu, component.dispatch
D, I, L, P, U = (getattr(gpu, k) for k in "DILPU")


def function(module, name):
    kernel = P()
    gpu.call("cuModuleGetFunction", ct.byref(kernel), module, name.encode())
    return kernel


def check_case(first, kernels, rng, events, queries=3):
    active = [first + t for t in range(queries)]
    tile_counts = [(keys + 255) // 256 for keys in active]
    max_tiles = tile_counts[-1]
    row_bytes = 24 * max_tiles * 258 * 4
    allocations = []

    def store(data):
        ptr = component.rec.Layer.stage(data)
        allocations.append(ptr)
        return ptr

    try:
        # Keep each row tightly packed by its own tile count, with padding after all heads.
        parts = np.zeros((queries, 24 * max_tiles * 258), dtype="<f4")
        for t, tiles in enumerate(tile_counts):
            row = parts[t, :24 * tiles * 258].reshape(24, tiles, 258)
            row[:, :, 0] = rng.normal(3, 4, (24, tiles))
            row[:, :, 1] = rng.uniform(.25, 16, (24, tiles))
            row[:, :, 2:] = rng.normal(0, .7, (24, tiles, 256))
            row[0, :, 1] = 0.0
            row[1, ::5, 1] = -0.0
            row[2, :, 0] = np.where(np.arange(tiles) % 2, -0.0, 0.0)
        scratch = store(parts.tobytes())
        output = store(bytes(queries * 24576))
        configs = [
            store(bytes((L * 22)(256, 256, 1, 24, 4, 4, keys, 24576, 1024, 576, 144, 576, 144,
                                0, 1024, 24576, 256, tiles, 24, 24, 24 * tiles * 258 * 4, keys)))
            for keys, tiles in zip(active, tile_counts)
        ]

        def run(mode):
            if mode != "serial":
                dispatch(kernels[mode], (24 * queries, 1), 128, (scratch, output, U(first), U(max_tiles)))
                return
            for t, cfg in enumerate(configs):
                dispatch(kernels["serial"], (24, 1), 32,
                         (D(scratch.value + t * row_bytes), D(output.value + t * 24576), cfg, I(0)))

        run("serial")
        expected = bytes(gpu.download(output, queries * 6144))
        for mode in kernels.keys() - {"serial"}:
            run(mode)
            assert bytes(gpu.download(output, queries * 6144)) == expected, (first, mode)

        samples = {mode: [] for mode in kernels}
        order = tuple(kernels)
        for mode in (*order, *reversed(order)) * 3:
            def repeat():
                for _ in range(20):
                    run(mode)
            samples[mode].append(component.elapsed(repeat, *events) / 20)
        return {
            "keys": active, "exact_output_values": queries * 6144,
            "median_ms": {mode: median(values) for mode, values in samples.items()}, "samples_ms": samples,
        }
    finally:
        for ptr in reversed(allocations):
            gpu.call("cuMemFree_v2", ptr)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        image, frozen = (component.rec.Layer.load(Path(path).read_bytes()) for path in sys.argv[3:5])
        kernels = {
            "serial": function(frozen, "goose_q4_attention_merge"),
            "parallel": function(image, "bonsai_verify_merge_parallel"),
        }
        queries = 1 if sys.argv[5:] == ["--single-query"] else 3
        if queries == 3:
            kernels = {"serial": kernels["serial"],
                       "direct": function(image, "bonsai_verify_merge_direct"),
                       "parallel": kernels["parallel"]}
        rng = np.random.default_rng(2036)
        events = component.cuda_event(), component.cuda_event()
        print(json.dumps([check_case(n, kernels, rng, events, queries)
                          for n in (1, 255, 256, 8192, 32767, 32768, 33279)], indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
