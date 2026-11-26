"""Measure resident-logit device choice and full readback on saved Bonsai logits."""

import ctypes as ct
import importlib.util
import json
from pathlib import Path
from statistics import median
import sys

import numpy as np


source = (Path(__file__).resolve().parents[2] /
          "EXP-2026-11-25-2034-bonsai-mtp-device-argmax/source/check.py")
spec = importlib.util.spec_from_file_location("choice", source)
choice = importlib.util.module_from_spec(spec)
spec.loader.exec_module(choice)
component, gpu = choice.component, choice.gpu
D, I, P, U = choice.D, choice.I, choice.P, choice.U
VOCAB, JOBS, RECORD = choice.VOCAB, choice.JOBS, choice.RECORD
EOG = [248044, 248046, 248063, 248064, 248065]


def check(image, logits):
    device, context = I(), P()
    gpu.call("cuInit", U(0))
    gpu.call("cuDeviceGet", ct.byref(device), I(0))
    gpu.call("cuCtxCreate_v2", ct.byref(context), U(0), device)
    try:
        module = component.rec.Layer.load(image)
        tiles = choice.kernel(module, b"bonsai_choice_tiles")
        final = choice.kernel(module, b"bonsai_choice_final")
        mask = np.zeros(VOCAB // 32, dtype="<u4")
        for token in EOG:
            mask[token // 32] |= 1 << (token % 32)
        mask_dev = component.rec.Layer.stage(mask.tobytes())
        logits_dev = gpu.allocate(VOCAB * 4)
        partial = gpu.allocate(JOBS * RECORD.itemsize)
        output = gpu.allocate(RECORD.itemsize)
        full_host = (ct.c_ubyte * (VOCAB * 4))()
        token_host = (ct.c_ubyte * RECORD.itemsize)()
        events = component.cuda_event(), component.cuda_event()

        def run_full():
            gpu.call("cuMemcpyDtoH_v2", full_host, logits_dev, gpu.S(len(full_host)))

        def run_choice(use_mask):
            component.dispatch(tiles, (JOBS, 1), 256, (logits_dev, mask_dev if use_mask else D(0),
                                                        partial, U(VOCAB), U(JOBS)))
            component.dispatch(final, (1, 1), 256, (partial, output, U(JOBS)))
            gpu.call("cuMemcpyDtoH_v2", token_host, output, gpu.S(len(token_host)))

        counts = {"finite_choices": 0, "masked_winner_cases": 0, "invalid_fallback": 0}
        timings = {"full_readback": [], "device_choice_readback": []}
        for row in logits:
            values = np.ascontiguousarray(row, dtype="<f4")
            raw = values.tobytes()
            gpu.upload(logits_dev, (ct.c_ubyte * len(raw)).from_buffer_copy(raw))
            for use_mask in (False, True):
                run_choice(use_mask)
                result = np.frombuffer(bytes(token_host), dtype=RECORD)[0]
                if not np.isfinite(values).all():
                    assert result["invalid"] == 1
                    counts["invalid_fallback"] += 1
                    continue
                expected = values.copy()
                if use_mask:
                    expected[EOG] = -np.inf
                top = int(np.argmax(expected))
                assert result["invalid"] == 0 and result["token"] == top
                assert result["logit"].tobytes() == expected[top].tobytes()
                counts["finite_choices"] += 1
                counts["masked_winner_cases"] += int(use_mask and int(np.argmax(values)) in EOG)
            if np.isfinite(values).all():
                for label in ("full_readback", "device_choice_readback",
                              "device_choice_readback", "full_readback"):
                    fn = run_full if label == "full_readback" else lambda: run_choice(True)
                    timings[label].append(component.elapsed(fn, *events))
        return {"cases": counts, "rows": len(logits), "eog_fixture": EOG,
                "median_us": {key: median(data) * 1000 for key, data in timings.items()},
                "raw_ms": timings}
    finally:
        gpu.call("cuCtxDestroy_v2", context)


def main():
    image = Path(sys.argv[3]).read_bytes()
    fixture = Path(sys.argv[4])
    dtype = np.dtype([("step","<u4"),("logits","<f4",(VOCAB,))])
    data = np.fromfile(fixture,dtype=dtype)
    assert len(data) > 0 and np.isfinite(data["logits"]).all()
    special = np.full((3,VOCAB),-5,dtype="<f4")
    special[0,[31,1024,VOCAB-1]] = 5
    special[0,EOG[0]] = 6
    special[1,200] = -0.0
    special[1,2048] = 0.0
    special[2,1],special[2,7] = np.nan,np.inf
    output = check(image,np.concatenate((data["logits"],special)))
    output["saved_logit_steps"] = data["step"].tolist()
    print(json.dumps(output,indent=2))


if __name__ == "__main__":
    main()
