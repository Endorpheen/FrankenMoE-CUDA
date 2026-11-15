#!/bin/bash
# Migration accounting: what the product needs, not what the old code contains.
#
# A raw function count of the old C++ is misleading in both directions: it counts
# OS primitives that must stay native, and it implies that wiring Goose calls into
# the old runtime is progress, which it is not. The question that matters is:
#
#   does the standalone Goose runtime carry every capability the product needs?
#
# So this script classifies the own C++ by ROLE and prints:
#   1. the product capabilities that must exist (from the task ledger's P-phases)
#   2. which of them the standalone runtime already answers
#   3. the own C++ split into substrate / superseded / still-needed
#
# It never counts a call into Goose from old C++ as progress.
#
# Usage: bash tools/migration-status.sh
set -uo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

say() { printf '%s\n' "$*"; }
fn_count() { grep -cE "^[A-Za-z_][A-Za-z_0-9 :<>,&*]*\**[A-Za-z_][A-Za-z_0-9]*\(" "$1" 2>/dev/null || echo 0; }

# ---- 1. the unit -----------------------------------------------------------
modules=$(awk '/^MODULES=\(/{f=1;next} /^\)/{f=0} f && /\.goose/{gsub(/^ +/,"");print}' \
          tools/build-main-runtime.sh)
n_modules=$(printf '%s\n' "$modules" | grep -c .)

# ---- 2. own C++ by role ----------------------------------------------------
# substrate: OS and memory primitives. Must stay native - deleting them means
# re-implementing pread and mmap, which is not the migration's goal.
substrate="runtime/core/src/io/platform_io.cpp"
sub_fns=0
for f in $substrate; do sub_fns=$((sub_fns + $(fn_count "$f"))); done

# superseded: the decisions live in a Goose module that is in the unit, so the
# native body is dead once the standalone runtime carries the capability.
superseded="runtime/core/src/config.cpp
runtime/core/src/engine/session.cpp
runtime/core/src/engine/thinking_control.cpp
runtime/core/src/engine/chat_parse.cpp
runtime/core/src/engine/ngram_draft.cpp
runtime/core/src/moe/router_hook.cpp
runtime/core/src/moe/expert_stream_source.cpp
runtime/core/src/moe/row_stream.cpp
runtime/core/src/moe/gguf_offsets.cpp
runtime/core/src/moe/arch_registry.cpp
runtime/core/src/moe/deterministic_workload.cpp
runtime/core/src/metrics/decode_trace_sink.cpp
runtime/core/src/metrics/route_trace_sink.cpp
runtime/core/src/metrics/csv_metrics_sink.cpp
runtime/core/src/io/file_reader.cpp"
sup_fns=0
for f in $superseded; do sup_fns=$((sup_fns + $(fn_count "$f"))); done

# still needed: logic the standalone runtime does not carry yet.
needed="runtime/core/src/moe/dense_weights.cpp
runtime/core/src/engine/runtime.cpp"
need_fns=0
for f in $needed; do need_fns=$((need_fns + $(fn_count "$f"))); done

# ---- 3. adapter debt ------------------------------------------------------
# Every C/C++ line written to connect Goose to a substrate is debt, not progress
# (AGENTS.md). It is listed so it can be removed once the substrate gains a
# Goose-callable surface; adapters must shrink, never grow into a parallel runtime.
# Tests are not adapters: a gate holds no runtime logic and nothing links it into
# the shipped host. Only files under a tests/ directory are excluded this way.
adapters=$(ls integration/goose-standalone/*.c integration/goose-standalone/*.cpp \
              integration/m1/host/*.c 2>/dev/null | grep -v '/tests/')
adapter_lines=0
adapter_files=0
for f in $adapters; do
    adapter_lines=$((adapter_lines + $(wc -l < "$f")))
    adapter_files=$((adapter_files + 1))
done

# ---- report ----------------------------------------------------------------
say "MIGRATION ACCOUNTING (derived from the repository; no build)"
say ""
say "1. Composed unit:            $n_modules modules"
say ""
say "2. Own C++ (runtime/core), by role - NOT a rewrite list:"
say "   substrate (stays native):  $(printf '%s ' $substrate | tr '\n' ' ')$sub_fns fns"
say "   superseded by Goose:       $sup_fns fns in $(printf '%s\n' $superseded | grep -c .) files"
say "   still needed:              $need_fns fns in $(printf '%s\n' $needed | grep -c .) files"
say ""
say "3. Adapter debt (to be REMOVED, never counted as migration):"
say "   files: $adapter_files   lines: $adapter_lines"
for f in $adapters; do printf '     %5s  %s\n' "$(wc -l < "$f")" "$f"; done
say ""
say "4. Standalone runtime capabilities (evidence: EXP671-673, tools/build-standalone-runtime.sh):"
say "   GGUF container read          yes  (substrate reader)"
say "   GGUF metadata, every key     yes  (4 files, agree=yes)"
say "   GGUF tensor directory        yes  (fixtures + 3 real models, 940 tensors)"
say "   tensor payload in memory     yes  (EXP680 fixtures, EXP682 real models)"
say "   type table / block sizes     yes  (abi_type_count, abi_blck_size)"
say "   config validator             yes  (cfg_validate, 48 rules)"
say "   tool + MIME tables           yes  (tool_index, mime_of/mime_text)"
say "   graph builder + execution    yes  (EXP681: Goose builds the node, ggml runs it)"
say "   Apex arch plan (qwen35moe)    yes  (EXP684: 42 layers, 10 attn / 31 recr / 1 MTP)"
say "   allocator                    arithmetic + verified placement (nbytes/layout)"
say "   backend seam (CPU)           yes  (EXP681: public ggml-backend API)"
say "   backend (GPU)                no   (CUDA is substrate + needs a run approval)"
say "   KV cache buffers             no"
say "   tokenizer mechanics          no  (decisions only)"
say "   HTTP server                  no  (decisions only)"
say "   model load + graph build     no"
say ""
say "Progress toward rule 15 is the third block: a capability is ready when the"
say "standalone runtime answers it without the old C++, not when the old C++ calls"
say "Goose. Substrate lines are never counted as debt."
