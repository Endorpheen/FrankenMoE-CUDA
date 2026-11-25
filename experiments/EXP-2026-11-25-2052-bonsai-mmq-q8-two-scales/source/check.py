"""Check both Q8-64 tile widths against independent CPU and FP16 references."""

import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
import sys

import numpy as np


path = Path(__file__).resolve().parents[2] / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py"
spec = importlib.util.spec_from_file_location("component", path)
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
I, P, U = gpu.I, gpu.P, gpu.U


def cpu_samples(data, weights, rows, width):
    blocks = width // 128
    rr = np.unique([0, 7, 8, 15, 16, 31, 32, 63, 64, rows // 2, rows - 1])
    tt = np.unique(np.linspace(0, len(data) - 1, 24, dtype=int))
    activation = data[tt].reshape(-1, blocks, 2, 64)
    scale = np.max(np.abs(activation), axis=-1) / np.float32(127.0)
    safe = np.where(scale > 0, scale, np.float32(1.0))[..., None]
    q8 = np.rint(activation / safe).clip(-127, 127).astype(np.int32)
    pq2 = np.frombuffer(weights, np.uint8).reshape(rows, blocks, 34)[rr]
    scales_w = pq2[..., :2].copy().view("<f2").reshape(-1, blocks).astype(np.float64)
    code = ((pq2[..., 2:, None] >> np.array([0, 2, 4, 6], np.uint8)) & 3)
    weight = code.reshape(-1, blocks, 2, 64).astype(np.int32) - 1
    dots = np.einsum("tbhk,rbhk->trbh", q8, weight, dtype=np.int32)
    return tt, rr, (dots * scales_w[None, :, :, None] * scale[:, None]).sum(axis=(-1, -2))


def run_case(kernels, raw, width, rows, tokens, rng):
    stage = component.rec.Layer.stage
    data = rng.normal(0, .7, (tokens, width)).astype(np.float32)
    data[0] = 0
    weight, source = stage(raw), stage(data.tobytes())
    packed = gpu.allocate(tokens * width * 2)
    scales = gpu.allocate(tokens * (width // 64) * 4)
    output = gpu.allocate(tokens * rows * 4)
    try:
        pairs = tokens * width // 2
        groups = tokens * (width // 128)
        shape = U(width), U(rows), U(tokens)
        component.dispatch(kernels["f16_pack"], ((pairs+255)//256, 1),
                           256, (source, packed, U(pairs)))
        component.dispatch(kernels["f16"], ((rows+63)//64, (tokens+127)//128),
                           256, (weight, packed, output, *shape))
        expected = np.ctypeslib.as_array(gpu.download(output, tokens*rows)).copy().reshape(tokens, rows)
        component.dispatch(kernels["q8_pack"], ((groups+3)//4, 1), 128,
                           (source, packed, scales, U(groups)))
        component.dispatch(kernels["q8"], ((rows+63)//64, (tokens+63)//64), 256,
                           (weight, packed, scales, output, *shape))
        q8 = np.ctypeslib.as_array(gpu.download(output, tokens*rows)).reshape(tokens, rows)
        l2_q8 = float(np.linalg.norm(q8 - expected) / np.linalg.norm(expected))
        component.dispatch(kernels["pack"], ((2*groups+7)//8, 1), 128,
                           (source, packed, scales, U(2*groups)))
        candidate_results = {}
        timings_order = (64, 128)
        tt, rr, cpu = cpu_samples(data, raw, rows, width)
        for tile in timings_order:
            component.dispatch(kernels[tile], ((rows+63)//64, (tokens+tile-1)//tile),
                               256, (weight, packed, scales, output, *shape))
            actual = np.ctypeslib.as_array(gpu.download(output, tokens*rows)).reshape(tokens, rows)
            assert np.isfinite(actual).all() and not actual[0].any()
            sampled = actual[np.ix_(tt, rr)]
            np.testing.assert_allclose(sampled, cpu, rtol=3e-5, atol=2e-4)
            l2 = float(np.linalg.norm(actual - expected) / np.linalg.norm(expected))
            candidate_results[tile] = {
                "relative_l2_to_fp16": l2,
                "improves_one_scale": l2 < l2_q8,
                "max_abs_sampled_vs_cpu": float(np.max(np.abs(sampled - cpu))),
            }
        return {"width": width, "rows": rows, "tokens": tokens,
                "q8_128_relative_l2": l2_q8, "q8_64": candidate_results}
    finally:
        for pointer in (output, scales, packed, source, weight):
            gpu.call("cuMemFree_v2", pointer)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        mod = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        names = {
            "f16_pack": "bonsai_pack_half",
            "f16": "bonsai_pq2_cached_64x128_k64",
            "q8_pack": "bonsai_pack_q8_128",
            "q8": "bonsai_pq2_mmq_int8",
            "pack": "bonsai_pack_q8_64",
            64: "bonsai_pq2_mmq_q8_64_n64",
            128: "bonsai_pq2_mmq_q8_64_n128",
        }
        kernels = {}
        for label, name in names.items():
            fn = P()
            gpu.call("cuModuleGetFunction", ct.byref(fn), mod, name.encode())
            kernels[label] = fn
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        rng = np.random.default_rng(2052)
        result = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
            for name, tokens, trim in (
                ("blk.0.attn_qkv.weight", 129, 1),
                ("blk.0.attn_qkv.weight", 1024, 0),
                ("blk.0.ffn_up.weight", 1024, 0),
                ("blk.0.ffn_down.weight", 1024, 0),
            ):
                tensor = tensors[name]
                width, rows = tensor["shape"]
                assert tensor["type"] == 142
                rows -= trim
                offset = gpu.inventory["data_offset"] + tensor["offset"]
                raw = weights[offset:offset+rows*(width//128)*34]
                result.append({"tensor": name, **run_case(kernels, raw, width, rows, tokens, rng)})
        print(json.dumps(result, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
