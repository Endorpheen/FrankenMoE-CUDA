"""Compare three ordered Q4 queries with the original single-query kernel."""
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
D, F, I, L, P, U = (getattr(gpu, name) for name in "DFILPU")


def function(module, name):
    kernel = P()
    gpu.call("cuModuleGetFunction", ct.byref(kernel), module, name.encode())
    return kernel


def check_case(first, kernels, rng, events):
    active = [first + t for t in range(3)]
    tile_counts = [(keys + 255) // 256 for keys in active]
    max_tiles = tile_counts[-1]
    row_bytes = 24 * max_tiles * 258 * 4
    allocations = []

    def store(data):
        ptr = component.rec.Layer.stage(data)
        allocations.append(ptr)
        return ptr

    def allocate(size):
        ptr = gpu.allocate(size)
        allocations.append(ptr)
        return ptr

    try:
        queries = store(rng.normal(0, .7, (3, 24, 256)).astype("<f4").tobytes())

        def quantized():
            blocks = rng.integers(0, 256, (active[-1] * 32, 18), dtype=np.uint8)
            scales = rng.uniform(-.25, .25, blocks.shape[0]).astype("<f2")
            blocks[:, :2] = scales.view(np.uint8).reshape(-1, 2)
            return store(blocks.tobytes())

        k, v = quantized(), quantized()
        scratch, output = allocate(3 * row_bytes), allocate(3 * 24576)
        configs = [
            store(bytes((L * 22)(256, 256, 1, 24, 4, 4, keys, 24576, 1024, 576, 144, 576, 144,
                                0, 1024, 24576, 256, tiles, 24, 24, 24 * tiles * 258 * 4, keys)))
            for keys, tiles in zip(active, tile_counts)
        ]

        def run(mode):
            if mode != "serial":
                heads = int(mode[-1])
                dispatch(kernels[mode], (24 // heads, max_tiles), 32,
                         (queries, k, v, scratch, U(first), U(max_tiles), F(1 / 16)))
            for t, (keys, tiles) in enumerate(zip(active, tile_counts)):
                partial = D(scratch.value + t * row_bytes)
                result = D(output.value + t * 24576)
                if mode == "serial":
                    heads = 3 if keys >= 32768 else 2
                    dispatch(kernels[f"single{heads}"], (24 // heads, tiles), 32,
                             (D(queries.value + t * 24576), k, v, partial, U(keys), U(tiles), F(1 / 16)))
                dispatch(kernels["merge"], (24, 1), 32, (partial, result, configs[t], I(0)))

        def capture():
            output_rows = bytes(gpu.download(output, 3 * 6144))
            assert np.isfinite(np.frombuffer(output_rows, dtype="<f4")).all()
            partial_rows = tuple(bytes(gpu.download(D(scratch.value + t * row_bytes), 24 * tiles * 258))
                                 for t, tiles in enumerate(tile_counts))
            return output_rows, partial_rows

        run("serial")
        expected = capture()
        for mode in ("shared1", "shared2"):
            run(mode)
            assert capture() == expected, (first, mode)
        samples = {mode: [] for mode in ("serial", "shared1", "shared2")}
        for mode in ("serial", "shared1", "shared2", "shared2", "shared1", "serial") * 3:
            def repeat():
                for _ in range(5):
                    run(mode)
            samples[mode].append(component.elapsed(repeat, *events) / 5)
        return {
            "keys": active, "exact_output_rows": 3, "exact_tile_values": sum(tile_counts) * 24 * 258,
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
            "single2": function(image, "bonsai_decode_gqa2"),
            "single3": function(image, "bonsai_decode_gqa3"),
            "shared1": function(image, "bonsai_verify_q4_gqa1"),
            "shared2": function(image, "bonsai_verify_q4_gqa2"),
            "merge": function(frozen, "goose_q4_attention_merge"),
        }
        rng = np.random.default_rng(2033)
        events = component.cuda_event(), component.cuda_event()
        print(json.dumps([check_case(n, kernels, rng, events)
                          for n in (1, 255, 256, 8192, 32767, 32768, 33279)], indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
