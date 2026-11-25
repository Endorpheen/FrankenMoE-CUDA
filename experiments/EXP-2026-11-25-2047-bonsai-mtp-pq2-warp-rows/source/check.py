"""Gate exact warp layouts with both warm and rotating real Bonsai weights."""
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
I, P, U = gpu.I, gpu.P, gpu.U
WORDS = {3: "three", 4: "four", 5: "five"}


def function(module, name):
    f = P()
    gpu.call("cuModuleGetFunction", ct.byref(f), module, name.encode())
    return f


def component_case(module, packet_kernel, raw_weights, width, rows, tokens, rng, events, control_group):
    data = rng.normal(0, .7, (tokens, width)).astype("<f4")
    data[0, :32] = 0.0
    data[1, 32:40] = [127, -127, .5, -.5, 1.5, -1.5, 63.5, -63.5]
    variants = {"control": (function(module, f"bonsai_pq2_mvq_q8_{WORDS[tokens]}_{control_group}"), control_group)}
    for unpack in ("permutation", "spread"):
        for group in (1, 2, 4):
            variants[f"{unpack}_{group}"] = function(module, f"bonsai_mvq_{tokens}_{unpack}_{group}"), group

    addresses = []
    try:
        for weights in raw_weights:
            addresses.append(component.rec.Layer.stage(weights))
        input_f32 = component.rec.Layer.stage(data.tobytes())
        addresses.append(input_f32)
        groups = tokens * (width // 32)
        packet = gpu.allocate(groups * 36)
        output = gpu.allocate(tokens * rows * 4)
        addresses.extend((packet, output))
        dispatch(packet_kernel, ((groups + 3) // 4, 1), 128, (input_f32, packet, U(groups)))

        def run(mode, weights):
            kernel, shape = variants[mode]
            dispatch(kernel, ((rows + 4*shape - 1) // (4*shape), 1), 128,
                     (weights, packet, output, U(width), U(rows)))

        run("control", addresses[0])
        expected = bytes(gpu.download(output, tokens * rows))
        assert np.isfinite(np.frombuffer(expected, dtype="<f4")).all()
        for mode in variants:
            run(mode, addresses[0])
            assert bytes(gpu.download(output, tokens * rows)) == expected, (width, rows, tokens, mode)

        measures = {}
        order = tuple(variants)
        for scenario, selected in {"warm": addresses[:1], "rotating": addresses[:len(raw_weights)]}.items():
            if scenario == "rotating" and len(selected) == 1:
                continue
            samples = {mode: [] for mode in variants}
            for mode in (order + order[::-1]) * 3:
                def repeat():
                    for _ in range(5):
                        for weights in selected:
                            run(mode, weights)
                samples[mode].append(component.elapsed(repeat, *events) / (5 * len(selected)))
            measures[scenario] = dict(samples_ms=samples, median_ms={mode: median(v) for mode, v in samples.items()})
        return dict(width=width, rows=rows, slots=len(raw_weights), weight_bytes=sum(map(len, raw_weights)),
                    tokens=tokens, exact_output_values=tokens * rows, measure=measures)
    finally:
        for ptr in reversed(addresses):
            gpu.call("cuMemFree_v2", ptr)


def check_unpack(module):
    output = gpu.allocate(256 * 4)
    try:
        dispatch(function(module, "bonsai_pq2_spread_check"), (1, 1), 256, (output,))
        observed = bytes(gpu.download(output, 256))
        expected = bytes((((code >> shift) & 3) - 1) & 255
                         for code in range(256) for shift in range(0, 8, 2))
        assert observed == expected
    finally:
        gpu.call("cuMemFree_v2", output)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        module = component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        check_unpack(module)
        packet = function(module, "bonsai_mv_pack_q8")
        events = component.cuda_event(), component.cuda_event()
        rng = np.random.default_rng(2047)
        tensors = {tensor["name"]: tensor for tensor in gpu.inventory["tensors"]}
        report = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
            for fixture, names, trim in (
                ("gate", [f"blk.{layer}.ffn_gate.weight" for layer in (0, 1, 42, 63)], 0),
                ("down", [f"blk.{layer}.ffn_down.weight" for layer in (0, 1, 42, 63)], 0),
                ("qkv_tail", [f"blk.{layer}.attn_qkv.weight" for layer in (0, 1, 2, 4)], 1),
                ("vocabulary", ["output.weight"], 0),
            ):
                width, rows = tensors[names[0]]["shape"]
                weights = []
                for name in names:
                    tensor = tensors[name]
                    assert tensor["type"] == 142 and tensor["shape"] == [width, rows]
                    at = gpu.inventory["data_offset"] + tensor["offset"]
                    weights.append(mapped[at:at+(rows - trim) * (width // 128) * 34])
                for tokens in (3, 4, 5):
                    result = component_case(module, packet, weights, width, rows - trim, tokens, rng, events,
                                            1 if fixture == "vocabulary" else 2)
                    report.append(dict(fixture=fixture, **result))
        print(json.dumps(dict(unpack_exact_codes=256, results=report), indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
