"""Check CUDA greedy selection on ties, EOG masking, and invalid logits."""
import ctypes as ct
import importlib.util
import json
from pathlib import Path
import sys

import numpy as np


experiments = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "pq2_mmq", experiments / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py")
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
D, I, P, U = gpu.D, gpu.I, gpu.P, gpu.U
VOCAB = 248320
JOBS = (VOCAB + 1023) // 1024
RECORD = np.dtype([("token", "<u4"), ("logit", "<f4"), ("invalid", "<u4")])


def kernel(module, name):
    function = P()
    gpu.call("cuModuleGetFunction", ct.byref(function), module, name)
    return function


def run_tests(image):
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        module = component.rec.Layer.load(image)
        tiles = kernel(module, b"bonsai_choice_tiles")
        final = kernel(module, b"bonsai_choice_final")
        rng = np.random.default_rng(2034)
        scores = rng.uniform(-5, -1, (3, VOCAB)).astype("<f4")
        scores[0, [31, 1024, VOCAB - 1]] = 5
        scores[1, 200] = -0.0
        scores[1, 2048] = 0.0
        scores[2, VOCAB - 1] = 12
        scores[2, 1] = np.nan
        scores[2, 7] = np.inf
        chosen_mask = [1, 7, 31, 200, VOCAB - 1]
        mask = np.zeros(VOCAB // 32, dtype="<u4")
        for token in chosen_mask:
            mask[token // 32] |= 1 << (token % 32)
        score_device = component.rec.Layer.stage(scores.tobytes())
        eog = component.rec.Layer.stage(mask.tobytes())
        partial = gpu.allocate(3 * JOBS * RECORD.itemsize)
        output = gpu.allocate(3 * RECORD.itemsize)
        answers = {}
        for use_mask in (False, True):
            component.dispatch(tiles, (JOBS, 3), 256, (score_device, eog if use_mask else D(0),
                                                     partial, U(VOCAB), U(JOBS)))
            component.dispatch(final, (3, 1), 256, (partial, output, U(JOBS)))
            host = (ct.c_ubyte * (3 * RECORD.itemsize))()
            gpu.call("cuMemcpyDtoH_v2", host, output, gpu.S(len(host)))
            result = np.frombuffer(bytes(host), dtype=RECORD)
            expected = scores.copy()
            invalid = ~np.isfinite(expected)
            expected[invalid] = -np.inf
            if use_mask:
                expected[:, chosen_mask] = -np.inf
            top = np.argmax(expected, axis=1)
            np.testing.assert_array_equal(result["token"], top)
            np.testing.assert_array_equal(result["invalid"], invalid.any(axis=1))
            assert result["logit"].tobytes() == expected[np.arange(3), top].tobytes()
            answers["masked" if use_mask else "unmasked"] = result.tolist()
        return answers
    finally:
        gpu.call("cuCtxDestroy_v2", context)


if __name__ == "__main__":
    print(json.dumps(run_tests(Path(sys.argv[3]).read_bytes()), indent=2))
