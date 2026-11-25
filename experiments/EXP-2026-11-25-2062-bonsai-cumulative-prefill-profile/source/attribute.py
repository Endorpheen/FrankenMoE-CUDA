"""Compare the stable, cumulative and PrismML Bonsai 32K/16 traces."""

import argparse
import importlib.util
import json
from pathlib import Path


experiments = Path(__file__).resolve().parents[2]
earlier = experiments / "EXP-2026-11-25-2050-bonsai-prefill-attribution/source/attribute.py"
spec = importlib.util.spec_from_file_location("prefill_attribution", earlier)
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)

MATRIX = {
    "bonsai_pq2_cached_64x128_k64",
    "bonsai_pq2_cached_fp16_acc_k32",
    "bonsai_pq2_mmq_int8",
    "bonsai_pack_half",
    "bonsai_pack_q8_128",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("prism", "stable", "experimental"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    ids = None
    traces = {}
    for label, path in vars(args).items():
        prism = label == "prism"
        trace_ids, result = analysis.read_trace(path, prism)
        if ids is not None and trace_ids != ids:
            raise ValueError(f"{label} returned different output IDs")
        ids = trace_ids
        kernels = result["prefill"]
        matrix = {"mul_mat_q", "quantize_mmq_q8_1"} if prism else MATRIX
        flash = {"flash_attn_ext_f16"} if prism else {n for n in kernels if n.startswith("bonsai_flash")}
        result["matrix_and_pack_ns"] = sum(kernels[n]["ns"] for n in matrix & kernels.keys())
        result["flash_ns"] = sum(kernels[n]["ns"] for n in flash & kernels.keys())
        traces[label] = result

    numerical = traces["experimental"]["prefill"]
    if "bonsai_pq2_cached_fp16_acc_k32" not in numerical or "bonsai_pq2_cached_64x128_k64" in numerical:
        raise ValueError("The experimental trace did not use the cumulative MMQ preset")
    print(json.dumps({"token_ids": ids, "traces": traces}, indent=2))


if __name__ == "__main__":
    main()
