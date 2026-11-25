"""Diagnostic dense FP16 GEMM using unchanged PQ2 operands."""

import ctypes as ct
import importlib.util
import json
import mmap
import os
from pathlib import Path
import sys

import numpy as np


path = Path(__file__).resolve().parents[2] / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py"
spec = importlib.util.spec_from_file_location("component", path)
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
I, P, U = gpu.I, gpu.P, gpu.U

OP_N, OP_T = 0, 1
CUDA_R_32F, CUDA_R_16F = 0, 2
COMPUTE_32F, DEFAULT_ALGO = 68, -1
DISALLOW_REDUCED_PRECISION_REDUCTION = 16


def blas(status):
    if status != 0:
        raise RuntimeError(f"cuBLAS error {status}")


def dense(library, handle, weight, packed, output, width, rows, tokens):
    one, zero = ct.c_float(1.0), ct.c_float(0.0)
    blas(library.cublasGemmEx(
        handle, I(OP_T), I(OP_N), I(rows), I(tokens), I(width), ct.byref(one),
        P(weight.value), I(CUDA_R_16F), I(width), P(packed.value), I(CUDA_R_16F), I(width),
        ct.byref(zero), P(output.value), I(CUDA_R_32F), I(rows), I(COMPUTE_32F), I(DEFAULT_ALGO),
    ))


def cpu_samples(data, weights, rows, width):
    rr = np.unique([0, 7, 8, 15, 16, 31, 32, 63, 64, rows // 2, rows - 1])
    tt = np.unique(np.linspace(0, len(data) - 1, 24, dtype=int))
    pq2 = np.frombuffer(weights, np.uint8).reshape(rows, width//128, 34)[rr]
    scales = pq2[..., :2].copy().view("<f2").reshape(-1, width//128)
    code = ((pq2[..., 2:, None] >> np.array([0, 2, 4, 6], np.uint8)) & 3)
    weight = (code.reshape(-1, width//128, 128).astype(np.int32) - 1) * scales[..., None]
    weight = weight.astype(np.float16).astype(np.float64).reshape(len(rr), width)
    activation = data[tt].astype(np.float16).astype(np.float64)
    return tt, rr, activation @ weight.T


def run_case(kernels, library, handle, raw, width, rows, tokens, rng):
    stage = component.rec.Layer.stage
    data = rng.normal(0, .7, (tokens, width)).astype(np.float32)
    data[0] = 0
    weight, source = stage(raw), stage(data.tobytes())
    packed = gpu.allocate(tokens * width * 2)
    expanded = gpu.allocate(rows * width * 2)
    output = gpu.allocate(tokens * rows * 4)
    try:
        pairs = tokens * width // 2
        component.dispatch(kernels["pack"], ((pairs+255)//256, 1),
                           256, (source, packed, U(pairs)))
        component.dispatch(kernels[width], ((rows+63)//64, (tokens+127)//128),
                           256, (weight, packed, output, U(width), U(rows), U(tokens)))
        expected = np.ctypeslib.as_array(gpu.download(output, tokens*rows)).copy().reshape(tokens, rows)
        component.dispatch(kernels["expand"], ((rows*width//4+255)//256, 1),
                           256, (weight, expanded, U(width), U(rows)))
        dense(library, handle, expanded, packed, output, width, rows, tokens)
        actual = np.ctypeslib.as_array(gpu.download(output, tokens*rows)).reshape(tokens, rows)
        assert np.isfinite(actual).all() and not actual[0].any()
        tt, rr, cpu = cpu_samples(data, raw, rows, width)
        sampled = actual[np.ix_(tt, rr)]
        np.testing.assert_allclose(sampled, cpu, rtol=3e-5, atol=2e-4)
        return {
            "width": width, "rows": rows, "tokens": tokens,
            "relative_l2_to_cumulative": float(np.linalg.norm(actual - expected) / np.linalg.norm(expected)),
            "max_abs_sampled_vs_cpu": float(np.max(np.abs(sampled - cpu))),
            "expanded_weight_bytes": rows*width*2,
        }
    finally:
        for pointer in (output, expanded, packed, source, weight):
            gpu.call("cuMemFree_v2", pointer)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    toolkit = Path(os.environ.get("CUDA_HOME", "/usr/local/cuda"))
    library = ct.CDLL(str(toolkit / "lib64/libcublas.so.12"))
    handle = P()
    try:
        blas(library.cublasCreate_v2(ct.byref(handle)))
        blas(library.cublasSetMathMode(handle, I(DISALLOW_REDUCED_PRECISION_REDUCTION)))
        version = I()
        blas(library.cublasGetVersion_v2(handle, ct.byref(version)))
        expansion, reference = (component.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5])
        names = {
            "pack": "bonsai_pack_half",
            5120: "bonsai_pq2_half_full_5120",
            17408: "bonsai_pq2_half_unpack_half2_17408",
            "expand": "bonsai_pq2_expand_half",
        }
        kernels = {}
        for label, name in names.items():
            fn = P()
            gpu.call("cuModuleGetFunction", ct.byref(fn),
                     expansion if label == "expand" else reference, name.encode())
            kernels[label] = fn
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        rng = np.random.default_rng(2075)
        result = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
            for name in ("blk.0.attn_qkv.weight", "blk.0.ffn_up.weight", "blk.0.ffn_down.weight"):
                tensor = tensors[name]
                width, rows = tensor["shape"]
                assert tensor["type"] == 142 and rows % 64 == 0
                offset = gpu.inventory["data_offset"] + tensor["offset"]
                raw = weights[offset:offset+rows*(width//128)*34]
                result.append({"tensor": name, **run_case(kernels, library, handle, raw, width, rows, 1024, rng)})
        print(json.dumps({"cublas_version": version.value, "cases": result}, indent=2))
    finally:
        if handle:
            blas(library.cublasDestroy_v2(handle))
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
