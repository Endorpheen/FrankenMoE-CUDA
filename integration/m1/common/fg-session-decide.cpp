// Staged S3.2 carrier: the native bodies of the P9-05 session decisions, their dispatch, and
// the composed open-time plan.
//
// Each body under `native::` is a transcription of the exact lines quoted above it; the gate
// compares it against the Goose export in the same process. With FG_GOOSE_SESSION=0 the plain
// name is a direct call to the transcription, so the OFF build is the native code path — the
// same convention S1.1 and S2.2 used, only the seam differs.
#include "fg-session-decide.h"

#ifdef FG_GOOSE_SESSION
#include "fg-goose-session-bridge.h"
#endif

namespace fg_session {

namespace {

constexpr int64_t kMib = 1048576;
constexpr int64_t kReserveMib = 4096;
constexpr int64_t kMinSentinels = 8;
constexpr int64_t kProbeMarginBase = 256;
constexpr int64_t kProbeMarginMtp = 512;
constexpr int64_t kMtpDraftUbatch = 32;

// The five step codes of rts_plan, in the order run() executes them.
constexpr int64_t kValidate = 1;
constexpr int64_t kMap = 2;
constexpr int64_t kOpen = 3;
constexpr int64_t kRequest = 4;
constexpr int64_t kGenerate = 5;

} // namespace

// ── the transcription ───────────────────────────────────────────────────────

namespace native {

// session.cpp:73 — const int sentinels = std::max(8, top_k);
int64_t sentinels(int64_t top_k) { return top_k > kMinSentinels ? top_k : kMinSentinels; }

// session.cpp:49-81 — hot_slots_from_budget. -1 mirrors its failure return, 0 means "budget
// off", >= 1 is the slot count. bytes_per_slot arrives already summed: the regex over the
// tensor names stays in the carrier (see the header).
int64_t hot_slots(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                  int64_t top_k) {
    if (budget_mb <= 0) return 0;
    if (!meta_ok || n_expert <= 0) return -1;
    if (bytes_per_slot == 0) return -1;
    const int64_t sent = sentinels(top_k);
    const uint64_t budget = (uint64_t) budget_mb * 1024ull * 1024ull;
    const uint64_t total = budget / (uint64_t) bytes_per_slot;
    if (total <= (uint64_t) sent) return -1;
    const int64_t slots = (int64_t) total - sent;
    return slots < n_expert ? slots : n_expert;
}

// session.cpp:50-80 — which error hot_slots_from_budget sets: 0 none, 1 no metadata,
// 2 no expert tensors, 3 sentinels alone do not fit.
int64_t hot_reason(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                   int64_t top_k) {
    if (budget_mb <= 0) return 0;
    if (!meta_ok || n_expert <= 0) return 1;
    if (bytes_per_slot == 0) return 2;
    const int64_t sent = sentinels(top_k);
    const uint64_t budget = (uint64_t) budget_mb * 1024ull * 1024ull;
    const uint64_t total = budget / (uint64_t) bytes_per_slot;
    if (total <= (uint64_t) sent) return 3;
    return 0;
}

// session.cpp:74-79 — (bytes_per_slot * sentinels + MiB - 1) / MiB.
int64_t sentinel_mib(int64_t bytes_per_slot, int64_t top_k) {
    if (bytes_per_slot <= 0) return 0;
    const uint64_t bytes = (uint64_t) bytes_per_slot * (uint64_t) sentinels(top_k);
    return (int64_t) ((bytes + (uint64_t) kMib - 1) / (uint64_t) kMib);
}

// session.cpp:669-685 — the pre-load clamp. -1 is the preflight refusal.
int64_t vram_clamp(int64_t free_mib, int64_t budget_mb, int64_t n_gpu_layers) {
    if (!(budget_mb > 0 && n_gpu_layers != 0)) return budget_mb;
    if ((uint64_t) free_mib <= (uint64_t) kReserveMib) return -1;
    const int64_t safe_mib = free_mib - kReserveMib;
    return budget_mb > safe_mib ? safe_mib : budget_mb;
}

// session.cpp:868 — 256 + (is_mtp ? 512 : 0).
int64_t probe_margin(int64_t is_mtp) { return kProbeMarginBase + (is_mtp ? kProbeMarginMtp : 0); }

// session.cpp:869 — the budget the post-load free-VRAM reading implies.
int64_t probe_budget(int64_t free_mib, int64_t is_mtp) {
    const int64_t margin = probe_margin(is_mtp);
    return (uint64_t) free_mib > (uint64_t) margin ? free_mib - margin : 0;
}

// session.cpp:856-902 — the action and its two outputs.
ResizePlan resize_plan(int64_t plan_slots, int64_t plan_mib, int64_t n_gpu_layers,
                       int64_t free_mib, int64_t is_mtp, int64_t meta_ok, int64_t n_expert,
                       int64_t bytes_per_slot, int64_t top_k) {
    ResizePlan p{0, plan_mib, plan_slots};
    if (!(plan_slots > 0 && n_gpu_layers != 0 && plan_mib > 0)) return p;
    const int64_t pb = probe_budget(free_mib, is_mtp);
    if (pb < plan_mib) {
        const int64_t slots = pb > 0 ? hot_slots(pb, meta_ok, n_expert, bytes_per_slot, top_k) : -1;
        if (slots > 0) {
            p.action = 1;
            p.budget = pb;
            p.slots = slots;
        } else {
            p.action = 2;
            p.slots = 0;
        }
    }
    return p;
}

// runtime.cpp:14 — sc.n_batch = cfg.n_ctx.
int64_t n_batch(int64_t n_ctx) { return n_ctx; }

// session.cpp:817 — cparams.n_ubatch = cfg.n_ubatch > 0 ? cfg.n_ubatch : cfg.n_batch.
int64_t ubatch(int64_t n_ubatch, int64_t n_batch_v) { return n_ubatch > 0 ? n_ubatch : n_batch_v; }

// runtime.cpp:38-53 — the executed steps, with the two early returns. A cap shorter than the
// plan truncates the trace; run() itself has no cap.
int64_t run_plan(int64_t validate_ok, int64_t open_ok, int64_t * out, int64_t cap) {
    int64_t n = 0;
    if (cap > 0) out[0] = kValidate;
    n = 1;
    if (!validate_ok || n >= cap) return n;
    out[1] = kMap;
    n = 2;
    if (n >= cap) return n;
    out[2] = kOpen;
    n = 3;
    if (!open_ok || n >= cap) return n;
    out[3] = kRequest;
    n = 4;
    if (n >= cap) return n;
    out[4] = kGenerate;
    return 5;
}

// session.cpp:926-927 — the MTP draft context's widest graph, clamped to the target width.
int64_t draft_ubatch(int64_t draft_max, int64_t target_ubatch) {
    int64_t w = draft_max + 1;
    if (w < kMtpDraftUbatch) w = kMtpDraftUbatch;
    return w > target_ubatch ? target_ubatch : w;
}

// session.cpp:983 — one verify batch, wide enough for a prefill chunk or 1 + draft_max.
int64_t verify_batch(int64_t n_batch_v, int64_t draft_max) {
    const int64_t v = draft_max + 1;
    return n_batch_v > v ? n_batch_v : v;
}

} // namespace native

// ── the switched entry points ───────────────────────────────────────────────

int64_t sentinels(int64_t top_k) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::sentinels(top_k);
#else
    return native::sentinels(top_k);
#endif
}

int64_t hot_slots(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                  int64_t top_k) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::hot_slots(budget_mb, meta_ok, n_expert, bytes_per_slot, top_k);
#else
    return native::hot_slots(budget_mb, meta_ok, n_expert, bytes_per_slot, top_k);
#endif
}

int64_t hot_reason(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                   int64_t top_k) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::hot_reason(budget_mb, meta_ok, n_expert, bytes_per_slot, top_k);
#else
    return native::hot_reason(budget_mb, meta_ok, n_expert, bytes_per_slot, top_k);
#endif
}

int64_t sentinel_mib(int64_t bytes_per_slot, int64_t top_k) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::sentinel_mib(bytes_per_slot, top_k);
#else
    return native::sentinel_mib(bytes_per_slot, top_k);
#endif
}

int64_t vram_clamp(int64_t free_mib, int64_t budget_mb, int64_t n_gpu_layers) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::vram_clamp(free_mib, budget_mb, n_gpu_layers);
#else
    return native::vram_clamp(free_mib, budget_mb, n_gpu_layers);
#endif
}

int64_t probe_margin(int64_t is_mtp) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::probe_margin(is_mtp);
#else
    return native::probe_margin(is_mtp);
#endif
}

int64_t probe_budget(int64_t free_mib, int64_t is_mtp) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::probe_budget(free_mib, is_mtp);
#else
    return native::probe_budget(free_mib, is_mtp);
#endif
}

ResizePlan resize_plan(int64_t plan_slots, int64_t plan_mib, int64_t n_gpu_layers,
                       int64_t free_mib, int64_t is_mtp, int64_t meta_ok, int64_t n_expert,
                       int64_t bytes_per_slot, int64_t top_k) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::resize_plan(plan_slots, plan_mib, n_gpu_layers, free_mib, is_mtp,
                                         meta_ok, n_expert, bytes_per_slot, top_k);
#else
    return native::resize_plan(plan_slots, plan_mib, n_gpu_layers, free_mib, is_mtp, meta_ok,
                               n_expert, bytes_per_slot, top_k);
#endif
}

int64_t n_batch(int64_t n_ctx) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::n_batch(n_ctx);
#else
    return native::n_batch(n_ctx);
#endif
}

int64_t ubatch(int64_t n_ubatch_v, int64_t n_batch_v) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::ubatch(n_ubatch_v, n_batch_v);
#else
    return native::ubatch(n_ubatch_v, n_batch_v);
#endif
}

int64_t run_plan(int64_t validate_ok, int64_t open_ok, int64_t * out, int64_t cap) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::run_plan(validate_ok, open_ok, out, cap);
#else
    return native::run_plan(validate_ok, open_ok, out, cap);
#endif
}

int64_t draft_ubatch(int64_t draft_max, int64_t target_ubatch) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::draft_ubatch(draft_max, target_ubatch);
#else
    return native::draft_ubatch(draft_max, target_ubatch);
#endif
}

int64_t verify_batch(int64_t n_batch_v, int64_t draft_max) {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::verify_batch(n_batch_v, draft_max);
#else
    return native::verify_batch(n_batch_v, draft_max);
#endif
}

// ── the composed open-time plan ─────────────────────────────────────────────
// The order is the native one and matters in two places: the preflight clamp runs before the
// hot store is sized, and the post-load probe runs after it. The probe sees the state the
// hot store produced, not the requested budget.

namespace {

OpenPlan plan_open_impl(const OpenState & st, bool nat) {
    OpenPlan p{};
    p.hot_slots = -1;

    const int64_t clamped = nat
                                ? native::vram_clamp(st.free_before_mib, st.budget_mb, st.n_gpu_layers)
                                : vram_clamp(st.free_before_mib, st.budget_mb, st.n_gpu_layers);
    p.effective_vram_mb = clamped;
    if (clamped < 0) return p; // the preflight refuses: no session, no plan

    if (st.budget_mb > 0) {
        p.hot_slots = nat ? native::hot_slots(clamped, st.meta_ok, st.n_expert, st.bytes_per_slot,
                                              st.top_k)
                          : hot_slots(clamped, st.meta_ok, st.n_expert, st.bytes_per_slot,
                                      st.top_k);
    } else {
        p.hot_slots = 0;
    }
    p.hot_reason = nat ? native::hot_reason(st.budget_mb, st.meta_ok, st.n_expert,
                                            st.bytes_per_slot, st.top_k)
                       : hot_reason(st.budget_mb, st.meta_ok, st.n_expert, st.bytes_per_slot,
                                    st.top_k);
    p.sentinel_mib = nat ? native::sentinel_mib(st.bytes_per_slot, st.top_k)
                         : sentinel_mib(st.bytes_per_slot, st.top_k);
    if (p.hot_slots < 0) return p; // the hot store refuses: no session

    const ResizePlan rp =
        nat ? native::resize_plan(p.hot_slots, clamped, st.n_gpu_layers, st.free_after_mib, st.src,
                                  st.meta_ok, st.n_expert, st.bytes_per_slot, st.top_k)
            : resize_plan(p.hot_slots, clamped, st.n_gpu_layers, st.free_after_mib, st.src,
                          st.meta_ok, st.n_expert, st.bytes_per_slot, st.top_k);
    p.probe_action = rp.action;
    p.probe_mib = rp.budget;
    p.probe_slots = rp.slots;
    if (rp.action == 1) {
        p.hot_slots = rp.slots;
        p.effective_vram_mb = rp.budget;
    } else if (rp.action == 2) {
        p.hot_slots = 0;
    }

    p.n_batch = nat ? native::n_batch(st.n_ctx) : n_batch(st.n_ctx);
    p.n_ubatch = nat ? native::ubatch(st.n_ubatch, p.n_batch) : ubatch(st.n_ubatch, p.n_batch);
    // Both widths belong to the speculative half: the draft context exists only for MTP and
    // the verify batch only when any source is on (session.cpp:909, :978).
    p.draft_ubatch = st.src == 1 ? (nat ? native::draft_ubatch(st.draft_max, p.n_ubatch)
                                        : draft_ubatch(st.draft_max, p.n_ubatch))
                                 : 0;
    p.verify_batch = st.src != 0 ? (nat ? native::verify_batch(p.n_batch, st.draft_max)
                                        : verify_batch(p.n_batch, st.draft_max))
                                 : 0;

    p.steps = nat ? native::run_plan(st.validate_ok, st.open_ok, p.step, 5)
                  : run_plan(st.validate_ok, st.open_ok, p.step, 5);
    p.ok = 1;
    return p;
}

} // namespace

OpenPlan plan_open(const OpenState & st) { return plan_open_impl(st, false); }
OpenPlan plan_open_native(const OpenState & st) { return plan_open_impl(st, true); }

long long goose_calls() {
#ifdef FG_GOOSE_SESSION
    return fg_goose_session::calls();
#else
    return 0;
#endif
}

} // namespace fg_session
