#!/usr/bin/env bash
# Refuse to benchmark Apex unless the accepted EXP906 runtime is intact.
#
# Usage: bash tools/verify-apex-reference.sh [run-dir]
# Default run-dir: /home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN=${1:-/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run}
python3 - "$ROOT/docs/reference/apex-exp906/manifest.json" "$RUN" <<'PY'
import hashlib, json, os, sys
manifest = json.load(open(sys.argv[1]))
run = sys.argv[2]
bad = []
for name, meta in manifest["files"].items():
    path = os.path.join(run, name)
    if not os.path.exists(path):
        bad.append(f"missing {name}")
        continue
    h = hashlib.sha256(open(path, "rb").read()).hexdigest()
    if h != meta["sha256"]:
        bad.append(f"{name}: sha256 {h} != {meta['sha256']}")
cuda = manifest["cuda_library_sha256"]
cpath = os.path.join(run, "libggml-cuda.so.0")
if os.path.exists(cpath):
    h = hashlib.sha256(open(cpath, "rb").read()).hexdigest()
    if h != cuda:
        bad.append(f"libggml-cuda.so.0: not the accepted EXP906 library ({h})")
if bad:
    print("REFERENCE FAILED:")
    for b in bad:
        print("  " + b)
    sys.exit(1)
print(f"reference ok: {len(manifest['files'])} files match, CUDA {cuda[:12]}")
PY
