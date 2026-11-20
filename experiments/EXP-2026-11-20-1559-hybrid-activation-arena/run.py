"""Run one frozen EXP-1476 arm with the experimental compute-buffer overlay."""
from pathlib import Path
import fcntl
import hashlib
import json
import os
import re
import signal
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request

if len(sys.argv) != 3 or sys.argv[1] not in {"a", "b"}:
    raise SystemExit("usage: run.py a|b EXPERIMENT_DIRECTORY")
arm, root = sys.argv[1], Path(sys.argv[2])
root = root.resolve()
out = root / arm
out.mkdir()
release = Path("/home/end0/AI/LlamaServer/frankenmoe-release-20261120-exp1476")
archive = Path("/home/end0/AI/FrankenMoE-CUDA/results/apex-runs/rel-exp1476-20261119-212613")
binary = release / "llama-server"
request = (archive / "request.json").read_bytes()
assert hashlib.sha256(request).hexdigest() == "484a12de00ec057e4b78d480ebb185153bb10af607eb23192a72995ba3026d0f"
server_args = (archive / "argv.txt").read_text().splitlines()
assert Path(server_args[0]) == binary
out.joinpath("request.json").write_bytes(request)
env = {key: value for key, value in os.environ.items()
       if not key.startswith(("GOOSE_", "GGML_", "FG_")) and key not in {"LD_PRELOAD", "LD_LIBRARY_PATH"}}
env.update(LD_LIBRARY_PATH=str(release), LD_PRELOAD=str(root / "arena-overlay.so"),
           FG_EXP1476_WIDE_ARENA="1" if arm == "b" else "0")
command = ["nsys", "profile", "--sample=none", "--cpuctxsw=none", "--trace=cuda,osrt",
           "--cuda-graph-trace=node", "--export=sqlite", f"--output={out / 'trace'}", *server_args]

def query(path, body=None, timeout=4):
    req = urllib.request.Request(f"http://127.0.0.1:9101/{path}", body, {"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as response:
        return response.read()

def swapout():
    for line in Path("/proc/vmstat").read_text().splitlines():
        if line.startswith("pswpout "):
            return int(line.split()[1])
    raise RuntimeError("pswpout unavailable")

def server_pid():
    matches = []
    for pid in Path("/proc").iterdir():
        if not pid.name.isdecimal():
            continue
        try:
            if pid.joinpath("exe").resolve(strict=True) != binary:
                continue
            argv = pid.joinpath("cmdline").read_bytes().split(b"\0")
            if b"--port" in argv and argv[argv.index(b"--port") + 1] == b"9101":
                matches.append(int(pid.name))
        except OSError:
            continue
    if len(matches) != 1:
        raise RuntimeError("could not identify the isolated server")
    return matches[0]

def gpu(query_name):
    text = subprocess.check_output(
        ["nvidia-smi", query_name, "--format=csv,noheader,nounits"], text=True, timeout=8
    )
    return [line.split(",") for line in text.splitlines()]

def sample(pid):
    values = {}
    for line in Path(f"/proc/{pid}/status").read_text().splitlines():
        key, _, value = line.partition(":")
        if key in {"VmRSS", "VmHWM", "VmSwap"}:
            values[key] = int(value.split()[0])
    values["process_vram_mib"] = next(
        (int(used) for who, used in gpu("--query-compute-apps=pid,used_memory")
         if who.strip() == str(pid) and used.strip().isdigit()), None
    )
    values["card_vram_mib"] = int(gpu("--query-gpu=memory.used")[0][0])
    values["t"] = time.monotonic()
    return values

samples = []
errors = []
stop = threading.Event()
thread = None
pid = None
metadata = {"arm": arm, "command": command, "swapout_before": swapout(),
            "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
            "overlay_sha256": hashlib.sha256((root / "arena-overlay.so").read_bytes()).hexdigest(),
            "widen": env["FG_EXP1476_WIDE_ARENA"]}

def monitor(pid):
    while not stop.is_set():
        try:
            samples.append(sample(pid))
        except (OSError, subprocess.SubprocessError, ValueError) as error:
            errors.append(str(error))
        stop.wait(1)

with open("/tmp/apex-test.lock", "a") as lock:
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    if subprocess.check_output(["ss", "-H", "-ltn", "sport = :9101"], text=True).strip():
        raise RuntimeError("port 9101 occupied")
    print(out, flush=True)
    with (out / "server.log").open("wb") as log:
        process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            for _ in range(150):
                if process.poll() is not None:
                    raise RuntimeError(f"reference process exited {process.returncode}")
                try:
                    (out / "ready.json").write_bytes(query("health"))
                    break
                except (OSError, urllib.error.URLError):
                    time.sleep(1)
            else:
                raise RuntimeError("readiness timeout")
            metadata["swapout_at_ready"] = swapout()
            pid = server_pid()
            metadata["pid"] = pid
            (out / "maps.txt").write_text(Path(f"/proc/{pid}/maps").read_text())
            thread = threading.Thread(target=monitor, args=(pid,), daemon=True)
            thread.start()
            start = time.monotonic()
            response = query("completion", request, timeout=480)
            metadata["request_wall_seconds"] = time.monotonic() - start
            (out / "response.json").write_bytes(response)
            samples.append(sample(pid))
            result = json.loads(response)
            timings = result["timings"]
            assert (timings["prompt_n"], timings["predicted_n"], timings["cache_n"]) == (32768, 512, 0)
            assert len(result["tokens"]) == 512
            print(arm, timings["prompt_per_second"], timings["predicted_per_second"],
                  "RSS high-water KiB", max(s["VmHWM"] for s in samples), flush=True)
        finally:
            stop.set()
            if thread is not None:
                thread.join(timeout=10)
            if process.poll() is None:
                if pid is not None and Path(f"/proc/{pid}").exists():
                    os.kill(pid, signal.SIGINT)
                else:
                    os.killpg(process.pid, signal.SIGTERM)
                process.wait(timeout=120)
            metadata["swapout_after"] = swapout()
            metadata["sample_errors"] = errors
            (out / "run.json").write_text(json.dumps(metadata, indent=2) + "\n")
            (out / "memory.jsonl").write_text("".join(json.dumps(row) + "\n" for row in samples))

stdout = (out / "server.log").read_text(errors="replace")
arena_events = [
    dict(zip(("requested", "target", "allocated"), map(int, row)))
    for row in re.findall(r"HYBRID_ARENA requested=(\d+) target=(\d+) allocated=(\d+)", stdout)
]
assert arena_events, "the compute-buffer hook did not observe a CUDA0 allocation"
(out / "arena-events.json").write_text(json.dumps(arena_events, indent=2) + "\n")
assert (out / "trace.sqlite").is_file()
print(arm, "CUDA compute allocations", arena_events, flush=True)
