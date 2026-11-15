// P9-05 S2: the runtime facade — one-shot open/generate and the
// RunConfig -> SessionConfig -> GenerateRequest path it walks.
//
// Native source: runtime/core/src/engine/runtime.cpp
//   session_config_from   runtime.cpp:9-26    the mapping, field by field
//   run()                 runtime.cpp:32-68   the order of the calls and the
//                                             two places it returns early
// plus the one context-width decision run() hands to Session::open():
//   cparams.n_ubatch      session.cpp:817     0 means "as wide as n_batch"
// and the RunResult invariant run() must not disturb:
//   `return session->generate(req, on_token, sink);`  runtime.cpp:67
//   (session.h: RunResult "cancelled ... ok stays true").
//
// Scope: the integer/boolean projection of the mapping. model_path, SamplingConfig,
// MoeStreamConfig and SpecConfig are copied whole by the carrier and are out of
// scope here; nothing in this module reads or rewrites them.
//
// Flat layout, cfg (const i64[:]) — RunConfig, integers only:
//   0 n_threads, 1 n_ctx, 2 n_ubatch, 3 n_gpu_layers, 4 vram_cache_mb,
//   5 expert_hot_slots, 6 chatml, 7 n_expert_used, 8 compute_trace_layers,
//   9 n_predict, 10 think, 11 progress
// Flat layout, sc (i64[:]) — SessionConfig, the same indices 0-8, then
//   9 n_batch (the derived field).
//
// Slice prefix: every export starts with rts_. (`rt_` is taken by
// goose/cpu/goose-meta-backend.goose: rt_n_threads_ok, rt_io_threads_ok.)

import std;

// rts_plan step codes, in the order run() executes them.
const RTS_VALIDATE: i64 = 1; // validate(cfg)                        (runtime.cpp:38)
const RTS_MAP: i64 = 2;      // session_config_from(cfg)             (runtime.cpp:45)
const RTS_OPEN: i64 = 3;     // Session::open(sc, ...)               (runtime.cpp:48)
const RTS_REQUEST: i64 = 4;  // GenerateRequest field by field       (runtime.cpp:55-65)
const RTS_GENERATE: i64 = 5; // session->generate(req, ...)          (runtime.cpp:67)

// rts_result codes.
const RTS_RES_ERROR: i64 = 0;     // ok == false
const RTS_RES_OK: i64 = 1;        // ok == true, completed
const RTS_RES_CANCELLED: i64 = 2; // ok == true, cancelled == true

// cfg/sc field indices (the layout above).
const RTS_N_THREADS: i64 = 0;
const RTS_N_CTX: i64 = 1;
const RTS_N_UBATCH: i64 = 2;
const RTS_N_GPU_LAYERS: i64 = 3;
const RTS_VRAM_CACHE_MB: i64 = 4;
const RTS_EXPERT_HOT_SLOTS: i64 = 5;
const RTS_CHATML: i64 = 6;
const RTS_N_EXPERT_USED: i64 = 7;
const RTS_COMPUTE_TRACE_LAYERS: i64 = 8;
const RTS_N_PREDICT: i64 = 9;
const RTS_THINK: i64 = 10;
const RTS_PROGRESS: i64 = 11;
const RTS_N_BATCH: i64 = 9; // sc only: the derived field, no RunConfig counterpart
const RTS_SC_FIELDS: i64 = 10;

// ── session_config_from ─────────────────────────────────────────────────────

// runtime.cpp:14 — sc.n_batch = cfg.n_ctx, so any prompt that fits the context
// prefills in one batch. This is the mapping's only derived field.
export fn rts_n_batch(n_ctx: i64) -> i64 {
    return n_ctx;
}

// runtime.cpp:9-26 — the projection, in the native assignment order. Returns
// the number of fields written (RTS_SC_FIELDS) or -1 when a slice is too short
// to hold the result; a caller that gets -1 must not read `sc`.
export fn rts_config(cfg: const i64[:], sc: i64[:]) -> i64 {
    if cfg.len < 12 || sc.len < RTS_SC_FIELDS {
        return -1;
    }
    sc[RTS_N_THREADS] = cfg[RTS_N_THREADS];
    sc[RTS_N_CTX] = cfg[RTS_N_CTX];
    sc[RTS_N_BATCH] = rts_n_batch(cfg[RTS_N_CTX]);
    sc[RTS_N_UBATCH] = cfg[RTS_N_UBATCH]; // 0 stays 0: the context widens it, see rts_ubatch
    sc[RTS_N_GPU_LAYERS] = cfg[RTS_N_GPU_LAYERS];
    sc[RTS_VRAM_CACHE_MB] = cfg[RTS_VRAM_CACHE_MB];
    sc[RTS_EXPERT_HOT_SLOTS] = cfg[RTS_EXPERT_HOT_SLOTS];
    sc[RTS_CHATML] = cfg[RTS_CHATML];
    sc[RTS_N_EXPERT_USED] = cfg[RTS_N_EXPERT_USED];
    sc[RTS_COMPUTE_TRACE_LAYERS] = cfg[RTS_COMPUTE_TRACE_LAYERS];
    return RTS_SC_FIELDS;
}

// session.cpp:817 — cparams.n_ubatch = cfg.n_ubatch > 0 ? cfg.n_ubatch : cfg.n_batch.
// run() sets n_batch = n_ctx, so the fallback is the context width.
export fn rts_ubatch(n_ubatch: i64, n_batch: i64) -> i64 {
    if n_ubatch > 0 {
        return n_ubatch;
    }
    return n_batch;
}

// ── run()'s order ───────────────────────────────────────────────────────────

// runtime.cpp:38-53 — which calls run() makes, in order, and where it stops.
// A failed validate returns before mapping; a failed Session::open returns
// before the request is built. Nothing runs after a failure, which is what
// keeps the two early returns from touching session state at all.
// out receives the executed steps; the return value is their count.
export fn rts_plan(validate_ok: i64, open_ok: i64, out: i64[:]) -> i64 {
    var n: i64 = 0;
    if out.len >= 1 {
        out[0] = RTS_VALIDATE;
    }
    n = 1;
    if validate_ok == 0 || n >= out.len {
        return n;
    }
    out[1] = RTS_MAP;
    n = 2;
    if n >= out.len {
        return n;
    }
    out[2] = RTS_OPEN;
    n = 3;
    if open_ok == 0 || n >= out.len {
        return n;
    }
    out[3] = RTS_REQUEST;
    n = 4;
    if n >= out.len {
        return n;
    }
    out[4] = RTS_GENERATE;
    return 5;
}

// runtime.cpp:55-65 — the GenerateRequest fields run() sets, and only those:
//   req.prompt = cfg.prompt                 (carrier: string, out of scope)
//   req.n_predict = cfg.n_predict
//   req.think = cfg.think
//   req.clear_kv = true                     // one-shot run is always a new chat
//   req.render_text = cfg.progress          // only --progress reads the parse
//   (workload paths copied as-is; empty means absent)
// out: 0 clear_kv, 1 n_predict, 2 think, 3 render_text.
export fn rts_request(n_predict: i64, think: i64, progress: i64, out: i64[:]) -> i64 {
    if out.len < 4 {
        return -1;
    }
    out[0] = 1; // clear_kv = true, unconditionally
    out[1] = n_predict;
    out[2] = think;
    out[3] = progress; // render_text = cfg.progress
    return 4;
}

// runtime.cpp:67 / session.h RunResult — run() returns generate()'s result
// unchanged, so the invariant a caller may rely on is generate()'s: a
// cancelled run is still ok, and only a failure is not.
export fn rts_result(ok: i64, cancelled: i64) -> i64 {
    if ok == 0 {
        return RTS_RES_ERROR;
    }
    if cancelled != 0 {
        return RTS_RES_CANCELLED;
    }
    return RTS_RES_OK;
}
