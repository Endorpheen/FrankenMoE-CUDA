#!/usr/bin/env python3
"""Profile cold and warm expert-tier requests without changing inference."""

import argparse
import hashlib
import json
import os
import re
import signal
import statistics
import subprocess
import threading
import time
import urllib.request


def proc_snapshot(pid):
    stat_text = open(f"/proc/{pid}/stat", encoding="utf-8").read()
    fields = stat_text[stat_text.rfind(")") + 2:].split()
    status = open(f"/proc/{pid}/status", encoding="utf-8").read()
    io_text = open(f"/proc/{pid}/io", encoding="utf-8").read()
    status_fields = dict(re.findall(r"(VmRSS|VmSwap):\s+(\d+)", status))
    io_fields = dict(re.findall(r"(read_bytes):\s+(\d+)", io_text))
    return {
        "time": time.time(),
        "minor_faults": int(fields[7]),
        "major_faults": int(fields[9]),
        "cpu_ticks": int(fields[11]) + int(fields[12]),
        "rss_kib": int(status_fields.get("VmRSS", 0)),
        "swap_kib": int(status_fields.get("VmSwap", 0)),
        "read_bytes": int(io_fields.get("read_bytes", 0)),
    }


def gpu_snapshot():
    text = subprocess.check_output([
        "nvidia-smi",
        "--query-gpu=utilization.gpu,memory.used",
        "--format=csv,noheader,nounits",
    ], text=True)
    util, memory = text.splitlines()[0].split(",")
    return {"gpu_util_pct": int(util.strip()), "gpu_used_mib": int(memory.strip())}


class Sampler(threading.Thread):
    def __init__(self, pid):
        super().__init__(daemon=True)
        self.pid = pid
        self.phase = "idle"
        self.rows = []
        self.stopped = False

    def run(self):
        while not self.stopped:
            try:
                row = proc_snapshot(self.pid)
                row.update(gpu_snapshot())
                row["phase"] = self.phase
                self.rows.append(row)
            except (OSError, subprocess.SubprocessError, ValueError):
                pass
            time.sleep(0.5)


def summarize_request(before, after, samples, timings, text, tag):
    clock_ticks = os.sysconf(os.sysconf_names["SC_CLK_TCK"])
    cpu_seconds = (after["cpu_ticks"] - before["cpu_ticks"]) / clock_ticks
    measured_s = (timings["prompt_ms"] + timings["predicted_ms"]) / 1000
    gpu_utils = [row["gpu_util_pct"] for row in samples]
    rss_values = [row["rss_kib"] for row in samples]
    gpu_memory = [row["gpu_used_mib"] for row in samples]
    return {
        "tag": tag,
        "generation_tok_s": timings["predicted_per_second"],
        "prompt_tok_s": timings["prompt_per_second"],
        "completion_tokens": timings["predicted_n"],
        "text_sha256": hashlib.sha256(text.encode()).hexdigest(),
        "measured_s": measured_s,
        "cpu_seconds": cpu_seconds,
        "cpu_cores_mean": cpu_seconds / measured_s,
        "minor_faults": after["minor_faults"] - before["minor_faults"],
        "major_faults": after["major_faults"] - before["major_faults"],
        "physical_read_mib": (after["read_bytes"] - before["read_bytes"]) / (1024 * 1024),
        "rss_peak_mib": max(rss_values, default=after["rss_kib"]) / 1024,
        "swap_peak_mib": max((row["swap_kib"] for row in samples), default=after["swap_kib"]) / 1024,
        "gpu_util_pct_mean": statistics.fmean(gpu_utils) if gpu_utils else None,
        "gpu_util_pct_peak": max(gpu_utils, default=None),
        "gpu_used_mib_peak": max(gpu_memory, default=None),
    }


def chat(api, prompt, max_tokens):
    body = json.dumps({
        "messages": [{"role": "user", "content": prompt}],
        "temperature": 0.0,
        "max_tokens": max_tokens,
        "stream": True,
        "stream_options": {"include_usage": True},
    }).encode()
    request = urllib.request.Request(
        api + "/v1/chat/completions", data=body,
        headers={"Content-Type": "application/json"})
    pieces = []
    timings = None
    with urllib.request.urlopen(request, timeout=1800) as response:
        for raw in response:
            line = raw.decode("utf-8", "replace").strip()
            if not line.startswith("data: ") or line[6:] == "[DONE]":
                continue
            event = json.loads(line[6:])
            timings = event.get("timings") or timings
            for choice in event.get("choices") or []:
                delta = choice.get("delta") or {}
                pieces.append(delta.get("content") or delta.get("reasoning_content") or "")
    if timings is None:
        raise RuntimeError("server response did not include timings")
    return timings, "".join(pieces)


def drop_model_cache(model):
    match = re.search(r"(\d{5})-of-(\d{5})", model)
    paths = [model]
    if match:
        count = int(match.group(2))
        paths = [model[:match.start(1)] + f"{index:05d}" + model[match.end(1):]
                 for index in range(1, count + 1)]
    for path in paths:
        descriptor = os.open(path, os.O_RDONLY)
        try:
            os.posix_fadvise(descriptor, 0, 0, os.POSIX_FADV_DONTNEED)
        finally:
            os.close(descriptor)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("model")
    parser.add_argument("--server", default="build/expert-tier-franken-cuda/bin/llama-server")
    parser.add_argument("--output", default="benchmarks/exp011-profile.json")
    parser.add_argument("--port", type=int, default=8095)
    parser.add_argument("--drop-model-cache", action="store_true")
    args = parser.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    server = args.server if os.path.isabs(args.server) else os.path.join(root, args.server)
    model = os.path.abspath(args.model)
    output = args.output if os.path.isabs(args.output) else os.path.join(root, args.output)
    log_path = os.path.join(root, "results", "exp011-profile-server.log")
    os.makedirs(os.path.dirname(output), exist_ok=True)
    api = f"http://127.0.0.1:{args.port}"
    if args.drop_model_cache:
        drop_model_cache(model)
    command = [
        "setsid", "nohup", server, "-m", model, "-ngl", "99", "--cpu-moe",
        "-ehs", "0", "-ot", "per_layer_token_embd.weight=CPU", "-c", "64000",
        "-fa", "on", "--jinja", "-t", "16", "--host", "127.0.0.1",
        "--port", str(args.port), "-ctk", "q4_0", "-ctv", "q4_0",
        "--reasoning-effort", "low",
    ]
    with open(log_path, "w", encoding="utf-8") as log:
        process = subprocess.Popen(
            command, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
    sampler = Sampler(process.pid)
    sampler.start()
    try:
        deadline = time.time() + 600
        while time.time() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f"server exited with status {process.returncode}")
            try:
                urllib.request.urlopen(api + "/health", timeout=2).read()
                break
            except Exception:
                time.sleep(1)
        else:
            raise RuntimeError("server did not become ready")

        prompt = "Briefly explain why addressed expert loading is useful for MoE models."
        requests = []
        for tag in ("cold", "warm"):
            sampler.phase = tag
            before = proc_snapshot(process.pid)
            timings, text = chat(api, prompt, 256)
            after = proc_snapshot(process.pid)
            phase_samples = [row for row in sampler.rows if row["phase"] == tag]
            requests.append(summarize_request(before, after, phase_samples, timings, text, tag))
            sampler.phase = "idle"

        result = {
            "experiment_id": "EXP-2026-09-02-011-expert-tier-bottleneck-profile",
            "server": server,
            "model": model,
            "command": command,
            "cpu_moe_prefetch": os.getenv("LLAMA_CPU_MOE_PREFETCH", "0"),
            "model_cache_dropped": args.drop_model_cache,
            "requests": requests,
            "samples": sampler.rows,
            "server_log": log_path,
        }
        with open(output, "w", encoding="utf-8") as out:
            json.dump(result, out, indent=2)
        for row in requests:
            print(json.dumps(row, sort_keys=True))
        print(f"saved {output}")
    finally:
        sampler.stopped = True
        sampler.join(timeout=2)
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGINT)
            try:
                process.wait(20)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGTERM)
                process.wait(10)


if __name__ == "__main__":
    main()
