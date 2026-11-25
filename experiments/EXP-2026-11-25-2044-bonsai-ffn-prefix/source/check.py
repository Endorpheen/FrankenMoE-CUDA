"""Compare complete FFN prefix preparation, including packed outputs and residual."""
import ctypes as ct
import importlib.util
import json
import mmap
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
F, I, P, U = (getattr(gpu, name) for name in "FIPU")
WIDTH = 5120


def check(count, weight, signs, epsilon, kernels, rng, events, q8):
    length = count * WIDTH
    size = length * 4
    packed_bytes = length if q8 else size // 2
    groups = length // 128
    allocations = []

    def store(data):
        ptr = component.rec.Layer.stage(data)
        allocations.append(ptr)
        return ptr

    def allocate(n):
        ptr = gpu.allocate(n)
        allocations.append(ptr)
        return ptr

    def read(ptr, n):
        out = (ct.c_ubyte * n)()
        gpu.call("cuMemcpyDtoH_v2", out, ptr, gpu.S(n))
        return bytes(out)

    try:
        x = rng.standard_normal((2, length), dtype=np.float32)
        x[:, :WIDTH] = 0.0
        hidden, branch = (store(row.tobytes()) for row in x)
        residual, normal, rotated, packed = (allocate(n) for n in (size, size, size, packed_bytes))

        scales = allocate(groups * 4)

        def control():
            dispatch(kernels["add"], (20 * count, 1), 256, (hidden, branch, residual))
            small = count <= 512
            dispatch(kernels["norm_small" if small else "norm_large"], (count, 1), 512 if small else 128,
                     (residual, weight, normal, F(epsilon)))
            dispatch(kernels["rotate"], (5, count), 256,
                     (normal, signs, rotated, U(0), U(0), U(0), U(1), U(WIDTH)))
            if q8:
                dispatch(kernels["pack_q8"], ((groups + 3) // 4, 1), 128, (rotated, packed, scales, U(groups)))
            else:
                dispatch(kernels["pack"], ((length // 2 + 255) // 256, 1), 256,
                         (rotated, packed, U(length // 2)))

        def fused():
            args = (hidden, branch, weight, signs, residual, packed)
            if q8:
                dispatch(kernels["fused_q8"], (count, 1), 256, (*args, scales, F(epsilon)))
            else:
                dispatch(kernels["fused"], (count, 1), 256, (*args, F(epsilon)))

        def capture():
            return read(packed, packed_bytes), read(residual, size), read(scales, groups * 4) if q8 else b""

        control()
        expected = capture()
        fused()
        assert expected == capture(), (count, q8)
        samples = {name: [] for name in ("control", "fused")}
        for name, run in (("control", control), ("fused", fused), ("fused", fused), ("control", control)) * 3:
            def repeat():
                for _ in range(10):
                    run()
            samples[name].append(component.elapsed(repeat, *events) / 10)
        return dict(tokens=count, format="q8" if q8 else "fp16",
                    exact_packed_bytes=packed_bytes, exact_residual_bytes=size,
                    exact_scale_bytes=groups * 4 if q8 else 0,
                    median_ms={name: median(values) for name, values in samples.items()}, samples_ms=samples)
    finally:
        for ptr in reversed(allocations):
            gpu.call("cuMemFree_v2", ptr)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        module = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        kernels = {}
        for label, name in {
            "add": "bonsai_residual", "norm_small": "bonsai_norm_wide_512_1",
            "norm_large": "bonsai_norm_wide_128_0", "rotate": "bonsai_fwht1024",
            "pack": "bonsai_pack_half", "fused": "bonsai_ffn_prefix_fp16",
            "pack_q8": "bonsai_pack_q8_128", "fused_q8": "bonsai_ffn_prefix_q8",
        }.items():
            f = P()
            gpu.call("cuModuleGetFunction", ct.byref(f), module, name.encode())
            kernels[label] = f
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        meta = gpu.inventory["metadata"]
        signs = component.rec.Layer.stage(np.asarray(meta["prism.hadamard.sign_values"][:WIDTH], dtype="<i4").tobytes())
        epsilon = meta["qwen35.attention.layer_norm_rms_epsilon"]
        events = component.cuda_event(), component.cuda_event()
        rng = np.random.default_rng(2044)
        result = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
            for layer in (0, 3):
                name = f"blk.{layer}.post_attention_norm.weight"
                tensor = tensors[name]
                assert tensor["type"] == 0 and tensor["shape"] == [WIDTH]
                at = gpu.inventory["data_offset"] + tensor["offset"]
                weight = component.rec.Layer.stage(mapped[at:at + WIDTH * 4])
                try:
                    for count in (128, 257, 512, 513, 1023, 1024):
                        for q8 in (False, True):
                            result.append(dict(layer=layer, **check(count, weight, signs, epsilon, kernels, rng, events, q8)))
                finally:
                    gpu.call("cuMemFree_v2", weight)
        print(json.dumps(result, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
