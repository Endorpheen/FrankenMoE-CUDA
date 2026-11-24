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
kernels = {}
for name, symbol, tiles in (("control", b"goose_moe_weighted", 8),
                             ("weighted4", b"goose_moe_weighted4", 2)):
    module, function = ptr(), ptr()
    image = Path(__file__).resolve().with_name(f"{name}.cubin")
    call("cuModuleLoad", c.byref(module), str(image).encode())
    call("cuModuleGetFunction", c.byref(function), module, symbol)
    kernels[name] = (function, tiles)


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
layers, width, experts = 16, 2048, 8
for tokens in (4, 252, 256):
    x = rng.standard_normal((layers, tokens, experts, width), dtype=np.float32)
    x[:, 0, 0, ::2] = -0.0
    weights = rng.standard_normal((layers, tokens, experts), dtype=np.float32)
    weights[:, 0, 1] = -0.0
    nx, nw = x[0].nbytes, weights[0].nbytes
    output_size = 4 * tokens * width
    xbase, wbase = upload(x), upload(weights)
    output = allocate(layers * output_size)
    args = [(u64(xbase.value + nx * layer), u64(wbase.value + nw * layer),
             u64(output.value + output_size * layer), c.c_int64(width), c.c_int64(experts))
            for layer in range(layers)]
    params = [(ptr * len(row))(*(c.addressof(a) for a in row)) for row in args]

    def launch(name):
        kernel, tiles = kernels[name]
        for param in params:
            call("cuLaunchKernel", kernel, tokens, tiles, 1, 256, 1, 1, 0, ptr(), param, ptr())

    replies = []
    for name in kernels:
        call("cuMemsetD8_v2", output, 255, c.c_size_t(layers * output_size))
        launch(name)
        values = np.empty((layers, tokens, width), dtype=np.float32)
        call("cuMemcpyDtoH_v2", ptr(values.ctypes.data), output, c.c_size_t(values.nbytes))
        assert np.isfinite(values).all(), name
        replies.append(values.view(np.uint32))
    mismatches = int(np.count_nonzero(replies[0] != replies[1]))
    print(f"tokens={tokens} words={replies[0].size} mismatches={mismatches}", flush=True)
    assert mismatches == 0
    timings = {name: [] for name in kernels}
    for order in (("control", "weighted4"), ("weighted4", "control")) * 3:
        for name in order:
            call("cuEventRecord", start, ptr())
            for _ in range(5):
                launch(name)
            call("cuEventRecord", end, ptr())
            call("cuEventSynchronize", end)
            elapsed = c.c_float()
            call("cuEventElapsedTime", c.byref(elapsed), start, end)
            timings[name].append(elapsed.value * 1000 / (5 * layers))
    for name, ms in timings.items():
        print(tokens, name, "us/launch", ms, "median", median(ms), flush=True)
    for address in (xbase, wbase, output):
        call("cuMemFree_v2", address)
