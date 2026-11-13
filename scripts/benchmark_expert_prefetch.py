#!/usr/bin/env python3
"""Run five interleaved cold/warm pairs for selected-expert prefetch."""

import argparse
import json
import os
import statistics
import subprocess
import sys


def summary(rows, tag):
    selected = [run[tag] for run in rows]
    speeds = [row["generation_tok_s"] for row in selected]
    return {
        "generation_tok_s_median": statistics.median(speeds),
        "generation_tok_s_min": min(speeds),
        "generation_tok_s_max": max(speeds),
        "physical_read_mib_median": statistics.median(row["physical_read_mib"] for row in selected),
        "major_faults_median": statistics.median(row["major_faults"] for row in selected),
        "rss_peak_mib_median": statistics.median(row["rss_peak_mib"] for row in selected),
        "swap_peak_mib": max(row["swap_peak_mib"] for row in selected),
        "hashes": [row["text_sha256"] for row in selected],
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("model")
    parser.add_argument("--output", default="benchmarks/exp012-selected-expert-madvise.json")
    args = parser.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    profiler = os.path.join(root, "scripts", "profile_expert_tier.py")
    output = args.output if os.path.isabs(args.output) else os.path.join(root, args.output)
    model = os.path.abspath(args.model)
    arms = {"off": [], "prefetch": []}

    for pair in range(1, 6):
        order = ("off", "prefetch") if pair % 2 else ("prefetch", "off")
        for arm in order:
            run_output = f"/tmp/franken-exp012-p{pair}-{arm}.json"
            env = os.environ.copy()
            if arm == "prefetch":
                env["LLAMA_CPU_MOE_PREFETCH"] = "1"
            else:
                env.pop("LLAMA_CPU_MOE_PREFETCH", None)
            command = [
                sys.executable, profiler, model, "--drop-model-cache",
                "--output", run_output,
            ]
            print(f"pair {pair}/5, {arm}", flush=True)
            subprocess.run(command, cwd=root, env=env, check=True)
            data = json.load(open(run_output, encoding="utf-8"))
            by_tag = {row["tag"]: row for row in data["requests"]}
            arms[arm].append({"pair": pair, "cold": by_tag["cold"], "warm": by_tag["warm"]})

    result = {
        "experiment_id": "EXP-2026-09-02-012-selected-expert-madvise",
        "pairs": 5,
        "protocol": "interleaved off/prefetch, model cache dropped before each server, cold then warm 256-token requests",
        "arms": arms,
        "summary": {
            arm: {tag: summary(rows, tag) for tag in ("cold", "warm")}
            for arm, rows in arms.items()
        },
    }
    os.makedirs(os.path.dirname(output), exist_ok=True)
    with open(output, "w", encoding="utf-8") as out:
        json.dump(result, out, indent=2)
    print(json.dumps(result["summary"], indent=2), flush=True)
    print(f"saved {output}", flush=True)


if __name__ == "__main__":
    main()
