// P9-01 S1: the config-consistency gate in Goose.
//
// Native source: runtime/core/src/config.cpp, bmoe::validate(const RunConfig &)
// (the whole function, 25-249), declared in runtime/core/include/bmoe/config.h.
// The native header calls it "pure policy (no llama.cpp dependency)"; this
// module is the same decision chain with numbers instead of English texts, so
// it can be compared against the real function with no model.
//
// Flat layout, because the export ABI takes scalars and slices only. `i` is the
// integer half of the RunConfig as validate() reads it and `f` the float half.
// Every index names the native field, in the order the header declares them.
//
//   i[0]  n_predict                    i[17] moe.io_threads
//   i[1]  n_threads                    i[18] moe.cache_mb
//   i[2]  n_ctx                        i[19] moe.cache_auto
//   i[3]  n_ubatch                     i[20] moe.cache_floor_mb
//   i[4]  n_expert_used                i[21] moe.cache_ceil_mb
//   i[5]  vram_cache_mb                i[22] moe.prefetch_layers
//   i[6]  expert_hot_slots             i[23] moe.io_two_wave
//   i[7]  n_gpu_layers                 i[24] moe.predict_log
//   i[8]  !model_path.empty()          i[25] moe.predict_prefetch
//   i[9]  !workload_capture_path.empty()  i[26] moe.predict_spec_max
//   i[10] !workload_replay_path.empty()  i[27] moe.route_ahead
//   i[11] sampling.top_k               i[28] moe.row_stream
//   i[12] spec.source (0 none, 1 mtp, 2 ngram)  i[29] moe.row_stream_mb
//   i[13] spec.draft_max               i[30] moe.overlap
//   i[14] spec.ngram_min_match         i[31] moe.force_cache
//   i[15] spec.ngram_max_match         i[16] moe.enabled
//
//   f[0] sampling.temp   f[1] sampling.top_p   f[2] spec.draft_p_min
//   f[3] moe.drop_cold_frac   f[4] moe.substitute_lambda
//
// The return value is the number of the FIRST rule that fails, in the native
// order (a later rule that would also fail is never reached), or 0 for a valid
// config. A caller that wants the text keeps it in the carrier: the native
// messages name values and remedies, which is presentation, not policy.
//
// Constants that the header owns are repeated here because the module must
// build standalone; a change in config.h without a change here is caught by the
// gate, which builds against the real header.

const CFG_OK: i64 = 0;

// Scalar rules (config.cpp:9-55).
const CFG_R_MODEL_PATH: i64 = 1;      //  9-11
const CFG_R_N_PREDICT: i64 = 2;       // 12-14
const CFG_R_N_THREADS: i64 = 3;       // 15-17
const CFG_R_N_CTX: i64 = 4;           // 18-20
const CFG_R_UBATCH_NEG: i64 = 5;      // 24-26
const CFG_R_UBATCH_OVER: i64 = 6;     // 27-31
const CFG_R_EXPERT_USED_NEG: i64 = 7; // 35-37
const CFG_R_WORKLOAD_BOTH: i64 = 8;   // 38-40
const CFG_R_WORKLOAD_MOE: i64 = 9;    // 41-43
const CFG_R_WORKLOAD_SPEC: i64 = 10;  // 44-46
const CFG_R_VRAM_NEG: i64 = 11;       // 47-49
const CFG_R_SLOTS_NEG: i64 = 12;      // 50-52
const CFG_R_VRAM_GPU: i64 = 13;       // 53-56

// Sampling (config.cpp:58-69).
const CFG_R_TOPK: i64 = 14;
const CFG_R_TOPP: i64 = 15;

// Speculation (config.cpp:71-119).
const CFG_R_SPEC_SAMPLING: i64 = 16;
const CFG_R_DRAFT_RANGE: i64 = 17;
const CFG_R_DRAFT_PMIN: i64 = 18;
const CFG_R_NGRAM_PMIN: i64 = 19;
const CFG_R_NGRAM_MAX: i64 = 20;
const CFG_R_NGRAM_MIN: i64 = 21;
const CFG_R_VERIFY_UBATCH: i64 = 22;

// Streaming switches outside the enabled block (config.cpp:121-160).
const CFG_R_OVERLAP: i64 = 23;
const CFG_R_PREDICT_LOG: i64 = 24;
const CFG_R_PREDICT_PREFETCH: i64 = 25;
const CFG_R_ROUTE_AHEAD: i64 = 26;
const CFG_R_ROW_STREAM: i64 = 27;
const CFG_R_ROW_STREAM_MB: i64 = 28;

// Inside `if (cfg.moe.enabled)` (config.cpp:162-247).
const CFG_R_IO_THREADS: i64 = 29;
const CFG_R_CACHE_MB_NEG: i64 = 30;
const CFG_R_CACHE_BAND: i64 = 31;
const CFG_R_PREFETCH_RANGE: i64 = 32;
const CFG_R_CACHE_AUTO_BOTH: i64 = 33;
const CFG_R_FLOOR_NEG: i64 = 34;
const CFG_R_CEIL_NEG: i64 = 35;
const CFG_R_PREFETCH_CACHE: i64 = 36;
const CFG_R_PREDICT_CACHE: i64 = 37;
const CFG_R_TWO_WAVE_OVERLAP: i64 = 38;
const CFG_R_TWO_WAVE_CACHE: i64 = 39;
const CFG_R_SPEC_MAX: i64 = 40;
const CFG_R_TWO_PREDICTORS: i64 = 41;
const CFG_R_ROUTE_AHEAD_RANGE: i64 = 42;
const CFG_R_ROUTE_AHEAD_EXCL: i64 = 43;
const CFG_R_ROUTE_AHEAD_SPEC: i64 = 44;
const CFG_R_DROP_CACHE: i64 = 45;
const CFG_R_DROP_RANGE: i64 = 46;
const CFG_R_SUB_CACHE: i64 = 47;
const CFG_R_SUB_RANGE: i64 = 48;

// config.h: MoeStreamConfig / SpecConfig limits at the base commit.
const CFG_CACHE_MIN_MB: i64 = 1500;
const CFG_IO_THREADS_MAX: i64 = 8;
const CFG_PREFETCH_MAX: i64 = 8;
const CFG_ROUTE_AHEAD_MAX: i64 = 8;
const CFG_DRAFT_MAX: i64 = 8;
const CFG_NGRAM_MATCH_MAX: i64 = 64;
const CFG_PREDICT_SPEC_MAX: i64 = 8;

const CFG_SRC_NONE: i64 = 0;
const CFG_SRC_MTP: i64 = 1;
const CFG_SRC_NGRAM: i64 = 2;

fn cfg_need(i: const i64[:], f: const f64[:]) -> i64 {
    if i.len < 32 || f.len < 5 {
        return -1; // caller error: the flat layout was not filled
    }
    return 0;
}

// config.cpp:5-249, rule by rule. Each block quotes the native condition it
// reproduces; the order of the blocks IS the contract.
export fn cfg_validate(i: const i64[:], f: const f64[:]) -> i64 {
    if cfg_need(i, f) != 0 {
        return -1;
    }

    // if (cfg.model_path.empty()) return fail(...)
    if i[8] == 0 {
        return CFG_R_MODEL_PATH;
    }
    if i[0] <= 0 {
        return CFG_R_N_PREDICT;
    }
    if i[1] <= 0 {
        return CFG_R_N_THREADS;
    }
    if i[2] <= 0 {
        return CFG_R_N_CTX;
    }
    if i[3] < 0 {
        return CFG_R_UBATCH_NEG;
    }
    // 0 means "as wide as the context"; a larger value would reserve compute
    // buffers for a batch that cannot arrive.
    if i[3] > i[2] {
        return CFG_R_UBATCH_OVER;
    }
    if i[4] < 0 {
        return CFG_R_EXPERT_USED_NEG;
    }
    if i[9] != 0 && i[10] != 0 {
        return CFG_R_WORKLOAD_BOTH;
    }
    if (i[9] != 0 || i[10] != 0) && i[16] == 0 {
        return CFG_R_WORKLOAD_MOE;
    }
    if (i[9] != 0 || i[10] != 0) && i[12] != CFG_SRC_NONE {
        return CFG_R_WORKLOAD_SPEC;
    }
    if i[5] < 0 {
        return CFG_R_VRAM_NEG;
    }
    if i[6] < 0 {
        return CFG_R_SLOTS_NEG;
    }
    if (i[5] > 0 || i[6] > 0) && i[7] == 0 {
        return CFG_R_VRAM_GPU;
    }

    // Sampling ranges bind only when sampling is on; under greedy the knobs are
    // inert and an out-of-range value still describes a valid run.
    if f[0] > 0.0 {
        if i[11] < 0 {
            return CFG_R_TOPK;
        }
        if f[1] <= 0.0 || f[1] > 1.0 {
            return CFG_R_TOPP;
        }
    }

    // Verification is token-identical only under argmax.
    if i[12] != CFG_SRC_NONE && f[0] > 0.0 {
        return CFG_R_SPEC_SAMPLING;
    }
    if i[12] != CFG_SRC_NONE && (i[13] < 1 || i[13] > CFG_DRAFT_MAX) {
        return CFG_R_DRAFT_RANGE;
    }
    if i[12] == CFG_SRC_MTP && (f[2] < 0.0 || f[2] > 1.0) {
        return CFG_R_DRAFT_PMIN;
    }
    // A knob for the other source is refused, not silently ignored.
    if i[12] == CFG_SRC_NGRAM && f[2] != 0.0 {
        return CFG_R_NGRAM_PMIN;
    }
    if i[12] == CFG_SRC_NGRAM && (i[15] < 1 || i[15] > CFG_NGRAM_MATCH_MAX) {
        return CFG_R_NGRAM_MAX;
    }
    if i[12] == CFG_SRC_NGRAM && (i[14] < 1 || i[14] > i[15]) {
        return CFG_R_NGRAM_MIN;
    }
    // The verify batch must fit in one graph; 0 means "as wide as the context".
    if i[12] != CFG_SRC_NONE && i[3] > 0 && i[3] < i[13] + 1 {
        return CFG_R_VERIFY_UBATCH;
    }

    if i[30] != 0 && i[16] == 0 {
        return CFG_R_OVERLAP;
    }
    if i[24] != 0 && i[16] == 0 {
        return CFG_R_PREDICT_LOG;
    }
    if i[25] != 0 && i[16] == 0 {
        return CFG_R_PREDICT_PREFETCH;
    }
    if i[27] > 0 && i[16] == 0 {
        return CFG_R_ROUTE_AHEAD;
    }
    if i[28] != 0 && i[16] == 0 {
        return CFG_R_ROW_STREAM;
    }
    if i[29] < 0 {
        return CFG_R_ROW_STREAM_MB;
    }

    if i[16] != 0 {
        if i[17] < 1 || i[17] > CFG_IO_THREADS_MAX {
            return CFG_R_IO_THREADS;
        }
        if i[18] < 0 {
            return CFG_R_CACHE_MB_NEG;
        }
        // The pathological band: a cache below one token's working set thrashes.
        if i[18] > 0 && i[18] < CFG_CACHE_MIN_MB && i[31] == 0 {
            return CFG_R_CACHE_BAND;
        }
        if i[22] < 0 || i[22] > CFG_PREFETCH_MAX {
            return CFG_R_PREFETCH_RANGE;
        }
        if i[19] != 0 && i[18] > 0 {
            return CFG_R_CACHE_AUTO_BOTH;
        }
        if i[20] < 0 {
            return CFG_R_FLOOR_NEG;
        }
        if i[21] < 0 {
            return CFG_R_CEIL_NEG;
        }
        // "The LRU cache is on" is a fixed budget OR auto-sizing.
        if i[22] > 0 && !(i[18] > 0 || i[19] != 0) {
            return CFG_R_PREFETCH_CACHE;
        }
        if i[25] != 0 && !(i[18] > 0 || i[19] != 0) {
            return CFG_R_PREDICT_CACHE;
        }
        if i[23] != 0 && i[30] == 0 {
            return CFG_R_TWO_WAVE_OVERLAP;
        }
        if i[23] != 0 && !(i[18] > 0 || i[19] != 0) {
            return CFG_R_TWO_WAVE_CACHE;
        }
        if i[26] < 0 || i[26] > CFG_PREDICT_SPEC_MAX {
            return CFG_R_SPEC_MAX;
        }
        if i[25] != 0 && i[22] > 0 {
            return CFG_R_TWO_PREDICTORS;
        }
        if i[27] < 0 || i[27] > CFG_ROUTE_AHEAD_MAX {
            return CFG_R_ROUTE_AHEAD_RANGE;
        }
        if i[27] > 0 && (i[24] != 0 || i[25] != 0 || i[22] > 0) {
            return CFG_R_ROUTE_AHEAD_EXCL;
        }
        if i[27] > 0 && i[12] != CFG_SRC_NONE {
            return CFG_R_ROUTE_AHEAD_SPEC;
        }
        if f[3] > 0.0 && !(i[18] > 0 || i[19] != 0) {
            return CFG_R_DROP_CACHE;
        }
        // Negated inclusive range, so NaN (every comparison false) is rejected.
        if !(f[3] >= 0.0 && f[3] <= 1.0) {
            return CFG_R_DROP_RANGE;
        }
        if f[4] > 0.0 && !(i[18] > 0 || i[19] != 0) {
            return CFG_R_SUB_CACHE;
        }
        if !(f[4] >= 0.0 && f[4] <= 1.0) {
            return CFG_R_SUB_RANGE;
        }
    }

    return CFG_OK;
}
