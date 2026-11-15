// Goose bridge for the P9-05 session decisions (staged S3.2 trial).
// See fg-goose-session-bridge.h for the contract; the module is the composition of
// goose/misc/{session-lifecycle,runtime-facade,draft-source}.goose into goose-gen/session.goose,
// generated to goose-gen/session.c. Composing all three into one unit keeps one goose_init and
// one object, so no objcopy rename is needed and nm shows exactly one definition per export.
#include "fg-goose-session-bridge.h"
#include "goose-host.h" // fg_goose_host_ensure_init

#include "session.h"

#include <mutex>

namespace fg_goose_session {

namespace {

long long g_calls = 0;

void init() {
    // M1: one start-up for the whole runtime. The per-stage guard of the
    // accepted card would be a tenth pthread_once in this link.
    fg_goose_host_ensure_init();
}

// The module only reads this slice, and every export guards out.len before writing.
sl_i64 sl64(int64_t * p, int64_t n) { return sl_i64{p, n}; }

} // namespace

long long calls() { return g_calls; }

int64_t sentinels(int64_t top_k) {
    init();
    ++g_calls;
    return ss_sentinels(top_k);
}

int64_t hot_slots(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                  int64_t top_k) {
    init();
    ++g_calls;
    return ss_hot_slots(budget_mb, meta_ok, n_expert, bytes_per_slot, top_k);
}

int64_t hot_reason(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                   int64_t top_k) {
    init();
    ++g_calls;
    return ss_hot_reason(budget_mb, meta_ok, n_expert, bytes_per_slot, top_k);
}

int64_t sentinel_mib(int64_t bytes_per_slot, int64_t top_k) {
    init();
    ++g_calls;
    return ss_sentinel_mib(bytes_per_slot, top_k);
}

int64_t vram_clamp(int64_t free_mib, int64_t budget_mb, int64_t n_gpu_layers) {
    init();
    ++g_calls;
    return ss_vram_clamp(free_mib, budget_mb, n_gpu_layers);
}

int64_t probe_margin(int64_t is_mtp) {
    init();
    ++g_calls;
    return ss_probe_margin(is_mtp);
}

int64_t probe_budget(int64_t free_mib, int64_t is_mtp) {
    init();
    ++g_calls;
    return ss_probe_budget(free_mib, is_mtp);
}

fg_session::ResizePlan resize_plan(int64_t plan_slots, int64_t plan_mib, int64_t n_gpu_layers,
                                   int64_t free_mib, int64_t is_mtp, int64_t meta_ok,
                                   int64_t n_expert, int64_t bytes_per_slot, int64_t top_k) {
    init();
    ++g_calls;
    int64_t out[3] = {-1, -1, -1};
    const int64_t action =
        ss_resize_plan(plan_slots, plan_mib, n_gpu_layers, free_mib, is_mtp, meta_ok, n_expert,
                       bytes_per_slot, top_k, sl64(out, 3));
    fg_session::ResizePlan p{action, plan_mib, plan_slots};
    if (out[0] >= 0) p.action = out[0];
    if (out[1] >= 0) p.budget = out[1];
    if (out[2] >= 0) p.slots = out[2];
    return p;
}

int64_t n_batch(int64_t n_ctx) {
    init();
    ++g_calls;
    return rts_n_batch(n_ctx);
}

int64_t ubatch(int64_t n_ubatch_v, int64_t n_batch_v) {
    init();
    ++g_calls;
    return rts_ubatch(n_ubatch_v, n_batch_v);
}

int64_t run_plan(int64_t validate_ok, int64_t open_ok, int64_t * out, int64_t cap) {
    init();
    ++g_calls;
    return rts_plan(validate_ok, open_ok, sl64(out, cap));
}

int64_t draft_ubatch(int64_t draft_max, int64_t target_ubatch) {
    init();
    ++g_calls;
    return nd_draft_ubatch(draft_max, target_ubatch);
}

int64_t verify_batch(int64_t n_batch_v, int64_t draft_max) {
    init();
    ++g_calls;
    return nd_verify_batch(n_batch_v, draft_max);
}

} // namespace fg_goose_session
