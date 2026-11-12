import json, subprocess, sys, time, urllib.request, os

STAGE = "/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-340-p6-03-request-trace/stage"
NATIVE = "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL = "/home/end0/.unsloth/.cache/stories260K.gguf"

CASES = [
    {"name": "greedy",   "temp": 0.0, "top_k": 1, "seed": 42},
    {"name": "seed42",   "temp": 0.8, "top_k": 40, "top_p": 0.9, "seed": 42},
    {"name": "seed43",   "temp": 0.8, "top_k": 40, "top_p": 0.9, "seed": 43},
    {"name": "topk1",    "temp": 1.0, "top_k": 1, "seed": 7},
    {"name": "minp",     "temp": 0.7, "top_k": 0, "min_p": 0.1, "seed": 11},
]

def run(arm, port):
    binpath = os.path.join(NATIVE if arm == "native" else STAGE, "llama-server")
    env = {"PATH": "/usr/bin:/bin", "HOME": "/tmp", "LD_LIBRARY_PATH": os.path.dirname(binpath)}
    p = subprocess.Popen([binpath, "-m", MODEL, "-c", "512", "-np", "1", "--port", str(port), "-t", "4"],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
    out = {}
    try:
        for _ in range(120):
            try:
                urllib.request.urlopen(f"http://127.0.0.1:{port}/health", timeout=1); break
            except Exception: time.sleep(0.5)
        for c in CASES:
            body = {"prompt": "Once upon a time", "n_predict": 16, "temperature": c["temp"],
                    "top_k": c["top_k"], "seed": c["seed"], "cache_prompt": False}
            for k in ("top_p", "min_p"):
                if k in c: body[k] = c[k]
            req = urllib.request.Request(f"http://127.0.0.1:{port}/completion",
                                         data=json.dumps(body).encode(), headers={"Content-Type": "application/json"})
            with urllib.request.urlopen(req, timeout=120) as r:
                out[c["name"]] = json.load(r)["content"]
    finally:
        p.terminate(); p.wait(timeout=20)
    return out

a = run("native", 18101)
b = run("ours", 18102)
bad = 0
for c in CASES:
    n = c["name"]
    same = a[n] == b[n]
    bad += 0 if same else 1
    print(f"{n:8s} {'SAME' if same else 'DIFF'}")
    if not same:
        print(f"   native: {a[n]!r}")
        print(f"   ours  : {b[n]!r}")
print("cases", len(CASES), "differences", bad)
sys.exit(1 if bad else 0)
