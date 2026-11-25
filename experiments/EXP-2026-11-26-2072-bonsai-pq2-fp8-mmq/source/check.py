"""Cross-check E4M3 activations with an independent sampled CPU oracle."""

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
    activation = data[tt].reshape(-1, blocks, 128)
    scale = np.max(np.abs(activation), axis=-1) / np.float32(448.0)
    safe = np.where(scale > 0, scale, np.float32(1.0))[..., None]
    normalized = np.abs(activation / safe)
    codes = np.arange(127)
    exponent, mantissa = codes // 8, codes % 8
    values = np.where(exponent == 0, mantissa * 2.0**-9,
                      (1.0 + mantissa/8) * 2.0**(exponent-7)).astype(np.float32)
    high = np.searchsorted(values, normalized).clip(0, 126)
    low = np.maximum(0, high-1)
    left, right = normalized - values[low], values[high] - normalized
    rounded = np.where((left < right) | ((left == right) & (low % 2 == 0)), low, high)
    fp8 = np.copysign(values[rounded], activation)
    pq2 = np.frombuffer(weights, np.uint8).reshape(rows, blocks, 34)[rr]
    scales_w = pq2[..., :2].copy().view("<f2").reshape(-1, blocks).astype(np.float64)
    code = ((pq2[..., 2:, None] >> np.array([0, 2, 4, 6], np.uint8)) & 3)
    weight = code.reshape(-1, blocks, 128).astype(np.int32) - 1
    dots = np.einsum("tbk,rbk->trb", fp8, weight, dtype=np.float64)
    return tt, rr, (dots * scales_w[None] * scale[:, None]).sum(axis=-1)


def run_case(kernels, raw, width, rows, tokens, rng):
    stage = component.rec.Layer.stage
    data = rng.normal(0, .7, (tokens, width)).astype(np.float32)
    data[0] = 0
    weight, source = stage(raw), stage(data.tobytes())
    packed = gpu.allocate(tokens * width * 2)
    scales = gpu.allocate(tokens * (width // 128) * 4)
    output = gpu.allocate(tokens * rows * 4)
    try:
        pairs = tokens * width // 2
        groups = tokens * (width // 128)
        shape = U(width), U(rows), U(tokens)
        order = ["reference", 64, 128]
        if "--up-only-reverse" in sys.argv:
            order.reverse()
        outputs = {}
        for mode in order:
            if mode == "reference":
                component.dispatch(kernels["f16_pack"], ((pairs+255)//256, 1),
                                   256, (source, packed, U(pairs)))
                component.dispatch(kernels[f"stable_{width}"], ((rows+63)//64, (tokens+127)//128),
                                   256, (weight, packed, output, *shape))
            else:
                if mode == order[0] or outputs.keys() == {"reference"}:
                    component.dispatch(kernels["pack"], ((groups+3)//4, 1), 128,
                                       (source, packed, scales, U(groups)))
                component.dispatch(kernels[mode], ((rows+63)//64, (tokens+mode-1)//mode),
                                   256, (weight, packed, scales, output, *shape))
            outputs[mode] = np.ctypeslib.as_array(gpu.download(output, tokens*rows)).copy().reshape(tokens, rows)
        expected = outputs["reference"]
        candidate_results = {}
        tt, rr, cpu = cpu_samples(data, raw, rows, width)
        for tile in (64, 128):
            actual = outputs[tile]
            assert np.isfinite(actual).all() and not actual[0].any()
            sampled = actual[np.ix_(tt, rr)]
            np.testing.assert_allclose(sampled, cpu, rtol=3e-5, atol=2e-4)
            candidate_results[tile] = {
                "relative_l2_to_cumulative": float(np.linalg.norm(actual - expected) / np.linalg.norm(expected)),
                "max_abs_sampled_vs_cpu": float(np.max(np.abs(sampled - cpu))),
            }
        return {"width": width, "rows": rows, "tokens": tokens, "fp8": candidate_results}
    finally:
        for pointer in (output, scales, packed, source, weight):
            gpu.call("cuMemFree_v2", pointer)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        trial, reference = (component.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5])
        names = {
            "f16_pack": "bonsai_pack_half",
            "stable_5120": "bonsai_pq2_half_full_5120",
            "stable_6144": "bonsai_pq2_half_full_6144",
            "stable_17408": "bonsai_pq2_half_unpack_half2_17408",
            "pack": "bonsai_pack_pq2_fp8",
            64: "bonsai_pq2_mmq_fp8_n64",
            128: "bonsai_pq2_mmq_fp8_n128",
        }
        kernels = {}
        for label, name in names.items():
            fn = P()
            module = reference if label == "f16_pack" or str(label).startswith("stable_") else trial
            gpu.call("cuModuleGetFunction", ct.byref(fn), module, name.encode())
            kernels[label] = fn
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        rng = np.random.default_rng(2072)
        result = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
            for name in ("blk.0.attn_qkv.weight", "blk.0.ssm_out.weight",
                         "blk.0.ffn_up.weight", "blk.0.ffn_down.weight"):
                if "--up-only-reverse" in sys.argv and not name.endswith("ffn_up.weight"):
                    continue
                tensor = tensors[name]
                width, rows = tensor["shape"]
                assert tensor["type"] == 142 and rows % 64 == 0
                offset = gpu.inventory["data_offset"] + tensor["offset"]
                raw = weights[offset:offset+rows*(width//128)*34]
                result.append({"tensor": name, **run_case(kernels, raw, width, rows, 1024, rng)})
        print(json.dumps(result, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
