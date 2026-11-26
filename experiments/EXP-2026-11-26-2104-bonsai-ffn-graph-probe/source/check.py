"""Measure CUDA graph replay for the exact Bonsai FFN decode launch sequence."""

import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
from statistics import median
import sys
import time

import numpy as np


source = (Path(__file__).resolve().parents[2] /
          "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py")
spec = importlib.util.spec_from_file_location("component", source)
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
F, I, P, U = gpu.F, gpu.I, gpu.P, gpu.U
HIDDEN, FFN, LAYERS = 5120, 17408, 16
NAMES = {
    "norm": "bonsai_norm_wide_512_1", "rotate": "bonsai_fwht1024",
    "pack": "bonsai_mv_pack_q8", "matrix": "bonsai_pq2_mvq_q8_2",
    "swiglu": "bonsai_swiglu", "add": "bonsai_residual",
}


class Dispatch(ct.Structure):
    _fields_ = [("function", P), ("grid_x", U), ("threads", U), ("params", ct.POINTER(P))]


def prepare(image, weights):
    module = component.rec.Layer.load(image)
    functions = {}
    for key, name in NAMES.items():
        fn = P()
        gpu.call("cuModuleGetFunction", ct.byref(fn), module, name.encode())
        functions[key] = fn

    sign_widths = gpu.inventory["metadata"]["prism.hadamard.sign_widths"]
    signs = gpu.inventory["metadata"]["prism.hadamard.sign_values"]
    sign_device = {}
    for width in (HIDDEN, FFN):
        offset = sum(sign_widths[:sign_widths.index(width)])
        sign_device[width] = component.rec.Layer.stage(
            np.asarray(signs[offset:offset+width], dtype="<i4").tobytes())

    tensors = {r["name"]: r for r in gpu.inventory["tensors"]}
    def tensor(layer, name, elements, quant_type):
        item = tensors[f"blk.{layer}.{name}.weight"]
        assert item["type"] == quant_type and np.prod(item["shape"]) == elements
        offset = gpu.inventory["data_offset"]+item["offset"]
        size = elements*4 if quant_type == 0 else (elements//128)*34
        return component.rec.Layer.stage(weights[offset:offset+size])

    first = gpu.allocate(HIDDEN*4)
    output = [gpu.allocate(HIDDEN*4), gpu.allocate(HIDDEN*4)]
    norm, rotated = (gpu.allocate(HIDDEN*4) for _ in range(2))
    gate, up, product, product_rotated = (gpu.allocate(FFN*4) for _ in range(4))
    packed = gpu.allocate((FFN//32)*36)
    down = gpu.allocate(HIDDEN*4)
    eps = F(gpu.inventory["metadata"]["qwen35.attention.layer_norm_rms_epsilon"])
    nodes, arguments = [], []
    def add(key, grid, threads, *args):
        arguments.append(args)
        params = (P * len(args))(*(ct.addressof(arg) for arg in args))
        nodes.append(Dispatch(functions[key], grid, threads, params))

    initial = first
    for layer in range(LAYERS):
        weight_norm = tensor(layer, "post_attention_norm", HIDDEN, 0)
        weights_gate, weights_up, weights_down = [
            tensor(layer, f"ffn_{part}", HIDDEN*FFN, 142) for part in ("gate", "up", "down")]
        add("norm", 1, 512, initial, weight_norm, norm, eps)
        add("rotate", HIDDEN//1024, 256, norm, sign_device[HIDDEN], rotated,
            U(0), U(0), U(0), U(1), U(HIDDEN))
        add("pack", HIDDEN//128, 128, rotated, packed, U(HIDDEN//32))
        add("matrix", FFN//8, 128, weights_gate, packed, gate, U(HIDDEN), U(FFN))
        add("matrix", FFN//8, 128, weights_up, packed, up, U(HIDDEN), U(FFN))
        add("swiglu", FFN//256, 256, gate, up, product, U(FFN))
        add("rotate", FFN//1024, 256, product, sign_device[FFN], product_rotated,
            U(0), U(0), U(0), U(1), U(FFN))
        add("pack", FFN//128, 128, product_rotated, packed, U(FFN//32))
        add("matrix", HIDDEN//8, 128, weights_down, packed, down, U(FFN), U(HIDDEN))
        last = output[layer%2]
        add("add", HIDDEN//256, 256, initial, down, last)
        initial = last
    return (Dispatch * len(nodes))(*nodes), arguments, first, initial


def test(image, native):
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as weights:
            nodes, arguments, first, last = prepare(image, weights)
        assert len(arguments) == len(nodes)
        library = ct.CDLL(str(native.resolve()))
        issue, issue_graphs = library.issue_ffn, library.issue_graphs
        issue_graphs.argtypes = [ct.POINTER(P), gpu.S, P]
        issue_graphs.restype = I
        issue.argtypes = [ct.POINTER(Dispatch), gpu.S, P]
        issue.restype = I
        stream = P()
        gpu.call("cuStreamCreate", ct.byref(stream), U(1))
        start, stop = component.cuda_event(), component.cuda_event()

        def direct(offset=0, count=len(nodes)):
            view = ct.cast(ct.byref(nodes, offset * ct.sizeof(Dispatch)), ct.POINTER(Dispatch))
            status = issue(view, count, stream)
            if status:
                raise RuntimeError(f"Uncaptured sequence: CUDA {status}")

        def capture(offset, expected_count):
            graph, executable = P(), P()
            start_ns = time.perf_counter_ns()
            gpu.call("cuStreamBeginCapture_v2", stream, I(0))
            direct(offset, expected_count)
            gpu.call("cuStreamEndCapture", stream, ct.byref(graph))
            capture_us = (time.perf_counter_ns()-start_ns)/1000
            count = gpu.S()
            gpu.call("cuGraphGetNodes", graph, P(), ct.byref(count))
            assert count.value == expected_count
            start_ns = time.perf_counter_ns()
            gpu.call("cuGraphInstantiateWithFlags", ct.byref(executable), graph, ct.c_uint64(0))
            instantiate_us = (time.perf_counter_ns()-start_ns)/1000
            start_ns = time.perf_counter_ns()
            gpu.call("cuGraphUpload", executable, stream)
            gpu.call("cuStreamSynchronize", stream)
            upload_us = (time.perf_counter_ns()-start_ns)/1000
            return graph, executable, dict(capture_us=capture_us,
                                           instantiate_us=instantiate_us, upload_ready_us=upload_us)

        rng = np.random.default_rng(2104)
        vectors = [rng.normal(0, .1, HIDDEN).astype("<f4") for _ in range(12)]
        def input_vector(values):
            raw = values.tobytes()
            gpu.upload(first, (ct.c_ubyte * len(raw)).from_buffer_copy(raw))
            # The pageable upload uses the default stream. The nonblocking
            # compute stream must only run after that upload finishes.
            gpu.call("cuCtxSynchronize")

        input_vector(vectors[0])
        direct()
        gpu.call("cuStreamSynchronize", stream)

        chain_graph, chain_exec, chain_setup = capture(0, len(nodes))
        per_layer = len(nodes) // LAYERS
        split = [capture(layer*per_layer, per_layer) for layer in range(LAYERS)]
        graph_list = (P * LAYERS)(*(entry[1] for entry in split))

        def replay():
            gpu.call("cuGraphLaunch", chain_exec, stream)

        def replay_layers():
            status = issue_graphs(graph_list, LAYERS, stream)
            if status:
                raise RuntimeError(f"Split graph replay: CUDA {status}")

        distinct_outputs = []
        for values in vectors:
            input_vector(values)
            direct()
            gpu.call("cuStreamSynchronize", stream)
            expected = bytes(gpu.download(last, HIDDEN))
            assert np.isfinite(np.frombuffer(expected, dtype="<f4")).all()
            for label, method in (("chain", replay), ("per_layer", replay_layers)):
                method()
                gpu.call("cuStreamSynchronize", stream)
                actual = bytes(gpu.download(last, HIDDEN))
                if actual != expected:
                    ref, got = (np.frombuffer(blob, dtype="<f4") for blob in (expected, actual))
                    raise ValueError(f"{label} replay differs for {np.count_nonzero(ref != got)} values")
            distinct_outputs.append(expected)
        assert distinct_outputs[0] != distinct_outputs[1]

        measurements = {"direct": [], "graph": [], "per_layer_graph": []}
        methods = {"direct": direct, "graph": replay, "per_layer_graph": replay_layers}
        order = tuple(methods)
        for label in (*order, *reversed(order)) * 12:
            host_start = time.perf_counter_ns()
            gpu.call("cuEventRecord", start, stream)
            enqueue_start = time.perf_counter_ns()
            methods[label]()
            enqueue_us = (time.perf_counter_ns()-enqueue_start)/1000
            gpu.call("cuEventRecord", stop, stream)
            gpu.call("cuEventSynchronize", stop)
            wall_us = (time.perf_counter_ns()-host_start)/1000
            gpu_ms = F()
            gpu.call("cuEventElapsedTime", ct.byref(gpu_ms), start, stop)
            measurements[label].append(dict(enqueue_us=enqueue_us, wall_us=wall_us, gpu_us=gpu_ms.value*1000))

        for graph, executable, _ in [(chain_graph, chain_exec, chain_setup), *split]:
            gpu.call("cuGraphExecDestroy", executable)
            gpu.call("cuGraphDestroy", graph)
        gpu.call("cuStreamDestroy_v2", stream)
        return {
            "layers": LAYERS, "graph_nodes": len(nodes), "input_vectors_bit_exact": len(vectors),
            "chain_setup_us": chain_setup,
            "per_layer_total_setup_us": {key:sum(entry[2][key] for entry in split) for key in chain_setup},
            "medians_us": {label:{key:median(row[key] for row in samples) for key in samples[0]}
                           for label, samples in measurements.items()}, "samples": measurements,
        }
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    print(json.dumps(test(Path(sys.argv[3]).read_bytes(), Path(sys.argv[4])), indent=2))
