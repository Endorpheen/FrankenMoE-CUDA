#!/usr/bin/env bash
# Start the integrated Goose runtime on Apex, exactly as the accepted reference profile does.
#
# Everything here is taken from saved artefacts, nothing is invented:
#   argv      runtime/exp244/results-apex-or244-or-goose/argv.json  (+ --moe-cache-mib 3584)
#   env       runtime/exp244/results-apex-or244-or-goose/environment.json
#   library   loaded-library-hashes.json names libggml-cpu 63ad4984..., which is the file
#             runtime/goose-stage/lib/libggml-cpu.so.0 points at
#
# The runtime directory holds symlinks to the stage-run libraries plus the accepted CPU
# library, so the loader cannot pick the wrong libggml-cpu by SONAME - which is what made
# the first end-to-end run 60% slow (EXP-2026-11-15-709).
#
# Usage: bash runtime/goose-stage/run-apex.sh [--port N]
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
STAGE=$ROOT/runtime/goose-stage
REF=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp244/results-apex-or244-or-goose
BIN=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build/stage-run/llama-server
MODEL=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf

PORT=8081
if [ "${1:-}" = "--port" ] && [ -n "${2:-}" ]; then
    PORT=$2
fi

# Refuse to start on a port that is already serving something: the agreed profile uses
# 8081 and the guard exists so this script cannot disturb a server that is already up.
if (exec 3<>/dev/tcp/127.0.0.1/"$PORT") 2>/dev/null; then
    echo "port $PORT is already in use; stop that server first or pass --port N" >&2
    exit 1
fi

for f in "$BIN" "$MODEL" "$STAGE/lib/libggml-cpu.so.0" "$REF/argv.json" "$REF/environment.json"; do
    if [ ! -e "$f" ]; then
        echo "missing: $f" >&2
        exit 1
    fi
done

# Confirm the loader will take the accepted CPU library, not the other build of the same
# SONAME that also sits in the stage-run tree.
# Two files carry the CPU backend and BOTH are checked, because ggml loads a plugin by
# path as well as resolving a SONAME. The first end-to-end run failed on the SONAME file
# and the second gap was hiding in the plugin file, which was still the slow build:
#
#   libggml-cpu.so.0                        resolved by the dynamic linker
#   libggml-cpu-shared-dd5a40e47065.so.*    loaded by ggml_backend_load_all
#
# Checking only one of them left 176 tok/s of prefill and 8 tok/s of decode on the table.
ACCEPTED=63ad4984881220e55e41463981faa3bba5d4a0d7218880e9b205c8c8c460a9a7
for f in libggml-cpu.so.0 libggml-cpu-shared-dd5a40e47065.so.0.27.0; do
    got=$(sha256sum "$STAGE/lib/$f" | cut -d' ' -f1)
    if [ "$got" != "$ACCEPTED" ]; then
        echo "$f is ${got:0:12}, expected the accepted ${ACCEPTED:0:12}" >&2
        exit 1
    fi
    echo "checked $f ${got:0:12}"
done

export LD_LIBRARY_PATH="$STAGE/lib"
# GGML_BACKEND_PATH was tried here and REMOVED: it made prefill worse (2006 vs 2222) even
# though both CPU files are already the accepted build. Registered twice, the backend is
# apparently initialised twice. Do not re-add it without a measurement that beats 2222.

# The 18 GGML_GOOSE_* switches from the saved environment, applied verbatim so the run is
# the same profile rather than a similar one.
while IFS='=' read -r k v; do
    [ -n "$k" ] && export "$k=$v"
done < <(python3 -c "
import json,sys
e=json.load(open('$REF/environment.json'))
for k,v in e.items():
    if k!='LD_LIBRARY_PATH': print(f'{k}={v}')
")

mapfile -t ARGV < <(python3 -c "
import json
a=json.load(open('$REF/argv.json'))
a[0]='$BIN'
for i,x in enumerate(a):
    if x=='--port': a[i+1]='$PORT'
a+=['--moe-cache-mib','3584']
print('\n'.join(a))
")

echo "runtime : $BIN"
echo "libraries: $STAGE/lib  (libggml-cpu 63ad49848812)"
echo "model   : $MODEL"
echo "port    : $PORT"
echo "open    : http://127.0.0.1:$PORT/"
echo

exec "${ARGV[@]}"
