#!/usr/bin/env bash
# Run the standalone harnesses that need the real Apex GGUF.
#
# Why this exists: integration/goose-standalone/tests/ holds 23 harnesses and the build ran exactly two of
# them (graph_exec, tokenizer_decisions - the ones that need no model). The rest were never executed by
# anything, so a pass or fail in them was not known. Running them found two passing and one failing on the
# first attempt.
#
# This is deliberately NOT part of build-standalone-runtime.sh: each run reads a 21.7 GB file and takes
# tens of seconds, which is too heavy for a build check (rule 18) and needs a reason to run.
#
# Usage: bash tools/verify-standalone-apex.sh [--model PATH] [harness...]
#        (no harness names = every one in the list below)
set -uo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
MODEL=${FG_APEX_GGUF:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
SUB=${FG_SUBSTRATE:-/tmp/opencode/sub-661/tree/build-off/llama.cpp/ggml/src}
W=$ROOT/builds/goose-standalone
OUT=$(mktemp -d /tmp/standalone-apex.XXXXXX)
trap 'rm -rf "$OUT"' EXIT

# Harnesses that take a model path and assert something about the real file. A harness whose name is added
# here is expected to PASS; one that does not pass is reported as a failure, not skipped.
HARNESSES=(
    # fast: no model read
    qg_split
    gdn_ops
    gdn_recur
    moe_block
    layer_orch
    kv_cache
    state_slot
    state_life
    state_reuse
    prefill_decode
    # read the GGUF
    apex_plan
    apex_shape
    q35_shape
    gdn_plan
    gdn_planfile
    stack_dispatch
    real_values
    real_chain
    apex_layer
    apex_graph
    gdn_layer
    gdn_layer_full
    # layer_real is NOT run: it is superseded by apex_layer, which uses the same tensors
    # (blk.3.attn_q/k/v/output.weight) but builds the chain through the Goose builder with per-head
    # handling, and passes with vs_ggml=0. layer_real hand-rolls a second attention chain written for MHA
    # while Apex is GQA (n_head 16, n_head_kv 2 -> n_embd_k_gqa 256 vs n_embd 2048), so ggml's shape
    # assertion fires correctly. Maintaining a second, wrong implementation of attention with weaker
    # evidence than the one that passes is not worth doing.
)

if [ ! -f "$MODEL" ]; then
    echo "no model at $MODEL (set FG_APEX_GGUF); nothing to verify" >&2
    exit 2
fi
if [ ! -f "$W/main-gc.o" ]; then
    echo "build the standalone first: bash tools/build-standalone-runtime.sh" >&2
    exit 2
fi

# Harnesses that report rather than assert (no PASS string in the source).
INFORMATIONAL="apex_plan"

want=("$@")
[ ${#want[@]} -eq 0 ] && want=("${HARNESSES[@]}")

fail=0
for h in "${want[@]}"; do
    src=$ROOT/integration/goose-standalone/tests/$h.c
    [ -f "$src" ] || { echo "no such harness: $h" >&2; fail=$((fail+1)); continue; }

    # compiled as C: g++ would mangle the extern "C" host symbols and the link fails
    if ! gcc-13 -O1 -w -I "$ROOT/build/goose-main" -I "$ROOT/integration/goose-standalone" \
            -I "$ROOT/integration/m1/host" -I "$ROOT/upstream/llama.cpp/ggml/include" \
            -c "$src" -o "$OUT/$h.o" 2>"$OUT/$h.cc.log"; then
        echo "FAIL $h (compile)"; tail -3 "$OUT/$h.cc.log"; fail=$((fail+1)); continue
    fi
    if ! g++-13 -O1 -fopenmp -Wl,--gc-sections "$OUT/$h.o" "$W/main-gc.o" "$W/host.o" \
            "$W/tensor-data.o" "$W/graph-bindings.o" -L"$SUB" -lggml-base -lggml -lggml-cpu \
            -lm -pthread -o "$OUT/$h" 2>"$OUT/$h.ld.log"; then
        echo "FAIL $h (link)"; tail -3 "$OUT/$h.ld.log"; fail=$((fail+1)); continue
    fi

    # Some harnesses are informational - they print the plan and contain no PASS string at all. For those,
    # a clean exit plus their expected marker is the result; requiring PASS would report a working harness
    # as broken, which is how apex_plan was first misread here.
    case " $INFORMATIONAL " in
        *" $h "*)
            line=$(timeout 300 "$OUT/$h" "$MODEL" 2>&1 | grep -E "^FG_[A-Z0-9_]+ " | tail -1)
            if [ -n "$line" ] && ! printf '%s' "$line" | grep -qiE "refused|assert"; then
                printf 'ok   %-16s (informational) %s\n' "$h" "$line"
            else
                printf 'FAIL %-16s %s\n' "$h" "${line:-no output}"; fail=$((fail+1))
            fi
            continue ;;
    esac
    line=$(timeout 300 "$OUT/$h" "$MODEL" 2>&1 | grep -E "PASS|FAIL|assert" | tail -1)
    case "$line" in
        *PASS*) printf 'ok   %-16s %s\n' "$h" "$line" ;;
        *)      printf 'FAIL %-16s %s\n' "$h" "${line:-no PASS line}"; fail=$((fail+1)) ;;
    esac
done

echo "standalone-apex: $(( ${#want[@]} - fail ))/${#want[@]} harness(es) pass"
exit $([ "$fail" -eq 0 ] && echo 0 || echo 1)
