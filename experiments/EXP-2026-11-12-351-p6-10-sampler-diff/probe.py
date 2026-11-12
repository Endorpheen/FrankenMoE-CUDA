import json, subprocess, sys, time, urllib.request, os

STAGE = "/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-340-p6-03-request-trace/stage"
NATIVE = "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin"
MODEL = "/home/end0/.unsloth/.cache/stories260K.gguf"

CASES = [
    {"name": "hot_base",  "temp": 1.5, "top_k": 0, "seed": 42, "n": 64},
    {"name": "hot_other", "temp": 1.5, "top_k": 0, "seed": 99, "n": 64},
    {"name": "hot_miro1", "temp": 1.5, "top_k": 40, "seed": 42, "n": 64, "extra": {"mirostat": 1, "mirostat_tau": 5.0, "mirostat_eta": 0.1}},
    {"name": "hot_miro2", "temp": 1.5, "top_k": 40, "seed": 42, "n": 64, "extra": {"mirostat": 2, "mirostat_tau": 5.0, "mirostat_eta": 0.1}},
    {"name": "hot_typ",   "temp": 1.5, "top_k": 0, "seed": 42, "n": 64, "extra": {"typical_p": 0.5}},
    {"name": "hot_xtc",   "temp": 1.5, "top_k": 0, "seed": 42, "n": 64, "extra": {"xtc_probability": 1.0, "xtc_threshold": 0.1}},
    {"name": "hot_dry",   "temp": 1.5, "top_k": 0, "seed": 42, "n": 64, "extra": {"dry_multiplier": 2.0, "dry_base": 1.75, "dry_allowed_length": 2, "dry_penalty_last_n": 64}},
    {"name": "hot_sigma", "temp": 1.5, "top_k": 0, "seed": 42, "n": 64, "extra": {"top_n_sigma": 1.0}},
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
            body = {"prompt": "Once upon a time", "n_predict": c.get("n", 16), "temperature": c["temp"],
                    "top_k": c["top_k"], "seed": c["seed"], "cache_prompt": False}
            for k in ("top_p", "min_p"):
                if k in c: body[k] = c[k]
            if c.get("bias"):
                body["logit_bias"] = [[ord("e"), -5.0]]
            if c.get("extra"):
                body.update(c["extra"])
            if c.get("grammar"):
                body["grammar"] = c["grammar"]
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
import json as _j
_j.dump({"native": a, "ours": b}, open("outputs.json", "w"), indent=1)
uniq = {}
for n, v in a.items(): uniq.setdefault(v, []).append(n)
print("distinct outputs:", len(uniq), "of", len(CASES))
for v, names in uniq.items():
    if len(names) > 1: print("  identical group:", names)
print("cases", len(CASES), "differences", bad)
sys.exit(1 if bad else 0)
