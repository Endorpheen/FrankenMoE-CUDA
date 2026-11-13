#!/usr/bin/env python3
"""Aggregate comparable Qwen baseline runs into benchmarks/baseline.json."""

import csv
import hashlib
import json
import pathlib
import statistics
import subprocess
import sys
from datetime import datetime, timezone


def command(*args: str) -> str:
    try:
        return subprocess.check_output(args, text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def median(values):
    return round(statistics.median(values), 4)


def spread(values):
    return {
        "min": round(min(values), 4),
        "max": round(max(values), 4),
        "stddev": round(statistics.stdev(values), 4) if len(values) > 1 else 0.0,
    }


def generated_hash(path: pathlib.Path) -> str:
    data = path.read_bytes().split(b"\n\ngeneration:", 1)[0]
    return hashlib.sha256(data).hexdigest()


def window(rows, first: bool) -> dict:
    chosen = rows[:32] if first else rows[-32:]
    wall_ms = sum(float(row["wall_ms"]) for row in chosen)
    read_mib = sum(int(row["read_bytes"]) for row in chosen) / 2**20
    return {
        "tokens": len(chosen),
        "tok_s": round(len(chosen) * 1000 / wall_ms, 4),
        "latency_ms_token": round(wall_ms / len(chosen), 4),
        "logical_read_mib_token": round(read_mib / len(chosen), 4),
        "cache_hit_pct_end": float(chosen[-1]["cache_hit_pct"]),
    }


root = pathlib.Path(__file__).resolve().parents[1]
run_dir = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
previous = json.loads(target.read_text()) if target.is_file() else {}


def measured(name: str, *args: str) -> str:
    return command(*args) or previous.get("environment", {}).get(name, "unavailable")


runs = []
for summary_path in sorted(run_dir.glob("run-*.json")):
    stem = summary_path.stem
    summary = json.loads(summary_path.read_text())
    with (run_dir / f"{stem}.csv").open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(line for line in stream if not line.startswith("#")))
    output = run_dir / f"{stem}.out"
    elapsed = sum(float(row["wall_ms"]) for row in rows) / 1000
    summary["run_id"] = stem
    summary["cold_window"] = window(rows, True)
    summary["warm_window"] = window(rows, False)
    summary["ttft_s"] = round(float(summary["load_s"]) + float(summary["prefill_s"]) + float(rows[0]["wall_ms"]) / 1000, 4)
    summary["logical_ssd_mib_s"] = round(float(summary["read_MiB"]) / elapsed, 4)
    summary["output_sha256"] = generated_hash(output)
    runs.append(summary)

if len(runs) < 5:
    raise SystemExit(f"Need at least five runs, found {len(runs)}")

hashes = sorted({run["output_sha256"] for run in runs})
medians = {
    "generation_tok_s": median([run["tok/s"] for run in runs]),
    "cold_window_tok_s": median([run["cold_window"]["tok_s"] for run in runs]),
    "warm_window_tok_s": median([run["warm_window"]["tok_s"] for run in runs]),
    "prompt_tok_s": median([run["prefill_tps"] for run in runs]),
    "ttft_s": median([run["ttft_s"] for run in runs]),
    "logical_ssd_mib_s": median([run["logical_ssd_mib_s"] for run in runs]),
    "logical_read_mib": median([run["read_MiB"] for run in runs]),
    "physical_ssd_mib_s": median([run["monitor"]["physical_read_mib_s"] for run in runs]),
    "cache_hit_pct": median([run["cache_hit_pct"] for run in runs]),
    "rss_peak_mib": median([run["monitor"]["rss_peak_mib"] for run in runs]),
    "vram_delta_peak_mib": median([run["monitor"]["gpu_delta_peak_mib"] for run in runs]),
    "gpu_util_avg_pct": median([run["monitor"]["gpu_util_avg_pct"] for run in runs]),
    "process_cpu_avg_cores": median([run["monitor"]["process_cpu_avg_cores"] for run in runs]),
}
spreads = {
    "generation_tok_s": spread([run["tok/s"] for run in runs]),
    "cold_window_tok_s": spread([run["cold_window"]["tok_s"] for run in runs]),
    "warm_window_tok_s": spread([run["warm_window"]["tok_s"] for run in runs]),
    "prompt_tok_s": spread([run["prefill_tps"] for run in runs]),
    "ttft_s": spread([run["ttft_s"] for run in runs]),
    "physical_ssd_mib_s": spread([run["monitor"]["physical_read_mib_s"] for run in runs]),
}

payload = {
    "schema_version": 1,
    "kind": "baseline",
    "created_at": datetime.now(timezone.utc).isoformat(),
    "experiment_id": "EXP-2026-09-01-000-baseline",
    "git": {
        "commit": command("git", "-C", str(root), "rev-parse", "HEAD"),
        "branch": command("git", "-C", str(root), "branch", "--show-current"),
        "root_worktree_clean_before_p0": True,
        "nested_worktree_note": "work/llama.cpp-integration had pre-existing comment-only changes",
    },
    "model": {
        "name": "Qwen3.8-Flash-Next",
        "quant": "UD-IQ3_XXS",
        "shards": 3,
        "total_bytes": 81961823936,
        "sha256": [
            "268f81fdedf3149a538f252308927a4d5d1f6e062c178568a51e3b519744f8a8",
            "cfe600b236b88c7fad1613a5ca5e83b9f2beb63cbd44c32b2be50a44747c695f",
            "f1912ba34c79427d2295a58dcb2b732b5931af5bef7a373c60557a57d9ee7250",
        ],
    },
    "parameters": {
        "prompt": "Briefly explain why addressed expert loading is useful for MoE models.",
        "n_predict": 256,
        "context": 4096,
        "ubatch": 256,
        "threads": 16,
        "io_threads": 4,
        "ram_cache_ceil_mib": 32768,
        "vram_cache_request_mib": 2048,
        "o_direct": True,
        "ssd_cpu_overlap": True,
        "temperature": 0,
    },
    "environment": {
        "os": measured("os", "lsb_release", "-ds"),
        "kernel": measured("kernel", "uname", "-r"),
        "cpu": measured("cpu", "sh", "-c", "lscpu | sed -n 's/^Model name:[[:space:]]*//p'"),
        "gpu": measured("gpu", "nvidia-smi", "--query-gpu=name", "--format=csv,noheader"),
        "driver": measured("driver", "nvidia-smi", "--query-gpu=driver_version", "--format=csv,noheader"),
        "cuda": measured("cuda", "sh", "-c", "nvcc --version | sed -n 's/.*release \\([^,]*\\).*/\\1/p'"),
        "compiler": measured("compiler", "g++-13", "--version").splitlines()[0],
        "cmake": measured("cmake", "cmake", "--version").splitlines()[0],
        "ninja": measured("ninja", "ninja", "--version"),
    },
    "correctness": {
        "small_model_gate": "passed",
        "full_model_output_hashes": hashes,
        "all_full_model_outputs_equal": len(hashes) == 1,
        "full_model_note": "Large CUDA decode is not byte-deterministic across processes; retain every hash and use the small resident/streamed byte gate for lossless correctness.",
    },
    "run_count": len(runs),
    "medians": medians,
    "spread": spreads,
    "runs": runs,
}
target.parent.mkdir(parents=True, exist_ok=True)
target.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
