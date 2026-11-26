"""Check Q4 weight reuse with the three real Bonsai draft matrices."""

import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
from statistics import median
import sys

import numpy as np


experiment_dir = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "mmq", experiment_dir / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py")
mmq = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mmq)
gpu = mmq.gpu
D, I, P, U = mmq.D, mmq.I, mmq.P, mmq.U


def run_projection(matrix, width, outputs, tokens, kernels, rng):
    source = rng.normal(0, 0.7, (tokens, width)).astype(np.float32)
    weights = mmq.rec.Layer.stage(matrix)
    values = mmq.rec.Layer.stage(source.tobytes())
    result = gpu.allocate(outputs * tokens * 4)
    pair = mmq.cuda_event(), mmq.cuda_event()

    def launch(group):
        mmq.dispatch(kernels[group], (outputs // 4, (tokens + group - 1) // group), 128,
                     (weights, values, result, U(width // 32), U(outputs), U(tokens)))

    try:
        launch(4)
        reference = bytes(gpu.download(result, outputs * tokens))
        assert np.isfinite(np.frombuffer(reference, dtype=np.float32)).all()
        for group in (8, 16, 32):
            launch(group)
            assert bytes(gpu.download(result, outputs * tokens)) == reference, (tokens, group)
        samples = {group: [] for group in kernels}
        if tokens == 128:
            for _ in range(8):
                for group in (4, 8, 16, 32, 32, 16, 8, 4):
                    samples[group].append(mmq.elapsed(lambda: launch(group), *pair))
        return {"width": width, "rows": outputs, "tokens": tokens,
                "matched_f32_words": outputs * tokens,
                "cuda_event_median_ms": {k: median(v) for k, v in samples.items() if v},
                "samples_ms": samples}
    finally:
        for event in pair:
            gpu.call("cuEventDestroy_v2", event)
        for ptr in (result, values, weights):
            gpu.call("cuMemFree_v2", ptr)


def main():
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        baseline, candidate = [mmq.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5]]
        kernels = {}
        for group in (4, 8, 16, 32):
            fn = P()
            suffix = "four" if group == 4 else str(group)
            gpu.call("cuModuleGetFunction", ct.byref(fn), baseline if group == 4 else candidate,
                     f"bonsai_mtp_q4_{suffix}".encode())
            kernels[group] = fn
        lookup = {t["name"]: t for t in gpu.inventory["tensors"]}
        rng = np.random.default_rng(2107)
        cases = []
        with gpu.model.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as mapping:
            for name in ("blk.64.nextn.eh_proj.weight", "blk.64.attn_k.weight", "blk.64.attn_v.weight"):
                tensor = lookup[name]
                width, rows = tensor["shape"]
                assert tensor["type"] == 2 and rows % 4 == 0
                start = gpu.inventory["data_offset"] + tensor["offset"]
                data = mapping[start:start + (width // 32) * rows * 18]
                for tokens in (128, 113):
                    cases.append({"tensor": name, **run_projection(data, width, rows, tokens, kernels, rng)})
        print(json.dumps({"cases": cases}, indent=2))
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    main()
