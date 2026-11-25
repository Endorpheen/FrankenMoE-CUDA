"""Time the fused FFN tail and half2 down projection with both layouts."""

import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
from statistics import median
import sys

import numpy as np


source = Path(__file__).resolve().parents[2] / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py"
spec = importlib.util.spec_from_file_location("component", source)
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
I, P, U = gpu.I, gpu.P, gpu.U
NAMES = {
    "token_tail": "bonsai_ffn_tail",
    "token_mmq": "bonsai_pq2_half_unpack_half2_17408",
    "panel_tail": "bonsai_ffn_tail_panel128",
    "panel_mmq": "bonsai_pq2_half_unpack_half2_17408_panel",
}
TOKENS, WIDTH, ROWS = 1024, 17408, 5120


def run_case(kernels, raw, signs, rng, events):
    stage = component.rec.Layer.stage
    gate = rng.normal(0, .7, (TOKENS, WIDTH)).astype(np.float32)
    gate[0] = 0
    up = rng.normal(0, .7, (TOKENS, WIDTH)).astype(np.float32)
    weight, gate_device, up_device = stage(raw), stage(gate.tobytes()), stage(up.tobytes())
    packed = gpu.allocate(TOKENS * WIDTH * 2)
    output = gpu.allocate(TOKENS * ROWS * 4)
    shape = U(WIDTH), U(ROWS), U(TOKENS)

    def launch(label):
        component.dispatch(kernels[f"{label}_tail"], (WIDTH // 1024, TOKENS),
                           256, (gate_device, up_device, signs, packed, U(WIDTH)))
        component.dispatch(kernels[f"{label}_mmq"], (ROWS // 64, TOKENS // 128),
                           256, (weight, packed, output, *shape))

    try:
        launch("token")
        expected = np.ctypeslib.as_array(gpu.download(output, TOKENS * ROWS)).copy()
        launch("panel")
        actual = np.ctypeslib.as_array(gpu.download(output, TOKENS * ROWS))
        assert np.isfinite(expected).all() and not expected[:ROWS].any()
        assert actual.tobytes() == expected.tobytes()
        timings = {"token": [], "panel": []}
        for label in ("token", "panel", "panel", "token") * 4:
            timings[label].append(component.elapsed(lambda: launch(label), *events))
        before, after = map(median, (timings["token"], timings["panel"]))
        return {
            "width": WIDTH, "rows": ROWS, "tokens": TOKENS, "bit_exact": True,
            "raw_ms": timings, "token_median_ms": before, "panel_median_ms": after,
            "time_reduction_pct": (1 - after / before) * 100,
        }
    finally:
        for pointer in (output, packed, up_device, gate_device, weight):
            gpu.call("cuMemFree_v2", pointer)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        module = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        kernels = {}
        for label, name in NAMES.items():
            fn = P()
            gpu.call("cuModuleGetFunction", ct.byref(fn), module, name.encode())
            kernels[label] = fn
        events = component.cuda_event(), component.cuda_event()
        inventory = gpu.inventory
        tensors = {tensor["name"]: tensor for tensor in inventory["tensors"]}
        widths = inventory["metadata"]["prism.hadamard.sign_widths"]
        values = inventory["metadata"]["prism.hadamard.sign_values"]
        start = sum(widths[:widths.index(WIDTH)])
        signs = component.rec.Layer.stage(np.asarray(values[start:start+WIDTH], dtype=np.int32).tobytes())
        rng = np.random.default_rng(2085)
        results = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
            for layer in (0, 21, 42, 63):
                name = f"blk.{layer}.ffn_down.weight"
                tensor = tensors[name]
                assert tensor["type"] == 142 and tensor["shape"] == [WIDTH, ROWS]
                offset = inventory["data_offset"] + tensor["offset"]
                raw = weights[offset:offset+ROWS*(WIDTH//128)*34]
                results.append({"tensor": name, **run_case(kernels, raw, signs, rng, events)})
        print(json.dumps(results, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
