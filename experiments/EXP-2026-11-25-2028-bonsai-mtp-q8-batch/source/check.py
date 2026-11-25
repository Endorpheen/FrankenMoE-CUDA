"""Compare the three-token kernel with three independent single-token projections."""
import ctypes as ct
from hashlib import sha256
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
D, I, P, U = gpu.D, gpu.I, gpu.P, gpu.U
TOKENS = 3


def check(matrix, width, rows, grouping, kernels, events, rng):
    x = rng.normal(0, .7, (TOKENS, width)).astype(np.float32)
    x[0, :32] = 0
    x[1, 32:40] = [127, -127, .5, -.5, 1.5, -1.5, 63.5, -63.5]
    x[2] *= np.float32(.03125)
    weights = component.rec.Layer.stage(matrix)
    inputs = component.rec.Layer.stage(x.tobytes())
    groups = width // 32
    packet_stride, output_stride = groups * 36, rows * 4
    packet = gpu.allocate(TOKENS * packet_stride)
    output = gpu.allocate(TOKENS * output_stride)
    row_tile = 4 * grouping
    projection_grid = ((rows + row_tile - 1) // row_tile, 1)

    def serial():
        for t in range(TOKENS):
            activation = D(inputs.value + t * width * 4)
            packed = D(packet.value + t * packet_stride)
            result = D(output.value + t * output_stride)
            dispatch(kernels["pack"], ((groups + 3) // 4, 1), 128, (activation, packed, U(groups)))
            dispatch(kernels[f"single{grouping}"], projection_grid, 128,
                     (weights, packed, result, U(width), U(rows)))

    def shared():
        dispatch(kernels["pack"], ((TOKENS * groups + 3) // 4, 1), 128,
                 (inputs, packet, U(TOKENS * groups)))
        dispatch(kernels[f"shared{grouping}"], projection_grid, 128,
                 (weights, packet, output, U(width), U(rows)))

    def snapshot(ptr, size):
        host = (ct.c_ubyte * size)()
        gpu.call("cuMemcpyDtoH_v2", host, ptr, gpu.S(size))
        return bytes(host)

    try:
        serial()
        expected = snapshot(output, TOKENS * output_stride)
        expected_pack = snapshot(packet, TOKENS * packet_stride)
        shared()
        assert expected == snapshot(output, TOKENS * output_stride)
        assert expected_pack == snapshot(packet, TOKENS * packet_stride)
        assert np.isfinite(np.frombuffer(expected, dtype="<f4")).all()

        samples = {"serial": [], "shared": []}
        for name, run in (("serial", serial), ("shared", shared),
                          ("shared", shared), ("serial", serial)) * 3:
            def repeat():
                for _ in range(12):
                    run()
            samples[name].append(component.elapsed(repeat, *events) / 12)
        return {
            "width": width, "rows": rows, "grouping": grouping,
            "exact_packet_bytes": len(expected_pack), "exact_results": TOKENS * rows,
            "sha256": sha256(expected).hexdigest(),
            "component_median_ms": {name: median(values) for name, values in samples.items()},
            "samples_ms": samples,
        }
    finally:
        for ptr in (output, packet, inputs, weights):
            gpu.call("cuMemFree_v2", ptr)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        paths = {
            "pack": "bonsai_mv_pack_q8",
            "single1": "bonsai_pq2_mvq_q8_1",
            "single2": "bonsai_pq2_mvq_q8_2",
            "shared1": "bonsai_pq2_mvq_q8_three_1",
            "shared2": "bonsai_pq2_mvq_q8_three_2",
        }
        module = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        kernels = {}
        for label, name in paths.items():
            fn = P()
            gpu.call("cuModuleGetFunction", ct.byref(fn), module, name.encode())
            kernels[label] = fn
        events = component.cuda_event(), component.cuda_event()
        rng = np.random.default_rng(2028)
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        results = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
            for name, grouping, trim in (
                ("blk.0.attn_qkv.weight", 2, 1), ("blk.0.ffn_gate.weight", 2, 0),
                ("blk.0.ffn_down.weight", 2, 0), ("output.weight", 1, 0),
            ):
                tensor = tensors[name]
                width, rows = tensor["shape"]
                assert tensor["type"] == 142
                rows -= trim
                start = gpu.inventory["data_offset"] + tensor["offset"]
                data = mapped[start:start + rows * (width // 128) * 34]
                results.append(dict(tensor=name, **check(data, width, rows, grouping, kernels, events, rng)))
        print(json.dumps(results, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
