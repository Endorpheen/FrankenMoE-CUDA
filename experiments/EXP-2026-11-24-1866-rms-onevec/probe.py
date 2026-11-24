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
kernels = {}
for name, symbol in (("control", b"goose_rms_reference"), ("onevec", b"goose_rms_reference_one")):
    module, function = ptr(), ptr()
    image = Path(__file__).resolve().with_name(f"{name}.cubin")
    call("cuModuleLoad", c.byref(module), str(image).encode())
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
rng = np.random.default_rng(1866)
cases = (
    (128, 131072, False, False, 0),
    (128, 65536, True, False, 0),
    (512, 65536, False, True, 0),
    (512, 8192, True, False, 16),
    (2048, 4096, True, False, 0),
    (2048, 4096, False, True, 0),
    (4096, 512, True, True, 0),
)
for width, rows, weighted, scaled, pad in cases:
    stride = width + pad
    lanes = min(1024, width // 4)
    x = rng.uniform(-1, 1, (rows, stride)).astype(np.float32)
    x[0, ::2] = -0.0
    weights = rng.uniform(-1, 1, width).astype(np.float32)
    cfg = (c.c_int64 * 26)()
    cfg[2:6] = (width, rows, 1, 1)
    cfg[6:14] = (width, rows, 1, 1, 4, stride * 4, stride * rows * 4, stride * rows * 4)
    cfg[14:22] = (width, 1, 1, 1, 4, width * 4, width * 4, width * 4)
    cfg[23] = 16 if scaled else 0
    address_x, address_weights = upload(x), upload(weights) if weighted else u64(0)
    size = rows * width * 4
    output = allocate(size)
    common = (address_x, address_weights, output, cfg, c.c_float(1e-6))
    scale = c.c_float(-0.375)
    launch_args = {}
    for name in kernels:
        args = common + ((i32(1),) if name == "control" else ()) + (scale,)
        packet = (ptr * len(args))(*(c.addressof(arg) for arg in args))
        launch_args[name] = (args, packet)

    def launch(name):
        call("cuLaunchKernel", kernels[name], rows, 1, 1, lanes, 1, 1, 0, ptr(), launch_args[name][1], ptr())

    replies = []
    for name in kernels:
        call("cuMemsetD8_v2", output, 255, c.c_size_t(size))
        launch(name)
        host = np.empty((rows, width), dtype=np.float32)
        call("cuMemcpyDtoH_v2", ptr(host.ctypes.data), output, c.c_size_t(size))
        assert np.isfinite(host).all(), name
        replies.append(host.view(np.uint32))
    mismatches = int(np.count_nonzero(replies[0] != replies[1]))
    print(f"width={width} rows={rows} weight={int(weighted)} scale={int(scaled)} pad={pad} mismatches={mismatches}", flush=True)
    assert mismatches == 0
    samples = {name: [] for name in kernels}
    for order in (("control", "onevec"), ("onevec", "control")) * 3:
        for name in order:
            call("cuEventRecord", start, ptr())
            for _ in range(20):
                launch(name)
            call("cuEventRecord", end, ptr())
            call("cuEventSynchronize", end)
            elapsed = c.c_float()
            call("cuEventElapsedTime", c.byref(elapsed), start, end)
            samples[name].append(elapsed.value / 20)
    for name, values in samples.items():
        print(f"{width} {rows} {int(weighted)} {int(scaled)} {pad} {name} ms/launch {values} median {median(values)}", flush=True)
    for address in (address_x, address_weights, output):
        if address.value:
            call("cuMemFree_v2", address)
