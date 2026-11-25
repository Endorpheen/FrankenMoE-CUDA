"""Time the fused FFN tail and half2 down projection against a candidate."""

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
    "broadcast_mmq": "bonsai_pq2_half_unpack_half2_17408_broadcast",
    "clock_mmq": "bonsai_pq2_half_unpack_half2_17408_clock",
    "k128_mmq": "bonsai_pq2_half_unpack_half2_17408_k128",
    "q8_tail": "bonsai_ffn_tail_q8_panel",
    "q8_mmq": "bonsai_pq2_mmq_int8_down_n128_spread",
}
TOKENS, WIDTH, ROWS = 1024, 17408, 5120
PHASES = ("weight_reads", "activation_copy_issue", "unpack",
          "copy_wait_and_first_barrier", "fragment_load_mma_and_fp32", "final_barrier")


def run_case(kernels, raw, signs, rng, events, candidate):
    stage = component.rec.Layer.stage
    gate = rng.normal(0, .7, (TOKENS, WIDTH)).astype(np.float32)
    gate[0] = 0
    up = rng.normal(0, .7, (TOKENS, WIDTH)).astype(np.float32)
    weight, gate_device, up_device = stage(raw), stage(gate.tobytes()), stage(up.tobytes())
    packed = gpu.allocate(TOKENS * WIDTH * 2)
    output = gpu.allocate(TOKENS * ROWS * 4)
    shape = U(WIDTH), U(ROWS), U(TOKENS)
    grid = ROWS // 64, TOKENS // 128
    counter_bytes = grid[0] * grid[1] * 2 * len(PHASES) * 8
    counters = gpu.allocate(counter_bytes) if candidate == "clock" else None
    scales = gpu.allocate(TOKENS * (WIDTH//128) * 4) if candidate == "q8" else None

    def launch(label):
        tail = f"{label}_tail" if label in ("panel", "q8") else "token_tail"
        tail_args = gate_device, up_device, signs, packed
        if label == "q8":
            tail_args = (*tail_args, scales)
        component.dispatch(kernels[tail], (WIDTH // 1024, TOKENS),
                           256, (*tail_args, U(WIDTH)))
        arguments = ((weight, packed, scales, output, *shape) if label == "q8"
                     else (weight, packed, output, *shape))
        if label == "clock":
            arguments = (*arguments, counters)
        component.dispatch(kernels[f"{label}_mmq"], grid, 256, arguments,
                           shared_bytes=55296 if label == "k128" else 0)

    try:
        if counters is not None:
            gpu.call("cuMemsetD8_v2", counters, ct.c_uint8(0), gpu.S(counter_bytes))
        launch("token")
        expected = np.ctypeslib.as_array(gpu.download(output, TOKENS * ROWS)).copy()
        launch(candidate)
        actual = np.ctypeslib.as_array(gpu.download(output, TOKENS * ROWS))
        assert np.isfinite(expected).all() and not expected[:ROWS].any()
        bit_exact = actual.tobytes() == expected.tobytes()
        if candidate != "q8":
            assert bit_exact
        assert np.isfinite(actual).all() and not actual[:ROWS].any()
        delta = actual - expected
        relative_l2 = float(np.linalg.norm(delta) / np.linalg.norm(expected))
        max_abs = float(np.max(np.abs(delta)))
        timings = {"token": [], candidate: []}
        for label in ("token", candidate, candidate, "token") * 4:
            timings[label].append(component.elapsed(lambda: launch(label), *events))
        before, after = map(median, (timings["token"], timings[candidate]))
        result = {
            "width": WIDTH, "rows": ROWS, "tokens": TOKENS, "bit_exact": bit_exact,
            "relative_l2": relative_l2, "max_abs": max_abs,
            "raw_ms": timings, "token_median_ms": before, f"{candidate}_median_ms": after,
            "time_reduction_pct": (1 - after / before) * 100,
        }
        if counters is not None:
            clocks = np.frombuffer(bytes(gpu.download(counters, counter_bytes // 4)), dtype="<u8")
            clocks = clocks.reshape(grid[1], grid[0], 2, len(PHASES))
            assert np.count_nonzero(np.any(clocks, axis=(2, 3))) == 40
            active = clocks[::2, ::8].reshape(40, 2, len(PHASES))
            assert np.all(active.sum(axis=2) > 0)
            result["sampled_ctas"] = len(active)
            result["sampled_clock_cycles"] = active.tolist()
            for step, parity in enumerate(("even", "odd")):
                elapsed = active[:, step]
                total = elapsed.sum(axis=0)
                result[f"{parity}_phase_clock_share"] = dict(zip(PHASES, (total / total.sum()).tolist()))
        return result
    finally:
        for temporary in (counters, scales):
            if temporary is not None:
                gpu.call("cuMemFree_v2", temporary)
        for pointer in (output, packed, up_device, gate_device, weight):
            gpu.call("cuMemFree_v2", pointer)


def main():
    candidate = sys.argv[4] if len(sys.argv) > 4 else "panel"
    if candidate not in ("panel", "broadcast", "clock", "k128", "q8"):
        raise ValueError(f"Unknown candidate: {candidate}")
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        module = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        kernels = {}
        labels = ["token_tail", "token_mmq", f"{candidate}_mmq"]
        if candidate in ("panel", "q8"):
            labels.append(f"{candidate}_tail")
        for label in labels:
            fn = P()
            gpu.call("cuModuleGetFunction", ct.byref(fn), module, NAMES[label].encode())
            kernels[label] = fn
        if candidate == "k128":
            gpu.call("cuFuncSetAttribute", kernels["k128_mmq"], I(8), I(55296))
            for label, shared in (("token", 0), ("k128", 55296)):
                blocks = I()
                gpu.call("cuOccupancyMaxActiveBlocksPerMultiprocessor",
                         ct.byref(blocks), kernels[f"{label}_mmq"], I(256), gpu.S(shared))
                print(f"{label}: {blocks.value} max active CTAs per SM", file=sys.stderr)
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
                results.append({"tensor": name, **run_case(kernels, raw, signs, rng, events, candidate)})
        print(json.dumps(results, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
