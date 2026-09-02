#!/usr/bin/env python3
"""Run three interleaved warm-decode pairs with 12 and 16 CPU threads."""

import argparse
import json
import os
import statistics
import subprocess
import sys


def summarize(rows):
    warm = [row["warm"] for row in rows]
    speeds = [row["generation_tok_s"] for row in warm]
    return {
        "generation_tok_s_median": statistics.median(speeds),
        "generation_tok_s_min": min(speeds),
        "generation_tok_s_max": max(speeds),
        "cpu_cores_mean_median": statistics.median(row["cpu_cores_mean"] for row in warm),
        "gpu_util_pct_mean_median": statistics.median(row["gpu_util_pct_mean"] for row in warm),
        "physical_read_mib_median": statistics.median(row["physical_read_mib"] for row in warm),
        "major_faults_median": statistics.median(row["major_faults"] for row in warm),
        "rss_peak_mib_median": statistics.median(row["rss_peak_mib"] for row in warm),
        "swap_peak_mib": max(row["swap_peak_mib"] for row in warm),
        "hashes": [row["text_sha256"] for row in warm],
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("model")
    parser.add_argument("--output", default="benchmarks/exp013-cpu-threads-12-vs-16.json")
    args = parser.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    profiler = os.path.join(root, "scripts", "profile_expert_tier.py")
    output = args.output if os.path.isabs(args.output) else os.path.join(root, args.output)
    model = os.path.abspath(args.model)
    arms = {"12": [], "16": []}

    for pair in range(1, 4):
        order = (12, 16) if pair % 2 else (16, 12)
        for threads in order:
            run_output = f"/tmp/franken-exp013-p{pair}-t{threads}-repeat.json"
            env = os.environ.copy()
            env.pop("LLAMA_CPU_MOE_PREFETCH", None)
            command = [
                sys.executable,
                profiler,
                model,
                "--threads",
                str(threads),
                "--output",
                run_output,
            ]
            print(f"pair {pair}/3, {threads} threads", flush=True)
            subprocess.run(command, cwd=root, env=env, check=True)
            with open(run_output, encoding="utf-8") as source:
                data = json.load(source)
            by_tag = {row["tag"]: row for row in data["requests"]}
            arms[str(threads)].append({
                "pair": pair,
                "warmup": by_tag["cold"],
                "warm": by_tag["warm"],
            })

    pair_changes = []
    for pair in range(3):
        speed_12 = arms["12"][pair]["warm"]["generation_tok_s"]
        speed_16 = arms["16"][pair]["warm"]["generation_tok_s"]
        pair_changes.append((speed_12 / speed_16 - 1) * 100)

    result = {
        "schema_version": 1,
        "experiment_id": "EXP-2026-09-02-013-cpu-thread-scaling",
        "pairs": 3,
        "protocol": "interleaved 12/16 threads, fresh server, one warmup then one measured 256-token request",
        "arms": arms,
        "summary": {threads: summarize(rows) for threads, rows in arms.items()},
        "paired_speed_change_pct_12_vs_16": pair_changes,
        "paired_speed_change_pct_median": statistics.median(pair_changes),
    }
    os.makedirs(os.path.dirname(output), exist_ok=True)
    with open(output, "w", encoding="utf-8") as out:
        json.dump(result, out, indent=2)
    print(json.dumps(result["summary"], indent=2), flush=True)
    print(json.dumps({"paired_speed_change_pct_12_vs_16": pair_changes}, indent=2), flush=True)
    print(f"saved {output}", flush=True)


if __name__ == "__main__":
    main()
