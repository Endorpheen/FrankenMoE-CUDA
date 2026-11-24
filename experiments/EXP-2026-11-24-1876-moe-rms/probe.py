import ctypes as c
from pathlib import Path
from statistics import median

import numpy as np

cuda = c.CDLL("libcuda.so.1")
ptr, u64, i32 = c.c_void_p, c.c_uint64, c.c_int

def call(name, *args):
    status = getattr(cuda, name)(*args)
    if status:
        raise RuntimeError(f"{name}: {status}")

call("cuInit", 0)
device, ctx = i32(), ptr()
call("cuDeviceGet", c.byref(device), 0)
call("cuDevicePrimaryCtxRetain", c.byref(ctx), device)
call("cuCtxSetCurrent", ctx)
import sys

kernels = {}
for name, filename, symbol in (
    ("rms", sys.argv[1], b"goose_rms_reference"),
    ("epilogue", "epilogue.cubin", b"goose_moe_shared_residual_prefill"),
    ("fused", "fused.cubin", b"goose_moe_epilogue_rms"),
):
    module, function = ptr(), ptr()
    call("cuModuleLoad", c.byref(module), str(Path(filename).resolve()).encode())
    call("cuModuleGetFunction", c.byref(function), module, symbol)
    kernels[name] = function


def allocate(size):
    address = u64()
    call("cuMemAlloc_v2", c.byref(address), c.c_size_t(size))
    return address

def upload(array):
    address = allocate(array.nbytes)
    call("cuMemcpyHtoD_v2", address, ptr(array.ctypes.data), c.c_size_t(array.nbytes))
    return address

start, end = ptr(), ptr()
call("cuEventCreate", c.byref(start), 0)
call("cuEventCreate", c.byref(end), 0)
rng = np.random.default_rng(1876)
width = 2048
for tokens, weighted in ((4, False), (4, True), (4092, True), (4096, False), (4096, True)):
    data = [rng.standard_normal((tokens, width), dtype=np.float32) for _ in range(3)]
    data[0][0, ::2] = -0.0
    data[2][0, 1::4] = 0.0
    gates = rng.uniform(0, 1, tokens).astype(np.float32)
    gates[0] = -0.0
    weights = rng.uniform(-1, 1, width).astype(np.float32)
    routed, shared, residual = [upload(x) for x in data]
    gate = upload(gates)
    w = upload(weights) if weighted else u64(0)
    size = data[0].nbytes
    out, norm = allocate(size), allocate(size)
    cfg = (c.c_int64 * 26)()
    cfg[2:6] = (width, tokens, 1, 1)
    cfg[6:14] = (width, tokens, 1, 1, 4, width * 4, size, size)
    cfg[14:22] = (width, 1, 1, 1, 4, width * 4, width * 4, width * 4)
    vectors = tokens * width // 4
    eps = c.c_float(1e-6)
    args = {
        "epilogue": (routed, shared, gate, residual, out, c.c_uint(vectors)),
        "rms": (out, w, norm, cfg, eps, i32(1), c.c_float(1)),
        "fused": (routed, shared, gate, residual, w, out, norm, eps),
        "alias": (routed, shared, gate, residual, w, routed, shared, eps)
    }
    params = {name: (ptr * len(row))(*(c.addressof(arg) for arg in row)) for name, row in args.items()}

    def launch(name, key, blocks, lanes):
        call("cuLaunchKernel", kernels[name], blocks, 1, 1, lanes, 1, 1, 0, ptr(), params[key], ptr())

    def evaluate(name):
        if name == "control":
            launch("epilogue", "epilogue", (vectors + 127) // 128, 128)
            launch("rms", "rms", tokens, 512)
        else:
            launch("fused", name, tokens, 512)

    def outputs(addresses):
        values = []
        for address in addresses:
            host = np.empty((tokens, width), dtype=np.float32)
            call("cuMemcpyDtoH_v2", ptr(host.ctypes.data), address, c.c_size_t(size))
            assert np.isfinite(host).all()
            values.append(host.view(np.uint32))
        return values

    evaluate("control")
    reference = outputs((out, norm))
    evaluate("fused")
    counts = [int(np.count_nonzero(a != b)) for a, b in zip(reference, outputs((out, norm)), strict=True)]
    print(f"tokens={tokens} weights={int(weighted)} separate_mismatches={counts}", flush=True)
    assert counts == [0, 0]
    samples = {name: [] for name in ("control", "fused")}
    for order in (("control", "fused"), ("fused", "control")) * 3:
        for name in order:
            call("cuEventRecord", start, ptr())
            for _ in range(24): evaluate(name)
            call("cuEventRecord", end, ptr())
            call("cuEventSynchronize", end)
            ms = c.c_float()
            call("cuEventElapsedTime", c.byref(ms), start, end)
            samples[name].append(ms.value / 24)
    for name, values in samples.items():
        print(tokens, int(weighted), name, "ms/eval", values, "median", median(values), flush=True)
    evaluate("alias")
    counts = [int(np.count_nonzero(a != b)) for a, b in zip(reference, outputs((routed, shared)), strict=True)]
    print(f"tokens={tokens} weights={int(weighted)} alias_mismatches={counts}", flush=True)
    assert counts == [0, 0]
    for address in (routed, shared, residual, gate, w, out, norm):
        if address.value: call("cuMemFree_v2", address)
