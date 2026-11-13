#!/bin/sh
# Memory-fit comparison on Apex. Runs only after Igor approves: 2 loads (one per line),
# no generation, logs the fit decisions only.
set -u
M=${APEX_MODEL:?set APEX_MODEL to the Apex gguf path}
N=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin
S=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-340-p6-03-request-trace/stage
cd "$(dirname "$0")" || exit 1
for arm in native ours; do
    if [ "$arm" = native ]; then B=$N/llama-server; else B=$S/llama-server; fi
    for mode in on off; do
        env -i PATH=/usr/bin:/bin HOME=/tmp LD_LIBRARY_PATH=$(dirname "$B") \
            timeout 900 "$B" -m "$M" -fit "$mode" --port 0 \
            > "fitapex-$arm-$mode.log" 2>&1
        echo "$arm fit=$mode rc=$?"
        grep -viE '^FG' "fitapex-$arm-$mode.log" | grep -iE 'fit|offload|buffer size|n_ctx' | head -8
    done
done
