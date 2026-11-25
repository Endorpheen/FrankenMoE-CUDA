"""Compare all fused GDN prefixes against consecutive frozen decode launches."""
import ctypes as ct
import importlib.util
import json
from pathlib import Path
from statistics import median
import sys

import numpy as np


experiments = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "check_gdn", experiments / "EXP-2026-11-25-1991-bonsai-gdn-mma/source/check-gdn.py")
reference = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reference)
gpu, component = reference.gpu, reference.component
D, I, P, U = (getattr(gpu, name) for name in "DIPU")
STATE = reference.STATE


def check(count, empty, kernels, rng, events):
    owned = []

    def store(values):
        ptr = component.rec.Layer.stage(np.asarray(values, dtype="<f4").tobytes())
        owned.append(ptr)
        return ptr

    def allocate(n):
        ptr = gpu.allocate(n * 4)
        owned.append(ptr)
        return ptr

    def offset(ptr, n):
        return D(ptr.value + n * 4)

    def launch(kernel, args):
        params = (P * len(args))(*(ct.addressof(arg) for arg in args))
        gpu.call("cuLaunchKernel", kernel, U(48), U(1), U(8), U(128), U(1), U(1), U(0), P(), params, P())

    try:
        qk = rng.normal(0, 1, (count, 32, 128))
        qk /= np.linalg.norm(qk, axis=-1, keepdims=True)
        q = store(qk)
        k = offset(q, 2048)
        v = store(rng.normal(0, .2, (count, 10240)))
        decay = store(-rng.uniform(.003, .20, (count, 48)))
        beta = store(rng.uniform(.05, .95, (count, 48)))
        initial_values = np.zeros(STATE) if empty else rng.normal(0, .008, STATE)
        initial, alias = store(initial_values), store(initial_values)
        output = allocate(count * 6144)
        snapshots = [allocate(STATE) for _ in range(count)]
        shape = reference.config(1)

        def serial():
            cur = initial
            for t, dst in enumerate(snapshots):
                ptrs = [offset(p, t * stride) for p, stride in zip((q,k,v,decay,beta), (4096,4096,10240,48,48))]
                ptrs.extend((cur, offset(output, t * 6144), dst))
                packet = (D * 57)(*(p.value for p in ptrs), *shape)
                launch(kernels["serial"], (packet,))
                cur = dst

        def fused(in_state=initial, out_state=snapshots[-1]):
            third = snapshots[2] if count == 4 else D(0)
            launch(kernels["fused"], (q, k, v, decay, beta, in_state, output, out_state,
                                      snapshots[0], snapshots[1], third, U(count)))

        def capture(last=snapshots[-1]):
            return (bytes(gpu.download(output, count * 6144)), *(
                bytes(gpu.download(p, STATE)) for p in [*snapshots[:-1], last]))

        serial()
        expected = capture()
        assert all(np.isfinite(np.frombuffer(v, dtype="<f4")).all() for v in expected)
        fused()
        assert capture() == expected, (count, empty)
        fused(alias, alias)
        assert capture(alias) == expected, (count, empty, "alias")

        samples = {mode: [] for mode in ("serial", "fused")}
        for mode, run in (("serial", serial), ("fused", fused), ("fused", fused), ("serial", serial)) * 3:
            def repeat():
                for _ in range(20):
                    run()
            samples[mode].append(component.elapsed(repeat, *events) / 20)
        return {
            "rows": count, "initial_state": "zero" if empty else "random",
            "exact_output_values": count * 6144, "exact_state_values": count * STATE,
            "exact_in_place_final": True,
            "median_ms": {mode: median(times) for mode, times in samples.items()}, "samples_ms": samples,
        }
    finally:
        for ptr in reversed(owned):
            gpu.call("cuMemFree_v2", ptr)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        image, frozen = (component.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5])
        kernels = {}
        for label, module, name in (("serial", frozen, b"goose_gdn_decode"),
                                    ("fused", image, b"bonsai_verify_gdn_prefix")):
            f = P()
            gpu.call("cuModuleGetFunction", ct.byref(f), module, name)
            kernels[label] = f
        events = component.cuda_event(), component.cuda_event()
        rng = np.random.default_rng(2045)
        print(json.dumps([check(n, empty, kernels, rng, events) for n in (3, 4)
                          for empty in (False, True)], indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
