"""Start only the staged Linux processes and verify deterministic HTTP generation."""
from pathlib import Path
import hashlib
import json
import os
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request


stage, root, output = (Path(arg).resolve() for arg in sys.argv[1:4])
requests = {
    "apex": "build/checkpoint-apex-exp1880-before-inkling/evidence/request.json",
    "inkling": "build/checkpoint-inkling-exp1969-before-bonsai/request.json",
    "bonsai": "experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json",
}
output.mkdir(parents=True, exist_ok=True)
receipt = output / "linux-smoke.json"
report = json.loads(receipt.read_text()) if receipt.exists() else {}
selected = sys.argv[4:] or list(requests)
model_dir = Path(os.environ.get("FRANKENMOE_MODELS_DIR", str(Path.home() / "AI/LocalAI/Models")))
weight_files = {
    "apex": "Apex-35B-V1.0-Q4_K_M.gguf",
    "inkling": "Inkling-2-Mini-AD-IQ4_XS.gguf",
    "bonsai": "Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf",
}
for name in selected:
    filename = requests[name]
    model = model_dir / weight_files[name]
    if name in ("apex", "inkling"):
        with model.open("rb") as file:
            while file.read(8 * 1024 * 1024):
                pass
        resident, size = map(int, subprocess.check_output(
            ["fincore", "-b", "-n", "-o", "RES,SIZE", str(model)], text=True).split())
        if resident < size:
            raise RuntimeError(f"{name} requires resident weights for this profile: {resident}/{size}")
        print(f"{name} resident: {resident}/{size}", flush=True)
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    command = [str(stage / "frankenmoe"), name, str(model), str(port)]
    env = {
        "PATH": "/usr/bin:/bin", "HOME": os.environ["HOME"], "LANG": "C",
        "CUDA_VISIBLE_DEVICES": "0", "FRANKENMOE_CONSOLE": "0",
    }
    request = json.loads((root / filename).read_text())
    request.update(prompt=request["prompt"][:129], n_predict=4, stream=False)
    url = f"http://127.0.0.1:{port}"
    with (output / f"{name}.log").open("w") as log:
        proc = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 55
            while True:
                if proc.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError(f"{name} exited or failed to listen")
                try:
                    with urllib.request.urlopen(url + "/health", timeout=1) as response:
                        assert response.status == 200
                        break
                except (OSError, urllib.error.URLError):
                    time.sleep(.2)

            def generate():
                data = urllib.request.Request(url + "/completion", json.dumps(request).encode(),
                                             {"Content-Type": "application/json"})
                with urllib.request.urlopen(data, timeout=70) as response:
                    return json.load(response)

            a, b = generate(), generate()
            assert a["tokens"] == b["tokens"] and len(a["tokens"]) == 4, (name, a, b)
            assert a["content"] == b["content"]
            report[name] = {
                "token_ids": a["tokens"], "text_sha256": hashlib.sha256(a["content"].encode()).hexdigest(),
                "prompt_tokens": 129, "repeat_match": True, "health": 200,
            }
            print(name, report[name], flush=True)
            receipt.write_text(json.dumps(report, indent=2) + "\n")
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(10)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
