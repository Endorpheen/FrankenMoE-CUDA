#!/usr/bin/env python3
"""Sample the owned Apex server and visible NVIDIA devices during a run."""

import csv
import json
import signal
import subprocess
import sys
import time
from pathlib import Path


def gpu_rows(query):
    result = subprocess.run(
        ["nvidia-smi", query, "--format=csv,noheader,nounits"],
        check=True, capture_output=True, text=True, timeout=4,
    )
    return list(csv.reader(result.stdout.splitlines(), skipinitialspace=True))


def collect(pid):
    status = Path(f"/proc/{pid}/status")
    while not stopping:
        timestamp = time.monotonic_ns()
        try:
            fields = dict(line.split(":", 1) for line in status.read_text().splitlines())
        except FileNotFoundError:
            break

        process_rows = gpu_rows("--query-compute-apps=pid,used_memory")
        allocations = [int(row[1]) for row in process_rows
                       if len(row) == 2 and row[0] == str(pid) and row[1].isdecimal()]
        devices = gpu_rows("--query-gpu=index,memory.used,memory.total")
        sample = {
            "monotonic_ns": timestamp,
            "rss_kib": int(fields["VmRSS"].split()[0]),
            "rss_hwm_kib": int(fields["VmHWM"].split()[0]),
            "swap_kib": int(fields["VmSwap"].split()[0]),
            "threads": int(fields["Threads"]),
            "process_vram_mib": sum(allocations) if allocations else None,
            "devices": [{"index": int(index), "used_mib": int(used), "total_mib": int(total)}
                        for index, used, total in devices],
        }
        print(json.dumps(sample, separators=(",", ":")), flush=True)
        time.sleep(max(0.0, 0.25 - (time.monotonic_ns() - timestamp) / 1e9))


def stop(_signum, _frame):
    global stopping
    stopping = True


if __name__ == "__main__":
    stopping = False
    signal.signal(signal.SIGTERM, stop)
    collect(int(sys.argv[1]))
