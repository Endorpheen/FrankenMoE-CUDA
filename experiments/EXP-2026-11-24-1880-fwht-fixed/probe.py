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
modules = {}
for name in ("control", "fixed"):
    module = ptr()
    path = Path(__file__).resolve().with_name(f"{name}.cubin")
    call("cuModuleLoad", c.byref(module), str(path).encode())
    modules[name] = module


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
rng = np.random.default_rng(1880)
for width, rows in ((64, 7), (256, 5), (64, 262144), (64, 261888), (256, 65536), (256, 65472)):
    data = rng.standard_normal((rows, width), dtype=np.float32)
    data[0, ::2] = -0.0
    input_dev, output_dev = upload(data), allocate(data.nbytes)
    args = (input_dev, output_dev, i32(width), c.c_int64(rows), c.c_float(width ** -0.5))
    params = (ptr * len(args))(*(c.addressof(a) for a in args))
    functions = {}
    for name, symbol in (("control","goose_kv_fwht"), ("fixed",f"goose_kv_fwht_{width}")):
        fn = ptr()
        call("cuModuleGetFunction", c.byref(fn), modules[name], symbol.encode())
        functions[name] = fn

    def launch(name, packet=params):
        call("cuLaunchKernel", functions[name], (rows + 3) // 4, 1, 1, 128, 1, 1, 0, ptr(), packet, ptr())

    def read(address):
        host = np.empty_like(data)
        call("cuMemcpyDtoH_v2", ptr(host.ctypes.data), address, c.c_size_t(host.nbytes))
        assert np.isfinite(host).all()
        return host.view(np.uint32)

    launch("control")
    reference = read(output_dev)
    launch("fixed")
    differences = int(np.count_nonzero(reference != read(output_dev)))
    print(f"width={width} rows={rows} out_mismatches={differences}", flush=True)
    assert differences == 0
    timings = {name: [] for name in functions}
    for order in (("control","fixed"),("fixed","control")) * 3:
        for name in order:
            call("cuEventRecord", start, ptr())
            for _ in range(24): launch(name)
            call("cuEventRecord", end, ptr())
            call("cuEventSynchronize", end)
            elapsed = c.c_float()
            call("cuEventElapsedTime", c.byref(elapsed), start, end)
            timings[name].append(elapsed.value/24)
    for name, values in timings.items():
        print(width, rows, name, "ms/launch", values, "median", median(values), flush=True)
    alias = (input_dev, input_dev) + args[2:]
    packet = (ptr * len(alias))(*(c.addressof(a) for a in alias))
    launch("fixed", packet)
    differences = int(np.count_nonzero(reference != read(input_dev)))
    print(f"width={width} rows={rows} alias_mismatches={differences}", flush=True)
    assert differences == 0
    for address in (input_dev,output_dev): call("cuMemFree_v2", address)
