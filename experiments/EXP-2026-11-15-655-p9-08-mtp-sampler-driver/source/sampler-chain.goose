// P9-08 G5: the sampler chain construction (session.cpp:937-945).
//
// Native source: runtime/core/src/engine/session.cpp at base commit 050354e9,
// the opt-in sampling block:
//
//   static_assert(SamplingConfig{}.seed == LLAMA_DEFAULT_SEED, ...);
//   if (cfg.sampling.temp > 0.0f) {
//       im.smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
//       llama_sampler_chain_add(im.smpl, llama_sampler_init_top_k(cfg.sampling.top_k));
//       llama_sampler_chain_add(im.smpl, llama_sampler_init_top_p(cfg.sampling.top_p, 1));
//       llama_sampler_chain_add(im.smpl, llama_sampler_init_temp(cfg.sampling.temp));
//       llama_sampler_chain_add(im.smpl, llama_sampler_init_dist(cfg.sampling.seed));
//   }
//
// The four llama_sampler_* calls are the substrate and stay in the carrier: they
// allocate a chain, add a stage and hand back a pointer. What is portable is the
// DECISION around them, and it is two decisions, not one:
//
//   * whether a chain exists at all (temp <= 0 leaves `smpl` null and the decode
//     loop on argmax — the deterministic path the byte-identity gates rely on);
//   * which stages, in which order, with which arguments, once it does.
//
// The order is not interchangeable. upsteam applies the chain in the order the
// stages were added, so top_k before top_p before temp before the final dist is
// the standard shape; any other order samples a different distribution. The
// `min_keep = 1` on top_p is what keeps the nucleus filter from emptying a
// distribution with a single sharp candidate, and the seed handed to dist is the
// caller's seed verbatim (`LLAMA_DEFAULT_SEED` means "random per run").
//
// The prefix is sch_. Plan layout, one stage per stride of three i64 and one f64:
//
//   out[3*k + 0] = stage kind (SCH_TOP_K .. SCH_DIST)
//   out[3*k + 1] = integer argument (top_k; the seed for dist; 0 elsewhere)
//   out[3*k + 2] = min_keep (1 on top_p, 0 elsewhere)
//   outf[k]      = floating argument (top_p; temp; 0.0 elsewhere)
//
// The plan returns the number of stages, and the carrier walks it calling the
// matching llama_sampler_init_* for each entry.

const SCH_NONE: i64 = 0;
const SCH_TOP_K: i64 = 1;
const SCH_TOP_P: i64 = 2;
const SCH_TEMP: i64 = 3;
const SCH_DIST: i64 = 4;

// SamplingConfig::seed default, which session.cpp static_asserts to be
// LLAMA_DEFAULT_SEED (0xFFFFFFFFu). Kept as an i64 here because the gate ABI
// compares integers; the carrier's assertion is the same constant.
const SCH_DEFAULT_SEED: i64 = 4294967295;

// session.cpp:939 — `cfg.sampling.temp > 0.0f` is the whole opt-in. temp == 0
// and temp < 0 are the greedy path, and a NaN temp (a bad --temp parse) is false
// too, exactly like the native comparison.
export fn sch_on(temp: f64) -> i64 {
    if temp > 0.0 {
        return 1;
    }
    return 0;
}

// The stage count for a given temp: four when sampling is on, none otherwise.
// A separate export so a caller can size its buffers before sch_plan runs.
export fn sch_entries(temp: f64) -> i64 {
    if temp > 0.0 {
        return 4;
    }
    return 0;
}

// session.cpp:937-938 — the assertion that the config default and llama.cpp's
// sentinel agree. The module states the constant so a config.h edit that moves
// the default is caught here rather than at the first sampled token.
export fn sch_default_seed() -> i64 {
    return SCH_DEFAULT_SEED;
}

export fn sch_seed_gate(seed: i64) -> i64 {
    if seed == SCH_DEFAULT_SEED {
        return 1;
    }
    return 0;
}

// top_p's min_keep. One, so a sharp distribution still has a candidate to fall
// back on; 0 would let the nucleus stage empty the candidate list.
export fn sch_min_keep() -> i64 {
    return 1;
}

fn sch_put(out: i64[:], outf: f64[:], n: i64, kind: i64, iarg: i64, min_keep: i64, farg: f64) -> i64 {
    if 3 * n + 2 < out.len && n < outf.len {
        out[3 * n] = kind;
        out[3 * n + 1] = iarg;
        out[3 * n + 2] = min_keep;
        outf[n] = farg;
    }
    return n + 1;
}

// session.cpp:940-945 — the chain, in add order. The buffer guards are the
// module's own (the native code cannot run out: it allocates per call); a plan
// that does not fit returns the full count anyway, so a caller can compare it
// with sch_entries and refuse rather than silently sampling differently.
fn sch_plan_impl(temp: f64, top_k: i64, top_p: f64, seed: i64, out: i64[:], outf: f64[:]) -> i64 {
    if temp <= 0.0 {
        return 0;
    }
    var n: i64 = 0;
    n = sch_put(out, outf, n, SCH_TOP_K, top_k, 0, 0.0);
    n = sch_put(out, outf, n, SCH_TOP_P, 0, 1, top_p);
    n = sch_put(out, outf, n, SCH_TEMP, 0, 0, temp);
    n = sch_put(out, outf, n, SCH_DIST, seed, 0, 0.0);
    return n;
}

export fn sch_plan(temp: f64, top_k: i64, top_p: f64, seed: i64, out: i64[:], outf: f64[:]) -> i64 {
    return sch_plan_impl(temp, top_k, top_p, seed, out, outf);
}
