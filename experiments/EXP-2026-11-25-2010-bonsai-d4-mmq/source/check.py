"""Validate D4 packing and dot sums independently with real PQ2 matrices."""
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
    "component", experiments / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py")
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
I, P, S, U = gpu.I, gpu.P, gpu.S, gpu.U


def read(ptr, size):
    buf = (ct.c_ubyte * size)()
    gpu.call("cuMemcpyDtoH_v2", buf, ptr, S(size))
    return bytes(buf)


def run_case(raw, width, rows, tokens, rng, kernels, events):
    data = rng.normal(0, .7, (tokens, width)).astype(np.float32)
    data[0] = 0
    data[1, :8] = [127, -127, .5, -.5, 1.5, -1.5, 63.5, -63.5]
    q = data.reshape(tokens, width // 32, 32)
    peak = np.max(np.abs(q), axis=2)
    inv = np.divide(np.float32(127), peak, out=np.zeros_like(peak), where=peak != 0)
    scales = np.divide(np.float32(1), inv, out=np.zeros_like(inv), where=inv != 0)
    value = q * inv[..., None]
    packed = np.copysign(np.floor(np.abs(value.astype(np.float64)) + .5), value).astype(np.int8)
    weights, x = map(component.rec.Layer.stage, (raw, data.tobytes()))
    half, codes, scales_dev, out = [gpu.allocate(n) for n in
        (tokens * width * 2, tokens * width, tokens * width // 8, tokens * rows * 4)]
    try:
        dims = U(width), U(rows), U(tokens)
        n = tokens * width

        def run(mode):
            if mode == "fp16":
                component.dispatch(kernels["half"], ((n//2 + 255)//256, 1), 256, (x, half, U(n//2)))
                component.dispatch(kernels["fp16"], ((rows+63)//64, (tokens+127)//128), 256,
                                   (weights, half, out, *dims))
                return
            p = 128 if mode == "previous" else 32
            group = 4 if mode == "previous" else 16
            component.dispatch(kernels["q8" if mode == "previous" else "d4"],
                               ((n//p + group - 1)//group, 1), 128, (x, codes, scales_dev, U(n//p)))
            tile = 128 if mode == "d4_128" else 64
            component.dispatch(kernels[mode], ((rows+63)//64, (tokens+tile-1)//tile), 256,
                               (weights, codes, scales_dev, out, *dims))

        run("fp16")
        reference = np.frombuffer(read(out, tokens*rows*4), dtype="<f4").reshape(tokens, rows)
        r = np.unique([0, 1, 7, 8, 15, 31, 63, rows//2, rows-1])
        t = np.unique(np.r_[0, 1, np.linspace(0, tokens-1, 24, dtype=int)])
        blocks = width // 128
        w = np.frombuffer(raw, dtype=np.uint8).reshape(rows, blocks, 34)[r]
        ws = w[..., :2].copy().view("<f2").reshape(-1, blocks).astype(np.float32)
        ws = np.repeat(ws, 4, axis=1)
        wi = (((w[..., 2:, None] >> np.array([0, 2, 4, 6], dtype=np.uint8)) & 3)
              .reshape(len(r), width//32, 32).astype(np.int32) - 1)
        dot = np.einsum("tgk,rgk->trg", packed[t].astype(np.int32), wi, dtype=np.int32).astype(np.float32)
        products = dot * (scales[t, None, :] * ws[None, :, :])
        gold = np.zeros(products.shape[:2], dtype=np.float32)
        for group in range(products.shape[-1]):
            gold += products[..., group]

        modes = ("fp16", "previous", "d4_64", "d4_128")
        measured = {}
        for mode in modes[1:]:
            run(mode)
            output = np.frombuffer(read(out, tokens*rows*4), dtype="<f4").reshape(tokens, rows)
            assert np.isfinite(output).all() and not output[0].any()
            metrics = {"relative_l2": float(np.linalg.norm(reference-output) / np.linalg.norm(reference))}
            if mode.startswith("d4_"):
                assert read(codes, n) == packed.tobytes()
                assert read(scales_dev, n//8) == scales.tobytes()
                selected = output[np.ix_(t, r)]
                metrics["oracle_max_abs"] = float(np.max(np.abs(selected-gold)))
                np.testing.assert_array_equal(selected, gold)
            measured[mode] = metrics
        timing = {mode:[] for mode in modes}
        for mode in (modes + modes[::-1])*3:
            timing[mode].append(component.elapsed(lambda: run(mode), *events))
        return {"width":width, "rows":rows, "tokens":tokens, "quality":measured,
                "median_ms":{m:median(v) for m,v in timing.items()}, "samples_ms":timing}
    finally:
        for ptr in (out, scales_dev, codes, half, x, weights):
            gpu.call("cuMemFree_v2", ptr)


device, ctx = I(), P()
gpu.call("cuInit", U(0))
gpu.call("cuDeviceGet", ct.byref(device), I(0))
gpu.call("cuCtxCreate_v2", ct.byref(ctx), U(0), device)
try:
    base, trial = (component.rec.Layer.load(Path(x).read_bytes()) for x in sys.argv[3:5])
    names = {"half":"bonsai_pack_half", "fp16":"bonsai_pq2_mmq_64x128_k64",
             "q8":"bonsai_pack_q8_128", "d4":"bonsai_pack_d4",
             "previous":"bonsai_pq2_mmq_int8", "d4_64":"bonsai_pq2_mmq_d4_64",
             "d4_128":"bonsai_pq2_mmq_d4_128"}
    kernels = {}
    for label, name in names.items():
        fn = P()
        gpu.call("cuModuleGetFunction", ct.byref(fn), base if label in ("half","fp16") else trial, name.encode())
        kernels[label] = fn
    t = {x["name"]:x for x in gpu.inventory["tensors"]}
    events = component.cuda_event(), component.cuda_event()
    rng = np.random.default_rng(2010)
    checks = []
    with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as mm:
        for name, count, trim in (("blk.0.attn_qkv.weight",257,1),("blk.0.ffn_gate.weight",1024,0),
            ("blk.0.ffn_up.weight",1024,0),("blk.0.ffn_down.weight",1023,0),("blk.42.ffn_gate.weight",1024,0),
            ("blk.42.ffn_down.weight",1024,0),("blk.3.attn_output.weight",1024,0)):
            width, rows = t[name]["shape"]
            assert t[name]["type"] == 142
            rows -= trim
            here = gpu.inventory["data_offset"] + t[name]["offset"]
            raw = mm[here:here+rows*(width//128)*34]
            checks.append(dict(tensor=name, **run_case(raw, width, rows, count, rng, kernels, events)))
    print(json.dumps(checks, indent=2))
finally:
    gpu.call("cuCtxDestroy_v2", ctx)
