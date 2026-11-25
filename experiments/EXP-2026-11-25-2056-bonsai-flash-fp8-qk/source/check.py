"""Compare E4M3 QK to the existing FP16 QK fused Flash chain."""

import ctypes as ct
import importlib.util
import json
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
    first, out_count = k - n + 1, n * 24 * 256
    allocations = []

    def alloc(size):
        pointer = gpu.allocate(size)
        allocations.append(pointer)
        return pointer

    def fill(dest, values):
        data = values.tobytes()
        gpu.upload(dest, (ct.c_ubyte * len(data)).from_buffer_copy(data))

    def panel(kernel, grid_x, args, arena):
        params = (P * len(args))(*(ct.addressof(arg) for arg in args))
        gpu.call("cuLaunchKernel", kernel, U(grid_x), U(1), U(1),
                 U(256), U(1), U(1), U(arena), P(), params, P())

    try:
        query, output = alloc(out_count * 4), alloc(out_count * 4)
        fill(query, rng.standard_normal(out_count, dtype=np.float32))
        key, value = (alloc(k * 576) for _ in range(2))
        pk, pv = (alloc(chunks * 4 * 16384) for _ in range(2))
        source = alloc(k * 4096)
        for dest in (key, value):
            fill(source, rng.standard_normal(k * 1024, dtype=np.float32) * .6)
            component.dispatch(kernels["row"], ((k * 32 + 127) // 128, 1),
                               128, (source, dest, D(4096), U(32), D(k * 32)))
        component.dispatch(kernels["pack"], (chunks, 4), 128, (key, value, pk, pv, U(k), U(chunks)))
        args = (query, pk, pv, output, U(n), U(k), U(first), U(tiles), U(0), U(chunks))
        panel(kernels["reference"], groups // 2, args, 50176)
        expected = np.ctypeslib.as_array(gpu.download(output, out_count)).copy()
        assert np.isfinite(expected).all()

        component.dispatch(kernels["pack_k"], (chunks, 4), 128, (key, pk, U(k), U(chunks)))
        component.dispatch(kernels["pack_v"], (chunks, 4), 128, (value, pv, U(k), U(chunks)))
        gpu.call("cuMemsetD8_v2", output, U(255), S(out_count*4))
        panel(kernels["fp8"], groups // 2, args, 41472)
        observed = np.ctypeslib.as_array(gpu.download(output, out_count))
        assert np.isfinite(observed).all()
        delta = observed - expected
        denominator = np.linalg.norm(expected.reshape(-1, 256), axis=1)
        row_l2 = np.linalg.norm(delta.reshape(-1, 256), axis=1) / denominator
        return {"keys": k, "queries": n, "values": out_count,
                "relative_l2": float(np.linalg.norm(delta) / np.linalg.norm(expected)),
                "p95_row_l2": float(np.percentile(row_l2, 95)),
                "max_abs": float(np.max(np.abs(delta)))}
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
            "reference": "bonsai_flash_kv_fused_ordered",
            "pack_k": "bonsai_flash_stage_k_fp8",
            "pack_v": "bonsai_flash_stage_v_only",
            "fp8": "bonsai_flash_fp8_qk",
        }
        kernels = {}
        for label, name in names.items():
            function = P()
            gpu.call("cuModuleGetFunction", ct.byref(function),
                     frozen if label == "row" else image, name.encode())
            if label in ("reference", "fp8"):
                gpu.call("cuFuncSetAttribute", function, I(8), I(50176 if label == "reference" else 41472))
            kernels[label] = function
        rng = np.random.default_rng(2056)
        cases = ((257, 31), (8192, 1024), (32767, 1023))
        print(json.dumps([run_case(k, n, kernels, rng) for k, n in cases], indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
