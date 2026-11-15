// P9-08 S1: what the engine admits at Session::open(), and in which order it
// brings its subsystems up.
//
// Native source: runtime/core/src/engine/session.cpp, Session::open()
// (session.cpp:628-1271). The load itself, the gguf parse, the backend calls
// and every allocation stay native; this module owns the decisions that
// surround them: which refusal fires first, which model/context parameters the
// flags project to, and the ordered stage list that an open() walks.
//
// Already accepted elsewhere, NOT repeated here:
//   ss_*  (EXP632) hot_slots_from_budget, the VRAM preflight clamp, the
//         post-context probe, the Impl teardown order;
//   nd_*  (EXP632) load_mtp, the MTP head admission, the streamed span,
//         n_rs_seq, the draft/verify widths;
//   rts_* (EXP632) session_config_from and the run() call order.
// This slice starts after the clamp and takes the already-clamped budget as an
// input, takes the head admission as settled, and never re-derives a width.
//
// Every export names the native lines it reproduces. Numbers, not texts: the
// error strings stay the carrier's.

import std;

// ── lga_hot_admit ───────────────────────────────────────────────────────────

// session.cpp:693-697 — an explicit expert_hot_slots above the model's expert
// count is refused, not clamped:
//   if (expert_hot_slots > info.n_expert) return fail("expert_hot_slots=... exceeds ...");
// The check runs only when slots were asked for; 0 passes untouched, and the
// budget-derived path is ss_hot_slots' (EXP632), which already caps at n_expert.
export fn lga_hot_admit(hot_slots: i64, n_expert: i64) -> i64 {
    if hot_slots > 0 && hot_slots > n_expert {
        return 1;
    }
    return 0;
}

// ── lga_override_admit ──────────────────────────────────────────────────────

// lga_override_admit results — the n_expert_used override gates.
const LGA_OV_APPLIED: i64 = 0;   // kv_override armed                       (737-751)
const LGA_OV_NOT_SET: i64 = 1;   // n_expert_used <= 0: nothing to override  (739)
const LGA_OV_NO_META: i64 = 2;   // "cannot read gguf metadata"              (740)
const LGA_OV_NO_ARCH: i64 = 3;   // "gguf has no general.architecture"       (741)
const LGA_OV_DENSE: i64 = 4;     // "the model is not MoE"                   (742-743)
const LGA_OV_TOO_BIG: i64 = 5;   // "n_expert_used=... exceeds ..."          (744-746)

// session.cpp:737-751 — one ordered admission for the active-expert override.
// The gguf is consulted before anything is written into kv_overrides, and the
// four refusals are tested in this order: meta, arch, dense, range. A dense or
// metadata-less model is refused before the range is even compared, so the
// first true condition is the message the user sees.
export fn lga_override_admit(
    n_expert_used: i64,
    meta_ok: i64,
    arch_len: i64,
    n_expert: i64
) -> i64 {
    if n_expert_used <= 0 {
        return LGA_OV_NOT_SET;
    }
    if meta_ok == 0 {
        return LGA_OV_NO_META;
    }
    if arch_len <= 0 {
        return LGA_OV_NO_ARCH;
    }
    if n_expert <= 0 {
        return LGA_OV_DENSE;
    }
    if n_expert_used > n_expert {
        return LGA_OV_TOO_BIG;
    }
    return LGA_OV_APPLIED;
}

// session.cpp:748-751 — the override is an INT tag on "<arch>.expert_used_count":
//   kv_overrides[0].tag = LLAMA_KV_OVERRIDE_TYPE_INT;
//   snprintf(key, sizeof(key), "%s", (arch + ".expert_used_count").c_str());
//   kv_overrides[0].val_i64 = n_expert_used;
// The tag id is llama.h's enum value 1 for INT. Key text stays the carrier's;
// the two facts a gate can check without a gguf are the tag and the value.
export fn lga_override_tag(code: i64) -> i64 {
    if code == LGA_OV_APPLIED {
        return 1;
    }
    return 0;
}

export fn lga_override_value(code: i64, n_expert_used: i64) -> i64 {
    if code == LGA_OV_APPLIED {
        return n_expert_used;
    }
    return 0;
}

// ── lga_mparams ─────────────────────────────────────────────────────────────

// lga_mparams flags, the llama_model_params fields open() writes.
const LGA_MP_MMAP: i64 = 1;      // mparams.load_mode = MMAP              (700)
const LGA_MP_NO_BUFT: i64 = 2;   // mparams.use_extra_bufts = false       (701)
const LGA_MP_CPU_ONLY: i64 = 4;  // mparams.devices = {cpu, null}         (705-708)
const LGA_MP_EXPERT_BUFT: i64 = 8; // tensor_buft_overrides armed         (716-719)
const LGA_MP_LOAD_MTP: i64 = 16; // mparams.load_mtp = spec.is_mtp()      (722)

// session.cpp:699-722 — the model-parameter projection. Two arms are
// conditional and both are easy to get wrong:
//   - the CPU-only device list is installed only when n_gpu_layers == 0 AND a
//     CPU device exists (an explicit list keeps the CUDA scheduler out of the
//     CPU baseline's callback ordering);
//   - the expert buft override is armed only when something needs experts on
//     the CPU (moe enabled or a hot store) AND the session is on the GPU.
export fn lga_mparams(
    n_gpu_layers: i64,
    cpu_dev_present: i64,
    moe_enabled: i64,
    hot_slots: i64,
    is_mtp: i64
) -> i64 {
    var m: i64 = LGA_MP_MMAP + LGA_MP_NO_BUFT;
    if n_gpu_layers == 0 && cpu_dev_present != 0 {
        m = m + LGA_MP_CPU_ONLY;
    }
    if (moe_enabled != 0 || hot_slots > 0) && n_gpu_layers != 0 {
        m = m + LGA_MP_EXPERT_BUFT;
    }
    if is_mtp != 0 {
        m = m + LGA_MP_LOAD_MTP;
    }
    return m;
}

// ── lga_cparams ─────────────────────────────────────────────────────────────

// session.cpp:819-847 — the context parameters that are not a width (those are
// nd_*'s). The hot-store triple is fixed: decay 0.98f, hysteresis 1.10f, dwell
// 100, in hundredths so the gate compares integers. The prepare callback is
// armed with the hot store and, under MoE, pointed at the stream source; the
// eval callback is installed for MoE OR for a compute trace alone, which is how
// a non-streamed run is measured.
//
// out: 0 expert_hot_s, 1 heat_decay_bp, 2 hyst_bp, 3 dwell, 4 prepare_armed,
//      5 cb_eval_armed.
export fn lga_cparams(
    hot_slots: i64,
    moe_enabled: i64,
    compute_trace: i64,
    out: i64[:]
) -> i64 {
    if out.len < 6 {
        return -1;
    }
    out[0] = hot_slots;
    out[1] = 98;
    out[2] = 110;
    out[3] = 100;
    var prepare: i64 = 0;
    if hot_slots > 0 && moe_enabled != 0 {
        prepare = 1;
    }
    out[4] = prepare;
    var cb: i64 = 0;
    if moe_enabled != 0 || compute_trace != 0 {
        cb = 1;
    }
    out[5] = cb;
    return 6;
}

// ── the init order ──────────────────────────────────────────────────────────

// Stage ids in the order Session::open() executes them. The list is the spine
// of open(), not its every statement: one id per subsystem that can fail.
const LGA_SG_BACKEND: i64 = 1;   // llama_backend_init                     (645)
const LGA_SG_META: i64 = 2;      // lazy gguf parse                        (655-667)
const LGA_SG_PREFLIGHT: i64 = 3; // clamped budget, pre-load refusal       (669-683)
const LGA_SG_HOTPLAN: i64 = 4;   // sizing or explicit slots + admit       (685-698)
const LGA_SG_MPARAMS: i64 = 5;   // model params + override                (699-751)
const LGA_SG_LOAD: i64 = 6;      // llama_model_load_from_file             (753-757)
const LGA_SG_HEAD: i64 = 7;      // n_layer_nextn + MTP head admission     (760-770)
const LGA_SG_ARCH: i64 = 8;      // arch string + recipe lookup            (772-783)
const LGA_SG_TEMPLATE: i64 = 9;  // chat templates + think control         (785-800)
const LGA_SG_HOOK: i64 = 10;     // RouterHook + its policy knobs          (802-813)
const LGA_SG_CPARAMS: i64 = 11;  // context params, callbacks, n_rs_seq    (815-851)
const LGA_SG_PROBE: i64 = 12;    // post-load VRAM probe                   (857-902)
const LGA_SG_CTX: i64 = 13;      // llama_init_from_model + threads        (904-907)
const LGA_SG_DCTX: i64 = 14;     // MTP draft context                      (909-931)
const LGA_SG_SAMPLER: i64 = 15;  // opt-in sampling chain                  (937-951)
const LGA_SG_ABORT: i64 = 16;    // abort callback                         (953-961)
const LGA_SG_MTP: i64 = 17;     // speculative driver                     (963-976)
const LGA_SG_VERIFY: i64 = 18;  // wide verify batch                      (978-986)
const LGA_SG_STREAM: i64 = 19;  // capture warm-up + stream source        (988-1150)
const LGA_SG_READY: i64 = 20;   // the session is usable                  (1271)

// session.cpp:628-1271 — the stage list, in order, and the first stage that
// refuses. fail[i] != 0 injects a failure at stage i+1, so the plan stops AT
// that stage and never enters its successors: a failed load never reaches the
// context, a failed context never reaches the stream source, and the backend is
// already up either way because ~Impl owns its teardown from the first line.
// out receives the stages entered, in order; the return value is their count,
// or -1 when either slice cannot hold the plan.
export fn lga_init_plan(fail: const i64[:], out: i64[:]) -> i64 {
    if fail.len < 20 || out.len < 20 {
        return -1;
    }
    var n: i64 = 0;
    var stage: i64 = LGA_SG_BACKEND;
    while stage <= LGA_SG_READY {
        out[n] = stage;
        n = n + 1;
        if fail[stage - 1] != 0 {
            return n;
        }
        stage = stage + 1;
    }
    return n;
}

// Which refusal a failing stage reports. 0 means "this stage cannot refuse".
export fn lga_refusal_code(stage: i64) -> i64 {
    if stage == LGA_SG_PREFLIGHT {
        return 1; // "VRAM preflight failed: ..."                  (673-676)
    }
    if stage == LGA_SG_HOTPLAN {
        return 2; // hot_slots_from_budget's own error, or "exceeds" (690-697)
    }
    if stage == LGA_SG_LOAD {
        return 3; // "failed to load model: ..."                    (755)
    }
    if stage == LGA_SG_HEAD {
        return 4; // "--mtp needs a model with a trained MTP head"  (765-770)
    }
    if stage == LGA_SG_ARCH {
        return 5; // "no MoE recipe for architecture '...'"         (779-782)
    }
    if stage == LGA_SG_CTX {
        return 6; // "failed to create context"                     (905)
    }
    if stage == LGA_SG_DCTX {
        return 7; // "failed to create the MTP draft context"       (929)
    }
    if stage == LGA_SG_MTP {
        return 8; // "failed to initialise MTP speculative decoding" (975)
    }
    if stage == LGA_SG_STREAM {
        return 9; // capture/offset/source/overlap failures         (997-1125)
    }
    return 0;
}

// The rank of a stage in the init order, 0-based; -1 for an unknown id. The
// gate compares the native order with permutations of it, the same way
// ss_teardown_check does for teardown.
export fn lga_stage_rank(stage: i64) -> i64 {
    return stage - 1;
}

export fn lga_stage_check(order: const i64[:]) -> i64 {
    var prev: i64 = -1;
    var i: i64 = 0;
    while i < order.len {
        let r = lga_stage_rank(order[i]);
        if r < 0 || r > 19 {
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
