#!/usr/bin/env bash
# Run the EXP-244 configuration exactly as it was measured at 45.68 tok/s, and keep the full command.
#
# Why this exists: the 45.68 run was NOT the build under runtime/reconstructed. It was libllama-exp244.so with
# LD_LIBRARY_PATH pointing at exp244/run and 20 GGML_GOOSE_* variables exported. Comparing anything against that number
# without reproducing this exact combination is meaningless, and loose shell one-liners lost it.
#
# The script writes every detail of what it ran into its own directory, so a later check can prove which binary, which
# libraries and which environment produced a number - the failure mode this project keeps repeating.
#
# Usage: bash tools/run-exp244.sh <tag>
set -euo pipefail

TAG=${1:?usage: run-exp244.sh <tag>}
PORT=${PORT:-9101}
R=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
RUN=${R}/runtime/exp244
ENVJSON=${RUN}/results-apex-or244-or-goose/environment.json
ARGVJSON=${RUN}/results-apex-or244-or-goose/argv.json
LOCK=/tmp/apex-test.lock
DIR=${PWD}/results/exp244-runs/${TAG}-$(date +%Y%m%d-%H%M%S)
mkdir -p "${DIR}"

exec 9>"${LOCK}"
flock -w 3600 9 || { echo "lock timeout"; exit 3; }
echo "[$(date +%H:%M:%S)] lock held" | tee "${DIR}/run.log"

cleanup() {
    local rc=$?
    if [ -n "${SRV:-}" ]; then
        kill -TERM -- "-${SRV}" 2>/dev/null || true
        for _ in 1 2 3 4 5 6 7 8 9 10; do kill -0 "${SRV}" 2>/dev/null || break; sleep 0.5; done
        kill -KILL -- "-${SRV}" 2>/dev/null || true
        wait "${SRV}" 2>/dev/null || true
    fi
    ss -ltn 2>/dev/null | grep -qE ":${PORT}\$" \
        && echo "PORT STILL HELD" | tee -a "${DIR}/run.log" \
        || echo "[$(date +%H:%M:%S)] port ${PORT} released" | tee -a "${DIR}/run.log"
    exit ${rc}
}
trap cleanup EXIT INT TERM

# refuse to start over a leftover, identified by the socket owner rather than by name
if ss -ltn 2>/dev/null | grep -qE ":${PORT}\$"; then
    echo "REFUSING: port ${PORT} owned by $(ss -ltnp 2>/dev/null | grep -oE 'pid=[0-9]+' | head -1)" | tee "${DIR}/run.log"
    exit 2
fi

# The exact command, saved before it runs.
python3 - "${ENVJSON}" "${ARGVJSON}" "${RUN}" "${PORT}" "${DIR}" <<'PY'
import json, sys
env_json, argv_json, run, port, out = sys.argv[1:6]
env = json.load(open(env_json))
argv = json.load(open(argv_json))
argv[0] = f"{run}/run/llama-server"
argv[argv.index("--port") + 1] = port
lines = [f"{k}={v}" for k, v in env.items() if k != "LD_LIBRARY_PATH"]
open(f"{out}/environment.txt", "w").write("\n".join(lines) + "\n")
open(f"{out}/argv.txt", "w").write(" ".join(argv) + "\n")
open(f"{out}/libpath.txt", "w").write(env["LD_LIBRARY_PATH"] + "\n")
PY
echo "[$(date +%H:%M:%S)] command saved to ${DIR}/argv.txt, environment.txt, libpath.txt" | tee -a "${DIR}/run.log"

LIBPATH=$(cat "${DIR}/libpath.txt")
ARGV=$(cat "${DIR}/argv.txt")
# shellcheck disable=SC2086
( set -a; . <(sed 's/^/export /' "${DIR}/environment.txt"); set +a
  export LD_LIBRARY_PATH="${LIBPATH}"
  exec setsid ${ARGV} ) > "${DIR}/server.log" 2>&1 &
SRV=$!
echo "[$(date +%H:%M:%S)] server pid ${SRV}" | tee -a "${DIR}/run.log"

READY=0
for _ in $(seq 1 120); do
    curl -s -m 2 "http://127.0.0.1:${PORT}/health" 2>/dev/null | grep -q ok && { READY=1; break; }
    kill -0 "${SRV}" 2>/dev/null || break
    sleep 2
done
[ "${READY}" -eq 1 ] || { echo "SERVER NOT READY" | tee -a "${DIR}/run.log"; tail -20 "${DIR}/server.log" > "${DIR}/diagnostics.txt"; exit 4; }
echo "[$(date +%H:%M:%S)] server ready" | tee -a "${DIR}/run.log"

for i in 1 2 3; do
    curl -s -m 1800 -X POST "http://127.0.0.1:${PORT}/completion" -H 'Content-Type: application/json' \
        --data-binary @/tmp/req512.json -o "${DIR}/r${i}.json" || true
done

python3 - "${DIR}" <<'PY'
import json, hashlib, sys, pathlib
d = pathlib.Path(sys.argv[1]); dec, pre, hs = [], [], set()
for i in (1, 2, 3):
    try:
        r = json.load(open(d/f"r{i}.json")); t = r["timings"]
        dec.append(round(t["predicted_per_second"], 2)); pre.append(round(t["prompt_per_second"], 1))
        hs.add(hashlib.sha256(r["content"].encode()).hexdigest()[:12])
    except Exception:
        pass
(d/"summary.txt").write_text(f"decode {dec} prefill {pre} sha {hs}\n")
print(f"decode {dec} prefill {pre} sha {hs}")
PY
