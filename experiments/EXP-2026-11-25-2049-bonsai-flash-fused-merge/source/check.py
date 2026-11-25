"""Check the fused ordered Flash output against the existing Q4 K/V chain."""

import ctypes as ct
import importlib.util
from pathlib import Path
import sys

import numpy as np


path = Path(__file__).resolve().parents[2] / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py"
spec = importlib.util.spec_from_file_location("component", path)
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
D, I, P, S, U = gpu.D, gpu.I, gpu.P, gpu.S, gpu.U


def run_case(k, n, kernels, rng):
    chunks, tiles = (k + 31) // 32, (k + 127) // 128
    per_head = (n * 6 + 15) // 16
    assert per_head % 2 == 0
    groups = 4 * per_head
    wave = ((32 << 20) // (tiles * 16 * 258 * 4)) & ~1
    first, out_count = k - n + 1, n * 24 * 256
    allocations = []

    def alloc(size):
        pointer = gpu.allocate(size)
        allocations.append(pointer)
        return pointer

    def fill(dest, values):
        data = values.tobytes()
        gpu.upload(dest, (ct.c_ubyte * len(data)).from_buffer_copy(data))

    def panel(kernel, grid_x, grid_y, args):
        params = (P * len(args))(*(ct.addressof(arg) for arg in args))
        gpu.call("cuLaunchKernel", kernel, U(grid_x), U(grid_y), U(1),
                 U(256), U(1), U(1), U(50176), P(), params, P())

    try:
        query, output, scratch = alloc(out_count * 4), alloc(out_count * 4), alloc(32 << 20)
        fill(query, rng.standard_normal(out_count, dtype=np.float32))
        key, value = (alloc(k * 576) for _ in range(2))
        pk, pv = (alloc(chunks * 4 * 16384) for _ in range(2))
        source = alloc(k * 4096)
        for dest in (key, value):
            fill(source, rng.standard_normal(k * 1024, dtype=np.float32) * .6)
            component.dispatch(kernels["row"], ((k * 32 + 127) // 128, 1),
                               128, (source, dest, D(4096), U(32), D(k * 32)))
        component.dispatch(kernels["pack"], (chunks, 4), 128, (key, value, pk, pv, U(k), U(chunks)))
        for start in range(0, groups, wave):
            count = min(wave, groups - start)
            panel(kernels["panel"], count // 2, (tiles + 1) // 2,
                  (query, pk, pv, scratch, U(n), U(k), U(first), U(tiles), U(start), U(chunks)))
            component.dispatch(kernels["merge"], (count * 16, 1), 128,
                               (scratch, output, U(n), U(first), U(tiles), U(start)))
        expected = bytes(gpu.download(output, out_count))
        assert np.isfinite(np.frombuffer(expected, dtype=np.float32)).all()
        gpu.call("cuMemsetD8_v2", output, U(255), S(out_count * 4))
        panel(kernels["fused"], groups // 2, 1,
              (query, pk, pv, output, U(n), U(k), U(first), U(tiles), U(0), U(chunks)))
        observed = bytes(gpu.download(output, out_count))
        assert observed == expected, (k, n, int(np.count_nonzero(
            np.frombuffer(observed, dtype=np.uint32) != np.frombuffer(expected, dtype=np.uint32))))
        print(f"{k} keys / {n} queries: {out_count:,} exact FP32 values", flush=True)
    finally:
        for pointer in reversed(allocations):
            gpu.call("cuMemFree_v2", pointer)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        frozen, image = (component.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5])
        names = {
            "row": "goose_set_q4_rows",
            "pack": "bonsai_flash_stage_kv",
            "merge": "bonsai_flash_compact_ordered_merge",
            "panel": "bonsai_flash_kv_lean2",
            "fused": "bonsai_flash_kv_fused_ordered",
        }
        kernels = {}
        for label, name in names.items():
            function = P()
            gpu.call("cuModuleGetFunction", ct.byref(function),
                     frozen if label == "row" else image, name.encode())
            if label in ("panel", "fused"):
                gpu.call("cuFuncSetAttribute", function, I(8), I(50176))
            kernels[label] = function
        rng = np.random.default_rng(2049)
        for keys, queries in ((257, 31), (8192, 1024), (32767, 1023)):
            run_case(keys, queries, kernels, rng)
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
