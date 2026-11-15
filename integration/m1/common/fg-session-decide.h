// Staged S3.2 carrier: the P9-05 session decisions behind one build flag.
//
// The three accepted slices (EXP-632) are pure integer functions:
//   goose/misc/session-lifecycle.goose  ss_   hot store sizing, VRAM preflight, post-load probe
//   goose/misc/runtime-facade.goose     rts_  n_batch, n_ubatch, run()'s five calls and early returns
//   goose/misc/draft-source.goose       nd_   the draft context width and the verify batch width
//
// This header is the shape the staged build uses instead of session.cpp and runtime.cpp:
// the same decisions, composed into one open-time plan, over synthetic state. The native
// bodies live in fg-session-decide.cpp under `native::` (always compiled, always reachable);
// with FG_GOOSE_SESSION=1 the plain names dispatch to the Goose exports through
// fg-goose-session-bridge.cpp.
//
// expert_bytes_per_slot is an INPUT, exactly as EXP-632 records: the native code derives it
// with a std::regex sum over the GGUF tensor names, that regex is not ported and stays in
// the carrier. Everything else this file needs is an integer.
#pragma once

#include <cstdint>

namespace fg_session {

// ── the switched decisions, one function each ───────────────────────────────
// Every plain name below has a twin in `native`. The flag selects which body the plain
// name ends up calling; nothing is deleted when the flag is on.

// ss_ — the hot store. session.cpp:49-81, :73, :74-79, :669-685, :856-902.
int64_t sentinels(int64_t top_k);
int64_t hot_slots(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                  int64_t top_k);
int64_t hot_reason(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                   int64_t top_k);
int64_t sentinel_mib(int64_t bytes_per_slot, int64_t top_k);
int64_t vram_clamp(int64_t free_mib, int64_t budget_mb, int64_t n_gpu_layers);
int64_t probe_margin(int64_t is_mtp);
int64_t probe_budget(int64_t free_mib, int64_t is_mtp);

struct ResizePlan {
    int64_t action; // 0 keep, 1 resize, 2 disable (SS_KEEP / SS_RESIZE / SS_DISABLE)
    int64_t budget; // effective budget MiB after the probe
    int64_t slots;  // slots per layer after the probe
};
ResizePlan resize_plan(int64_t plan_slots, int64_t plan_mib, int64_t n_gpu_layers,
                       int64_t free_mib, int64_t is_mtp, int64_t meta_ok, int64_t n_expert,
                       int64_t bytes_per_slot, int64_t top_k);

// rts_ — the context widths and run()'s order. runtime.cpp:9-26, :32-68; session.cpp:817.
int64_t n_batch(int64_t n_ctx);
int64_t ubatch(int64_t n_ubatch, int64_t n_batch);

// The five calls run() makes and the two places it returns early. out receives the executed
// step codes (1 validate, 2 map, 3 open, 4 request, 5 generate); the return value is their
// count, so a short out[] truncates the trace instead of overflowing it.
int64_t run_plan(int64_t validate_ok, int64_t open_ok, int64_t * out, int64_t cap);

// nd_ — the widths the session builds from an already validated SpecConfig.
// session.cpp:909-931, :978-986.
int64_t draft_ubatch(int64_t draft_max, int64_t target_ubatch);
int64_t verify_batch(int64_t n_batch, int64_t draft_max);

// ── the native transcription ────────────────────────────────────────────────
// The oracle of the gate, compiled in both configurations. Each body quotes the lines it
// reproduces. It is the OFF path and the thing the ON path is compared against, in one
// process, so a transcription mistake shows up as a divergence rather than as agreement.

namespace native {

int64_t sentinels(int64_t top_k);
int64_t hot_slots(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                  int64_t top_k);
int64_t hot_reason(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                   int64_t top_k);
int64_t sentinel_mib(int64_t bytes_per_slot, int64_t top_k);
int64_t vram_clamp(int64_t free_mib, int64_t budget_mb, int64_t n_gpu_layers);
int64_t probe_margin(int64_t is_mtp);
int64_t probe_budget(int64_t free_mib, int64_t is_mtp);
ResizePlan resize_plan(int64_t plan_slots, int64_t plan_mib, int64_t n_gpu_layers,
                       int64_t free_mib, int64_t is_mtp, int64_t meta_ok, int64_t n_expert,
                       int64_t bytes_per_slot, int64_t top_k);
int64_t n_batch(int64_t n_ctx);
int64_t ubatch(int64_t n_ubatch, int64_t n_batch);
int64_t run_plan(int64_t validate_ok, int64_t open_ok, int64_t * out, int64_t cap);
int64_t draft_ubatch(int64_t draft_max, int64_t target_ubatch);
int64_t verify_batch(int64_t n_batch, int64_t draft_max);

} // namespace native

// ── the composed open-time plan ─────────────────────────────────────────────
// The state a session would hold in its Impl at the three native sites this stage covers,
// flattened to integers: the gguf shape, the requested budget, and the two free-VRAM
// readings (before the load, and after the throwaway probe context is freed).
struct OpenState {
    const char * name;

    // gguf side (bytes_per_slot is the input EXP-632 records as not ported)
    int64_t meta_ok;
    int64_t n_expert;
    int64_t top_k;
    int64_t bytes_per_slot;

    // launcher / config side
    int64_t budget_mb;
    int64_t n_gpu_layers;
    int64_t n_ctx;
    int64_t n_ubatch;
    int64_t draft_max;
    int64_t src; // the spec source code nd_ takes: 0 off, 1 mtp, 2 ngram

    // device side
    int64_t free_before_mib;
    int64_t free_after_mib;

    // run() admission, for the call-order trace
    int64_t validate_ok;
    int64_t open_ok;
};

struct OpenPlan {
    int ok;                  // 0: the VRAM preflight or the hot store refused the open
    int64_t effective_vram_mb;
    int64_t hot_slots;       // -1 when the native code would have refused the open
    int64_t hot_reason;      // which refusal, for the caller's message (0 = none)
    int64_t sentinel_mib;    // the MiB figure the too-small message quotes
    int64_t probe_action;
    int64_t probe_mib;
    int64_t probe_slots;
    int64_t n_batch;
    int64_t n_ubatch;
    int64_t draft_ubatch;
    int64_t verify_batch;
    int64_t steps;           // run() trace length
    int64_t step[5];         // run() trace codes
};

// The same sequence, twice: plan_open dispatches every decision through the flag,
// plan_open_native takes the transcription. The gate compares them in one process; with the
// flag off they are the same function by construction.
OpenPlan plan_open(const OpenState & st);
OpenPlan plan_open_native(const OpenState & st);

// Decisions taken by the Goose module in this process (0 in an OFF build, because the bridge
// is not compiled in). The gate prints it as the witness that dispatch really entered Goose.
long long goose_calls();

} // namespace fg_session
