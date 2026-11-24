import ctypes as c
import json
from pathlib import Path
from statistics import median

import numpy as np

cuda = c.CDLL("libcuda.so.1")
ptr, u64, i32 = c.c_void_p, c.c_uint64, c.c_int


def call(name, *args):
    status = getattr(cuda, name)(*args)
    if status:
        raise RuntimeError(f"{name}: {status}")


def allocate(size):
    address = u64()
    call("cuMemAlloc_v2", c.byref(address), c.c_size_t(size))
    return address


def upload(array):
    address = allocate(array.nbytes)
    call("cuMemcpyHtoD_v2", address, ptr(array.ctypes.data), c.c_size_t(array.nbytes))
    return address


def load_function(module, entry):
    handle = ptr()
    call("cuModuleGetFunction", c.byref(handle), module, entry.encode())
    return handle


def launch(function, threads, params, stream):
    raw = (ptr * len(params))(*(c.addressof(value) for value in params))
    call("cuLaunchKernel", function, ROWS, 1, 1, threads, 1, 1, 0, stream, raw, ptr())


def build_graph(kernel, weights, packed, out, stride, kind, stream):
    graph, executable = ptr(), ptr()
    call("cuStreamBeginCapture_v2", stream, 1)
    for weight in weights:
        launch(*kernel, (weight, packed, out, u64(stride), i32(EXTENT), i32(kind)), stream)
    call("cuStreamEndCapture", stream, c.byref(graph))
    call("cuGraphInstantiateWithFlags", c.byref(executable), graph, u64(0))
    call("cuGraphDestroy", graph)
    return executable


def benchmark(graphs, stream, nodes):
    start, stop = ptr(), ptr()
    call("cuEventCreate", c.byref(start), 0)
    call("cuEventCreate", c.byref(stop), 0)
    names = list(graphs)
    samples = {name: [] for name in names}
    for repeat in range(12):
        order = names[repeat % len(names):] + names[:repeat % len(names)]
        for name in (order[::-1] if repeat >= 6 else order):
            graph = graphs[name]
            call("cuGraphLaunch", graph, stream)
            call("cuEventRecord", start, stream)
            for _ in range(12):
                call("cuGraphLaunch", graph, stream)
            call("cuEventRecord", stop, stream)
            call("cuEventSynchronize", stop)
            elapsed = c.c_float()
            call("cuEventElapsedTime", c.byref(elapsed), start, stop)
            samples[name].append(1000 * elapsed.value / (nodes * 12))
    call("cuEventDestroy_v2", start)
    call("cuEventDestroy_v2", stop)
    return {"samples_us": samples, "median_us": {name: median(v) for name, v in samples.items()}}


ROWS, EXTENT, MATRIX_COUNT = 8192, 8, 8
call("cuInit", 0)
device, ctx, stream = i32(), ptr(), ptr()
call("cuDeviceGet", c.byref(device), 0)
call("cuDevicePrimaryCtxRetain", c.byref(ctx), device)
call("cuCtxSetCurrent", ctx)
call("cuStreamCreate", c.byref(stream), 1)
modules = {}
for name in ("control", "candidate"):
    module = ptr()
    call("cuModuleLoad", c.byref(module), str(Path(__file__).with_name(f"{name}.cubin")).encode())
    modules[name] = module

rng = np.random.default_rng(1886)
report = {"rows": ROWS, "extent": EXTENT, "matrix_count": MATRIX_COUNT, "results": {}}
out = allocate(ROWS * 4)
for label, kind, block_bytes in (("q4", 12, 144), ("q6", 14, 210)):
    kernels = {
        "control": (load_function(modules["control"], "goose_mmvq_dense_matvec"), 128),
        "fold_two": (load_function(modules["candidate"], f"goose_mmvq_fold_two_{label}"), 64),
        "fold_four": (load_function(modules["candidate"], f"goose_mmvq_fold_four_{label}"), 32),
    }
    stride = block_bytes * EXTENT
    weights = []
    for _ in range(MATRIX_COUNT):
        host = rng.integers(0, 256, (ROWS, EXTENT, block_bytes), dtype=np.uint8)
        scales = host[..., :4] if kind == 12 else host[..., 208:]
        scales.view(np.float16)[:] = rng.uniform(0.0, 0.02, scales.view(np.float16).shape)
        weights.append(upload(host))
    packed_size = EXTENT * 288
    packed = allocate(packed_size)
    matched_rows = 0
    for weight in weights:
        host = rng.integers(0, 256, (EXTENT, 8, 36), dtype=np.uint8)
        host[..., :4].view(np.float16)[:] = rng.uniform(-0.1, 0.1, (EXTENT, 8, 2))
        call("cuMemcpyHtoD_v2", packed, ptr(host.ctypes.data), c.c_size_t(packed_size))
        ref = None
        for name, kernel in kernels.items():
            call("cuMemsetD8Async", out, 255, c.c_size_t(ROWS * 4), stream)
            launch(*kernel, (weight, packed, out, u64(stride), i32(EXTENT), i32(kind)), stream)
            call("cuStreamSynchronize", stream)
            actual = np.empty(ROWS, dtype=np.float32)
            call("cuMemcpyDtoH_v2", ptr(actual.ctypes.data), out, c.c_size_t(actual.nbytes))
            assert np.isfinite(actual).all(), (label, name)
            if ref is None:
                ref = actual.view(np.uint32)
            else:
                differences = int(np.count_nonzero(ref != actual.view(np.uint32)))
                assert differences == 0, (label, name, differences)
                matched_rows += ROWS
    print(label, "bit-exact FP32 rows (both variants)", matched_rows, flush=True)
    data = {"bit_exact_rows": matched_rows, "weight_working_set_bytes": MATRIX_COUNT * stride * ROWS}
    for mode, traversal in (("same_matrix", [weights[0]] * MATRIX_COUNT), ("rotating_matrices", weights)):
        graphs = {name: build_graph(kernel, traversal, packed, out, stride, kind, stream)
                  for name, kernel in kernels.items()}
        data[mode] = benchmark(graphs, stream, len(traversal))
        print(label, mode, data[mode]["median_us"], flush=True)
        for graph in graphs.values():
            call("cuGraphExecDestroy", graph)
    report["results"][label] = data
    for address in (*weights, packed):
        call("cuMemFree_v2", address)
Path("probe-results.json").write_text(json.dumps(report, indent=2) + "\n")
