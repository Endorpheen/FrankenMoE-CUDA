// P9-05 S1: the standalone session's own lifecycle.
//
// Native source: runtime/core/src/engine/session.cpp
//   hot_slots_from_budget     session.cpp:49-81   (VRAM hot store sized from the gguf shape)
//   the VRAM preflight clamp  session.cpp:669-700 (budget vs free VRAM before the load)
//   the post-context probe    session.cpp:856-902 (budget re-derived after KV+compute reserves)
//   the Impl teardown order   session.cpp:415-431 (~Impl, the one explicit order)
//
// What is NOT here, on purpose: model loading, gguf parsing, the regex over tensor
// names, the accelerator query and llama_backend_init/free. Those are the
// carrier. This module owns the arithmetic and the ordering that surround
// them, so they can be gated without a model, a device or a file.
//
// Every export names the native lines it reproduces. Numbers, not texts: the
// error strings stay the carrier's; ss_hot_reason returns which one it would be.

import std;

// ss_hot_reason results — which hot_slots_from_budget error, if any.
const SS_OK: i64 = 0;          // 0 slots (budget off) or a valid slot count
const SS_ERR_NO_META: i64 = 1; // "GGUF has no valid expert metadata"      (54-58)
const SS_ERR_NO_TENSORS: i64 = 2; // "no expert weight tensors found"     (66-69)
const SS_ERR_TOO_SMALL: i64 = 3;  // "sentinel slots alone require ..."   (76-80)

const SS_MIB: i64 = 1048576;
const SS_VRAM_RESERVE_MIB: i64 = 4096; // session.cpp:671
const SS_MIN_SENTINELS: i64 = 8;       // session.cpp:73 std::max(8, top_k)
const SS_PROBE_MARGIN_BASE: i64 = 256; // session.cpp:868
const SS_PROBE_MARGIN_MTP: i64 = 512;  // session.cpp:868, the MTP draft context's reserve

// ss_resize_plan actions.
const SS_KEEP: i64 = 0;    // the open-time plan survives the probe
const SS_RESIZE: i64 = 1;  // fewer slots, derived from what the probe left free
const SS_DISABLE: i64 = 2; // too little left even for the sentinels: experts run cold

// ── hot_slots_from_budget ───────────────────────────────────────────────────

// session.cpp:73 — one zero sentinel slot per top-k lane, never fewer than 8.
export fn ss_sentinels(top_k: i64) -> i64 {
    if top_k > SS_MIN_SENTINELS {
        return top_k;
    }
    return SS_MIN_SENTINELS;
}

// session.cpp:49-81, the whole function as one decision. -1 mirrors the native
// -1 return; 0 mirrors "budget off"; >= 1 is the slot count.
// bytes_per_slot is the summed expert-tensor bytes divided by n_expert, which
// the carrier computes over the gguf offset table.
export fn ss_hot_slots(budget_mb: i64, meta_ok: i64, n_expert: i64, bytes_per_slot: i64, top_k: i64) -> i64 {
    if budget_mb <= 0 {
        return 0;
    }
    if meta_ok == 0 || n_expert <= 0 {
        return -1;
    }
    if bytes_per_slot <= 0 {
        return -1;
    }
    let sent = ss_sentinels(top_k);
    let total = (budget_mb * SS_MIB) / bytes_per_slot;
    if total <= sent {
        return -1;
    }
    let slots = total - sent;
    if slots > n_expert {
        return n_expert;
    }
    return slots;
}

// The same decision's failure code, so a caller can pick the native message.
export fn ss_hot_reason(budget_mb: i64, meta_ok: i64, n_expert: i64, bytes_per_slot: i64, top_k: i64) -> i64 {
    if budget_mb <= 0 {
        return SS_OK;
    }
    if meta_ok == 0 || n_expert <= 0 {
        return SS_ERR_NO_META;
    }
    if bytes_per_slot <= 0 {
        return SS_ERR_NO_TENSORS;
    }
    let sent = ss_sentinels(top_k);
    let total = (budget_mb * SS_MIB) / bytes_per_slot;
    if total <= sent {
        return SS_ERR_TOO_SMALL;
    }
    return SS_OK;
}

// session.cpp:74-79 — the MiB figure the too-small message quotes:
//   (bytes_per_slot * sentinels + MiB - 1) / MiB
export fn ss_sentinel_mib(bytes_per_slot: i64, top_k: i64) -> i64 {
    if bytes_per_slot <= 0 {
        return 0;
    }
    let bytes = bytes_per_slot * ss_sentinels(top_k);
    return (bytes + SS_MIB - 1) / SS_MIB;
}

// ── the preflight clamp ─────────────────────────────────────────────────────

// session.cpp:669-685. The effective budget after the pre-load check:
//   -1  the preflight fails ("at least 4096 MiB must remain ...", 673-676)
//   otherwise min(budget_mb, free_mib - 4096), or budget_mb untouched when the
//   check does not apply (budget off, or no GPU layers asked for).
export fn ss_vram_clamp(free_mib: i64, budget_mb: i64, n_gpu_layers: i64) -> i64 {
    if budget_mb <= 0 || n_gpu_layers == 0 {
        return budget_mb;
    }
    if free_mib <= SS_VRAM_RESERVE_MIB {
        return -1;
    }
    let safe = free_mib - SS_VRAM_RESERVE_MIB;
    if budget_mb > safe {
        return safe;
    }
    return budget_mb;
}

// session.cpp:868 — the reserve the post-load probe must leave for the MTP
// draft context's own KV and compute buffers.
export fn ss_probe_margin(is_mtp: i64) -> i64 {
    if is_mtp != 0 {
        return SS_PROBE_MARGIN_BASE + SS_PROBE_MARGIN_MTP;
    }
    return SS_PROBE_MARGIN_BASE;
}

// session.cpp:869 — the budget the probe's own free-VRAM reading implies.
export fn ss_probe_budget(free_mib: i64, is_mtp: i64) -> i64 {
    let margin = ss_probe_margin(is_mtp);
    if free_mib > margin {
        return free_mib - margin;
    }
    return 0;
}

// session.cpp:856-902 — what the probe does to the open-time plan.
// out[0] action (SS_KEEP / SS_RESIZE / SS_DISABLE), out[1] effective budget MiB,
// out[2] slots per layer. The probe only runs for a live hot store on a GPU
// session; otherwise the plan is kept untouched.
export fn ss_resize_plan(
    plan_slots: i64,
    plan_mib: i64,
    n_gpu_layers: i64,
    free_mib: i64,
    is_mtp: i64,
    meta_ok: i64,
    n_expert: i64,
    bytes_per_slot: i64,
    top_k: i64,
    out: i64[:]
) -> i64 {
    var action: i64 = SS_KEEP;
    var budget: i64 = plan_mib;
    var slots: i64 = plan_slots;
    if plan_slots > 0 && n_gpu_layers != 0 && plan_mib > 0 {
        let pb = ss_probe_budget(free_mib, is_mtp);
        if pb < plan_mib {
            var s: i64 = -1;
            if pb > 0 {
                s = ss_hot_slots(pb, meta_ok, n_expert, bytes_per_slot, top_k);
            }
            if s > 0 {
                action = SS_RESIZE;
                budget = pb;
                slots = s;
            } else {
                // 886-897: too little left for the sentinels. The streamed path
                // stays lossless without a hot tier; the budget itself is not
                // rewritten by the native code.
                action = SS_DISABLE;
                slots = 0;
            }
        }
    }
    if out.len >= 1 {
        out[0] = action;
    }
    if out.len >= 2 {
        out[1] = budget;
    }
    if out.len >= 3 {
        out[2] = slots;
    }
    return action;
}

// ── the teardown order ──────────────────────────────────────────────────────

// Stage ids are the Impl member declaration order (session.cpp:322-370), which
// is NOT the teardown order; that is the whole point of ~Impl stating one.
const SS_ST_MODEL: i64 = 0;
const SS_ST_CTX: i64 = 1;
const SS_ST_HOOK: i64 = 2;
const SS_ST_WORKLOAD: i64 = 3;
const SS_ST_SOURCE: i64 = 4;
const SS_ST_CTX_DFT: i64 = 5;
const SS_ST_MTP: i64 = 6;
const SS_ST_MTP_BATCH: i64 = 7;
const SS_ST_SMPL: i64 = 8;
const SS_ST_BACKEND: i64 = 9;

// session.cpp:415-431, one rank per stage, 0 destroyed first:
//   source.shutdown() -> smpl -> mtp -> mtp_batch -> ctx_dft -> ctx -> hook ->
//   model -> backend
// -1 for a stage ~Impl does not touch explicitly (workload, or a bad id).
export fn ss_teardown_rank(stage: i64) -> i64 {
    if stage == SS_ST_SOURCE {
        return 0;
    }
    if stage == SS_ST_SMPL {
        return 1;
    }
    if stage == SS_ST_MTP {
        return 2;
    }
    if stage == SS_ST_MTP_BATCH {
        return 3;
    }
    if stage == SS_ST_CTX_DFT {
        return 4;
    }
    if stage == SS_ST_CTX {
        return 5;
    }
    if stage == SS_ST_HOOK {
        return 6;
    }
    if stage == SS_ST_MODEL {
        return 7;
    }
    if stage == SS_ST_BACKEND {
        return 8;
    }
    return -1;
}

// Does `a` have to be destroyed before `b`? Exactly the relation ~Impl encodes.
export fn ss_teardown_before(a: i64, b: i64) -> i64 {
    let ra = ss_teardown_rank(a);
    let rb = ss_teardown_rank(b);
    if ra < 0 || rb < 0 {
        return 0;
    }
    if ra < rb {
        return 1;
    }
    return 0;
}

// Is `order` a valid teardown sequence: every stage known and strictly later
// than its predecessor? The gate feeds it the native order and permutations.
export fn ss_teardown_check(order: const i64[:]) -> i64 {
    var prev: i64 = -1;
    var i: i64 = 0;
    while i < order.len {
        let r = ss_teardown_rank(order[i]);
        if r < 0 {
            return 0;
        }
        if r <= prev {
            return 0;
        }
        prev = r;
        i = i + 1;
    }
    if order.len == 0 {
        return 0;
    }
    return 1;
}
