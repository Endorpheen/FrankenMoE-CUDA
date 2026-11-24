import ctypes as c
from pathlib import Path
from statistics import median

import numpy as np

cuda = c.CDLL("libcuda.so.1")
ptr = c.c_void_p
u64 = c.c_uint64

def call(name, *args):
    status = getattr(cuda, name)(*args)
    if status:
        raise RuntimeError(f"{name}: {status}")

call("cuInit", 0)
device, ctx = c.c_int(), ptr()
call("cuDeviceGet", c.byref(device), 0)
call("cuDevicePrimaryCtxRetain", c.byref(ctx), device)
call("cuCtxSetCurrent", ctx)
import sys

kernels = {}
control, candidate = ptr(), ptr()
call("cuModuleLoad", c.byref(control), sys.argv[1].encode())
image = Path(__file__).resolve().with_name("epilogue.cubin")
call("cuModuleLoad", c.byref(candidate), str(image).encode())
for name, module, symbol in (("broadcast", control, b"goose_vector_ops"),
                             ("add", control, b"goose_vector_flat4"),
                             ("fused", candidate, b"goose_moe_shared_residual_prefill")):
    fn = ptr()
    call("cuModuleGetFunction", c.byref(fn), module, symbol)
    kernels[name] = fn


def allocate(nbytes):
    address = u64()
    call("cuMemAlloc_v2", c.byref(address), c.c_size_t(nbytes))
    return address

def upload(array):
    address = allocate(array.nbytes)
    call("cuMemcpyHtoD_v2", address, ptr(array.ctypes.data), c.c_size_t(array.nbytes))
    return address

start, end = ptr(), ptr()
call("cuEventCreate", c.byref(start), 0)
call("cuEventCreate", c.byref(end), 0)
rng = np.random.default_rng(1874)
width = 2048
for tokens in (4, 4092, 4096):
    tensors = [rng.standard_normal((tokens, width), dtype=np.float32) for _ in range(3)]
    tensors[0][0, ::2] = -0.0
    gates = rng.uniform(0.0, 1.0, tokens).astype(np.float32)
    gates[0] = -0.0
    routed, shared, residual = [upload(tensor) for tensor in tensors]
    gate = upload(gates)
    output_bytes = tensors[0].nbytes
    scaled, mix, output = [allocate(output_bytes) for _ in range(3)]
    cfg = (c.c_int64 * 26)()
    cfg[:6] = (3, tokens * width, width, tokens, 1, 1)
    cfg[6:14] = (width, tokens, 1, 1, 4, 4 * width, output_bytes, output_bytes)
    cfg[14:22] = (1, tokens, 1, 1, 4, 4, 4 * tokens, 4 * tokens)
    grid = (tokens * width + 127) // 128
    cfg[24:26] = (grid, 128)
    vectors = tokens * width // 4
    args = {
        "broadcast": (shared, gate, scaled, cfg, c.c_float(0), c.c_float(0)),
        "add": (routed, scaled, mix, c.c_int64(1), c.c_int64(vectors), c.c_float(0), c.c_float(0)),
        "residual": (mix, residual, output, c.c_int64(1), c.c_int64(vectors), c.c_float(0), c.c_float(0)),
        "fused": (routed, shared, gate, residual, output, c.c_uint(vectors))
    }
    params = {name: (ptr * len(row))(*(c.addressof(arg) for arg in row)) for name, row in args.items()}

    def launch(name, key, blocks):
        call("cuLaunchKernel", kernels[name], blocks, 1, 1, 128, 1, 1, 0, ptr(), params[key], ptr())

    def evaluate(arm):
        if arm == "fused":
            launch("fused", "fused", (vectors + 127) // 128)
        else:
            launch("broadcast", "broadcast", grid)
            launch("add", "add", (vectors + 127) // 128)
            launch("add", "residual", (vectors + 127) // 128)

    replies = []
    for arm in ("control", "fused"):
        call("cuMemsetD8_v2", output, 255, c.c_size_t(output_bytes))
        evaluate(arm)
        host = np.empty((tokens, width), dtype=np.float32)
        call("cuMemcpyDtoH_v2", ptr(host.ctypes.data), output, c.c_size_t(output_bytes))
        assert np.isfinite(host).all(), arm
        replies.append(host.view(np.uint32))
    mismatches = int(np.count_nonzero(replies[0] != replies[1]))
    print(f"tokens={tokens} words={replies[0].size} mismatches={mismatches}", flush=True)
    assert mismatches == 0
    samples = {arm: [] for arm in ("control", "fused")}
    for order in (("control", "fused"), ("fused", "control")) * 3:
        for arm in order:
            call("cuEventRecord", start, ptr())
            for _ in range(32):
                evaluate(arm)
            call("cuEventRecord", end, ptr())
            call("cuEventSynchronize", end)
            elapsed = c.c_float()
            call("cuEventElapsedTime", c.byref(elapsed), start, end)
            samples[arm].append(elapsed.value / 32)
    for arm, values in samples.items():
        print(tokens, arm, "ms/eval", values, "median", median(values), flush=True)
    for address in (routed, shared, residual, gate, scaled, mix, output):
        call("cuMemFree_v2", address)
