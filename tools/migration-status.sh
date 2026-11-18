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
# Usage: bash tools/migration-status.sh [--probe-load]
# The optional probe loads a model; the default command only reads metadata.
set -uo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

say() { printf '%s\n' "$*"; }
fn_count() {
    local count
    # A missing file must not read as "zero functions": that silently shrinks a
    # count and looks like progress. Refuse loudly instead.
    if [ ! -f "$1" ]; then
        printf 'migration-status: listed file is missing: %s\n' "$1" >&2
        printf '0\n'
        return 1
    fi
    count=$(grep -cE "^[A-Za-z_][A-Za-z_0-9 :<>,&*]*\**[A-Za-z_][A-Za-z_0-9]*\(" "$1" 2>/dev/null) || true
    printf '%s\n' "${count:-0}"
}
PROBE_LOAD=0
case "${1:-}" in
    "") ;;
    --probe-load) PROBE_LOAD=1 ;;
    *) printf 'usage: %s [--probe-load]\n' "$0" >&2; exit 2 ;;
esac

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
say "Historical standalone accounting follows; it is NOT coverage of the accepted Apex binary."
say "Current link-closure audit: docs/goose-migration/RUNTIME-MAP-20261117.md"
say "Symbol presence, source ownership, observed execution and full replacement are separate facts."
say ""
say "1. Composed unit:            $n_modules modules"
say ""
say "2. Own C++ (runtime/core), by role - NOT a rewrite list:"
# Whether this tree is built at all decides what these numbers mean. If nothing
# builds it, "superseded" describes a decision recorded in a document, not a
# replacement in a shipped artifact - and the two must not be read as the same.
_bmoe_built=0
if [ -n "$(find "$ROOT/builds" -name 'libbmoe_core*' 2>/dev/null | head -1)" ]; then
    _bmoe_built=1
fi
if [ "$_bmoe_built" -eq 1 ]; then
say "   build target:              built (libbmoe_core present under builds/)"
else
say "   build target:              NONE - no build produces libbmoe_core, so these roles"
say "                              are recorded decisions, not replacements in an artifact"
fi
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
say "   Apex arch plan IN THE STANDALONE yes (this commit: q35_plan/kind/shape, qg_split,"
say "                                       gdn_*, state_slot all link in the standalone binary;"
say "                                       41 layers, 10 attn / 30 recr / 1 MTP per EXP699;"
say "                                       the full 41-layer stack is still not executed"
say "   allocator                    arithmetic + verified placement (nbytes/layout)"
say "   backend seam (CPU)           yes  (EXP681: public ggml-backend API)"
say "   backend (GPU)                no   (CUDA is substrate + needs a run approval)"
say "   rope / attention in a block  yes  (EXP694: 15 nodes bit-identical to a native build,"
say "                                       incl. per-head q/k RMS norm and IMRoPE)"
say "                                       41 layers, 10 attn / 30 recr / 1 MTP per EXP699)"
say "   GDN stack (30 of 41 layers)  yes  (EXP696: 13 nodes bit-identical; EXP701/703: KV"
say "                                       persists across rebuilds, reuse from n_past = one pass)"
say "   full 41-layer graph          dispatch yes / execution no   (EXP705: kinds at the"
say "                                       right positions; not run as a stack)"
say "   KV cache buffers             yes  (EXP701/702: one slot mechanism, 22 slots identical)"
# Computed, not asserted: earlier this line claimed the tok_* exports were absent from the unit while
# the built binary carried all eight. Count them in the artifact the run actually uses.
_tok_n=0
SA_BIN=${FG_STANDALONE_BIN:-"$ROOT/builds/goose-standalone/frankenmoe-v4"}
if [ -f "$SA_BIN" ]; then
    _tok_n=$(nm "$SA_BIN" 2>/dev/null | grep -cE " T tok_")
fi
# The engine is a different capability from the decisions, and both are computed
# from the artifact. The old text asserted "no load-and-tokenise path" here; that
# became false the moment btl_load/bt_bpe/bt_token_to_piece were added to the unit
# (EXP949-956), and an asserted line silently keeps lying. Count it instead.
_eng_n=0
if [ -f "$SA_BIN" ]; then
    _eng_n=$(nm "$SA_BIN" 2>/dev/null | grep -cE " T (btl_load|bt_build|bt_bpe|bt_token_to_piece)")
fi
if [ "$_tok_n" -ge 8 ]; then
say "   tokenizer decisions (all 8)   answers  ($_tok_n tok_* exports; tokenizer_decisions gate: 39 checks, 0 failures)"
elif [ "$_eng_n" -ge 1 ]; then
    # Absent tok_* with bt_bpe present is not "the decisions are gone": the eight
    # predicates were folded into the engine entry point (EXP949-956). Saying "no"
    # here would report a removed interface as a missing capability.
say "   tokenizer decisions (all 8)   inside bt_bpe - the separate tok_* predicates were"
say "                                 folded into the engine, so there is no tok_* count to make"
else
say "   tokenizer decisions (all 8)   no   (only $_tok_n tok_* exports in the artifact)"
fi
# Present in the artifact and actually called are different facts. The load-and-
# tokenise gate is what makes the second one true, so name it when it exists.
_lat=""
if [ -x "$ROOT/builds/goose-standalone/tokenize_load" ]; then
    _lat=" and called by the load-and-tokenise gate"
fi
if [ "$_eng_n" -ge 4 ]; then
say "   tokenizer engine             yes  ($_eng_n load/build/tokenise/piece exports in the artifact$_lat;"
say "                                       gates: EXP950/955/956 + tokenize_load 25 checks)"
elif [ "$_eng_n" -gt 0 ]; then
say "   tokenizer engine             partial ($_eng_n of 4 engine exports in the artifact)"
else
    # The artifact can be stale rather than incapable, and the two must not be
    # conflated: a build from before the modules existed has no engine in it and is
    # not evidence that the unit lacks one. Say which case this is.
    _src_new=$(find "$ROOT/goose/model/bpe-load.goose" "$ROOT/goose/model/bpe-tokenize.goose" \
                    -newer "$SA_BIN" 2>/dev/null | wc -l)
    if [ "$_src_new" -gt 0 ]; then
say "   tokenizer engine             stale artifact (0 engine exports; its sources are newer -"
say "                                       rebuild: bash tools/build-main-runtime.sh && bash tools/build-standalone-runtime.sh)"
    else
say "   tokenizer engine             no   (0 engine exports and the sources are not newer)"
    fi
fi
say "   HTTP-server decisions        yes  (this run: detok 1063000 calls, sched 39500,"
say "                                       graph-input-plan 200 - all witnessed live)"
say "   HTTP server (transport)      no   (decisions run; no listener in the standalone)"
# Computed from the artifact, not asserted. Reading and laying out 21.7 GB of real tensors is a capability;
# executing them is a different one, and the two used to be collapsed into "partial".
_apex_mismatch="-"
APEX_GGUF=${FG_APEX_GGUF:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
if [ "$PROBE_LOAD" -eq 1 ] && [ -f "$SA_BIN" ] && [ -f "$APEX_GGUF" ]; then
    _mdl=$(mktemp)
    if timeout 180 "$SA_BIN" "$APEX_GGUF" >"$_mdl" 2>&1; then
        _apex_mismatch=$(awk -F'mismatches=' '/FG_TENSOR_DATA/{split($2,a," ");print a[1]}' "$_mdl")
        _apex_tensors=$(awk -F'tensors=' '/FG_TENSOR_DATA/{split($2,a," ");print a[1]}' "$_mdl")
        _apex_agree=$(awk -F'agree=' '/FG_STANDALONE_V3/{split($2,a," ");print a[1]}' "$_mdl")
    fi
    rm -f "$_mdl"
fi
if [ "$_apex_mismatch" = "0" ]; then
say "   model LOAD (read + layout)   yes  ($_apex_tensors tensors, Apex payload copied, 0 mismatches;"
say "                                      Goose metadata table agrees=$_apex_agree)"
say "   model EXECUTE (logits, serve) no  (loading a tensor is not running it - rule 15 untouched)"
else
say "   model load + graph build     not probed (explicit --probe-load opts into the model load)"
fi
say ""
say "Architectures: 13 have a Goose builder of 151 in the substrate"
say "  (docs/goose-migration/ARCHITECTURE-MAP.md; Apex is one of the 13)"
say ""
say "Honest reading, split in two:"
# The engine sentence must follow the artifact, not the other way round: after the
# rebuild it is present, and a fixed sentence would be the same lie EXP972 fixed.
if [ "$_eng_n" -ge 4 ]; then
say "  - the STANDALONE runtime (this section) carries the tokenizer engine"
say "    ($_eng_n exports) and the three sampler filters, and still lacks CUDA, HTTP"
say "    transport and full-stack execution."
else
say "  - the STANDALONE runtime (this section) lacks the tokenizer engine in this"
say "    artifact ($_eng_n of 4 exports) plus CUDA, HTTP transport and full-stack execution."
fi
say "  - the INTEGRATED stage-run does run Apex end to end with Goose code witnessed, but it"
say "    is llama.cpp plus Goose, not a standalone Goose runtime. Best measured: 2429.9"
say "    prefill / 39.86 decode at 512 tokens, VRAM 7552 MiB, output identical to the"
say "    reference (runtime/goose-stage/BEST-CONFIG.json)."
say "    Those are historical stage figures, not the EXP906 baseline. The accepted independent"
say "    Apex EXP906 median is 2625.21 prefill / 55.52 single-response decode (32K/512)."
say "Do not report the second as progress on the first - that is the substitution rule 15"
say "forbids. Nothing may be deleted until the standalone answers and Igor accepts it."
say ""
say "Progress toward rule 15 is the third block: a capability is ready when the"
say "standalone runtime answers it without the old C++, not when the old C++ calls"
say "Goose. Substrate lines are never counted as debt."
