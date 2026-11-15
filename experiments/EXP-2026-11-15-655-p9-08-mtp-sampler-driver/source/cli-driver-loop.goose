// P9-08 G7: the CLI driver loop's argument projection (gap G7).
//
// Native source: runtime/cli/main.cpp at base commit 050354e9:
//
//   590-776   main()'s parse loop: every flag, the value it consumes, the field
//             it writes, the code it exits with when the value is missing or the
//             argument is unknown
//   876-883   the workload/mode rejection (capture/replay vs session/ppl)
//   886-940   the --ppl command path: the file list, the request projection and
//             the report arithmetic
//
// `cc_` (goose/cli/config-resolve.goose, P9-01 S3) already owns the
// flag-vs-environment precedence and the --dense-weights vocabulary; nothing
// here repeats those. What is left, and what this slice owns, is the shape of
// the loop itself:
//
//   * which argument is a flag at all, and which id it is;
//   * whether it consumes the next token (a missing value is exit 1, not a
//     crash), and which field the stored value lands in;
//   * how the value is parsed: integer, float, unsigned, string, "sets a bool
//     true", "sets a bool false", the --cache-mb auto|number split, the
//     --dense-weights enum, a comma-separated list, or an action that ends the
//     run before a model is opened;
//   * the exit code of each refusal: a missing value and an unknown argument are
//     1 (the native prints usage), a draft-source contradiction and a bad
//     --dense-weights value are 2, the workload/mode conflict is 1, an
//     unreadable --ppl-list is 2 and an unreadable --ppl file is 2;
//   * the --ppl-list line filter (trailing CR and space trimmed, empty lines and
//     '#' comments skipped) and the report arithmetic (the next-token percentage
//     is 0.0 when nothing was scored, and the file label prints only when more
//     than one text is scored).
//
// Deliberately NOT here: argv -> string decoding (the carrier keeps argv), the
// number parsers (atoi/atof/strtoul stay native; cc_atoi is the accepted
// atoi-compatible one), the enum vocabulary (cc_dense_mode), the environment
// block (cc_env_int/cc_env_bool/cc_cache_auto) and every sink construction.
//
// The prefix is drv_. The flag table is the native loop's order, ids 0..62.

// ── flag ids ───────────────────────────────────────────────────────────────
const DRV_M: i64 = 0;
const DRV_P: i64 = 1;
const DRV_N: i64 = 2;
const DRV_T: i64 = 3;
const DRV_C: i64 = 4;
const DRV_UBATCH: i64 = 5;
const DRV_N_EXPERT_USED: i64 = 6;
const DRV_GPU_LAYERS: i64 = 7;
const DRV_VRAM_CACHE_MB: i64 = 8;
const DRV_EXPERT_HOT_SLOTS: i64 = 9;
const DRV_TEMP: i64 = 10;
const DRV_TOP_K: i64 = 11;
const DRV_TOP_P: i64 = 12;
const DRV_SEED: i64 = 13;
const DRV_MTP: i64 = 14;
const DRV_NGRAM: i64 = 15;
const DRV_DRAFT: i64 = 16;
const DRV_MTP_P_MIN: i64 = 17;
const DRV_NGRAM_MIN_MATCH: i64 = 18;
const DRV_CHATML: i64 = 19;
const DRV_NO_THINK: i64 = 20;
const DRV_PROGRESS: i64 = 21;
const DRV_SESSION: i64 = 22;
const DRV_CSV: i64 = 23;
const DRV_ROUTE_TRACE: i64 = 24;
const DRV_COMPUTE_TRACE: i64 = 25;
const DRV_COMPUTE_TRACE_LAYERS: i64 = 26;
const DRV_IO_TRACE: i64 = 27;
const DRV_WORKLOAD_CAPTURE: i64 = 28;
const DRV_WORKLOAD_REPLAY: i64 = 29;
const DRV_MOE_STREAM: i64 = 30;
const DRV_CACHE_MB: i64 = 31;
const DRV_CACHE_FLOOR_MB: i64 = 32;
const DRV_CACHE_CEIL_MB: i64 = 33;
const DRV_IO_THREADS: i64 = 34;
const DRV_NO_ODIRECT: i64 = 35;
const DRV_ROW_STREAM: i64 = 36;
const DRV_ROW_STREAM_MB: i64 = 37;
const DRV_DENSE_WEIGHTS: i64 = 38;
const DRV_NO_WARM_DENSE: i64 = 39;
const DRV_DENSE_ODIRECT: i64 = 40;
const DRV_LOAD_ALL: i64 = 41;
const DRV_FORCE_CACHE: i64 = 42;
const DRV_OVERLAP: i64 = 43;
const DRV_IO_TWO_WAVE: i64 = 44;
const DRV_PREFETCH: i64 = 45;
const DRV_PREFETCH_SYNC: i64 = 46;
const DRV_DROP_COLD_EXPERTS: i64 = 47;
const DRV_EXPERT_SUBSTITUTE: i64 = 48;
const DRV_PPL: i64 = 49;
const DRV_PPL_SKIP: i64 = 50;
const DRV_PPL_STEP: i64 = 51;
const DRV_PPL_LIST: i64 = 52;
const DRV_PPL_CHOICES: i64 = 53;
const DRV_DROP_NO_RENORM: i64 = 54;
const DRV_DROP_IN_PREFILL: i64 = 55;
const DRV_ROUTE_AHEAD: i64 = 56;
const DRV_PREDICT_LOG: i64 = 57;
const DRV_PREDICT_PREFETCH: i64 = 58;
const DRV_PREDICT_SPEC_MAX: i64 = 59;
const DRV_LIST_ARCHS: i64 = 60;
const DRV_HELP: i64 = 61;
const DRV_VERSION: i64 = 62;
const DRV_N_FLAGS: i64 = 63;

// ── projection fields ──────────────────────────────────────────────────────
const DRV_F_MODEL: i64 = 0;
const DRV_F_PROMPT: i64 = 1;
const DRV_F_N_PREDICT: i64 = 2;
const DRV_F_N_THREADS: i64 = 3;
const DRV_F_N_CTX: i64 = 4;
const DRV_F_N_UBATCH: i64 = 5;
const DRV_F_N_EXPERT_USED: i64 = 6;
const DRV_F_N_GPU_LAYERS: i64 = 7;
const DRV_F_VRAM_CACHE_MB: i64 = 8;
const DRV_F_EXPERT_HOT_SLOTS: i64 = 9;
const DRV_F_TEMP: i64 = 10;
const DRV_F_TOP_K: i64 = 11;
const DRV_F_TOP_P: i64 = 12;
const DRV_F_SEED: i64 = 13;
const DRV_F_SPEC_SOURCE: i64 = 14;
const DRV_F_DRAFT_MAX: i64 = 15;
const DRV_F_DRAFT_P_MIN: i64 = 16;
const DRV_F_NGRAM_MIN_MATCH: i64 = 17;
const DRV_F_CHATML: i64 = 18;
const DRV_F_THINK: i64 = 19;
const DRV_F_PROGRESS: i64 = 20;
const DRV_F_SESSION: i64 = 21;
const DRV_F_CSV: i64 = 22;
const DRV_F_ROUTE_TRACE: i64 = 23;
const DRV_F_COMPUTE_TRACE: i64 = 24;
const DRV_F_COMPUTE_TRACE_LAYERS: i64 = 25;
const DRV_F_IO_TRACE: i64 = 26;
const DRV_F_WORKLOAD_CAPTURE: i64 = 27;
const DRV_F_WORKLOAD_REPLAY: i64 = 28;
const DRV_F_MOE_ENABLED: i64 = 29;
const DRV_F_CACHE_MB: i64 = 30;
const DRV_F_CACHE_AUTO: i64 = 31;
const DRV_F_CACHE_FLOOR_MB: i64 = 32;
const DRV_F_CACHE_CEIL_MB: i64 = 33;
const DRV_F_IO_THREADS: i64 = 34;
const DRV_F_O_DIRECT: i64 = 35;
const DRV_F_ROW_STREAM: i64 = 36;
const DRV_F_ROW_STREAM_MB: i64 = 37;
const DRV_F_DENSE_WEIGHTS: i64 = 38;
const DRV_F_LOAD_ALL: i64 = 39;
const DRV_F_FORCE_CACHE: i64 = 40;
const DRV_F_OVERLAP: i64 = 41;
const DRV_F_IO_TWO_WAVE: i64 = 42;
const DRV_F_PREFETCH_LAYERS: i64 = 43;
const DRV_F_PREFETCH_SYNC: i64 = 44;
const DRV_F_DROP_COLD_FRAC: i64 = 45;
const DRV_F_SUBSTITUTE_LAMBDA: i64 = 46;
const DRV_F_PPL: i64 = 47;
const DRV_F_PPL_SKIP: i64 = 48;
const DRV_F_PPL_STEP: i64 = 49;
const DRV_F_PPL_LIST: i64 = 50;
const DRV_F_PPL_CHOICES: i64 = 51;
const DRV_F_DROP_RENORM: i64 = 52;
const DRV_F_DROP_PREFILL: i64 = 53;
const DRV_F_ROUTE_AHEAD: i64 = 54;
const DRV_F_PREDICT_LOG: i64 = 55;
const DRV_F_PREDICT_PREFETCH: i64 = 56;
const DRV_F_PREDICT_SPEC_MAX: i64 = 57;
const DRV_F_ACTION: i64 = 58;
const DRV_N_FIELDS: i64 = 59;

// ── parse kinds ────────────────────────────────────────────────────────────
const DRV_K_INT: i64 = 0;
const DRV_K_FLOAT: i64 = 1;
const DRV_K_U32: i64 = 2;
const DRV_K_STRING: i64 = 3;
const DRV_K_TRUE: i64 = 4;       // no value, sets the field to 1
const DRV_K_FALSE: i64 = 5;      // no value, sets the field to 0
const DRV_K_CACHE: i64 = 6;      // "auto" -> auto flag, anything else -> number
const DRV_K_DENSE: i64 = 7;      // mmap | warm | anon | ahwb (cc_dense_mode)
const DRV_K_CSV: i64 = 8;        // one field per comma, empty fields kept
const DRV_K_ACTION: i64 = 9;     // ends the run before any model is opened
const DRV_K_DRAFT_MTP: i64 = 10;
const DRV_K_DRAFT_NGRAM: i64 = 11;
const DRV_K_STRING_TRUE: i64 = 12; // stores the value and sets a second field true
const DRV_K_DENSE_MMAP: i64 = 13;  // deprecated alias, no value
const DRV_K_DENSE_ANON: i64 = 14;

// Action ids carried in the plan's argument slot for DRV_K_ACTION.
const DRV_ACT_HELP: i64 = 0;
const DRV_ACT_VERSION: i64 = 1;
const DRV_ACT_LIST_ARCHS: i64 = 2;

// ── exit codes ─────────────────────────────────────────────────────────────
const DRV_X_OK: i64 = 0;
const DRV_X_MISSING_VALUE: i64 = 1;
const DRV_X_UNKNOWN_ARG: i64 = 1;
const DRV_X_DRAFT_CONFLICT: i64 = 2;
const DRV_X_DENSE_ENUM: i64 = 2;
const DRV_X_WORKLOAD_CONFLICT: i64 = 1;
const DRV_X_PPL_LIST_OPEN: i64 = 2;
const DRV_X_PPL_FILE_OPEN: i64 = 2;
const DRV_X_PPL_FAIL: i64 = 1;
const DRV_X_USAGE: i64 = 1;

const DRV_BAD: i64 = -1;

fn drv_eq(a: const u8[:], b: const u8[:]) -> i64 {
    if a.len != b.len {
        return 0;
    }
    var i: i64 = 0;
    while i < a.len {
        if a[i] != b[i] {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

// main.cpp:606-776 — the argument table, in the native branch order. An alias is
// the same id as the flag it aliases, which is what makes `seen` bookkeeping and
// the flag-wins-over-env rule work for -m/--model alike.
fn drv_flag_of(name: const u8[:]) -> i64 {
    if drv_eq(name, "-m") != 0 || drv_eq(name, "--model") != 0 {
        return DRV_M;
    }
    if drv_eq(name, "-p") != 0 || drv_eq(name, "--prompt") != 0 {
        return DRV_P;
    }
    if drv_eq(name, "-n") != 0 || drv_eq(name, "--n-predict") != 0 {
        return DRV_N;
    }
    if drv_eq(name, "-t") != 0 || drv_eq(name, "--threads") != 0 {
        return DRV_T;
    }
    if drv_eq(name, "-c") != 0 || drv_eq(name, "--ctx-size") != 0 {
        return DRV_C;
    }
    if drv_eq(name, "--ubatch") != 0 {
        return DRV_UBATCH;
    }
    if drv_eq(name, "--n-expert-used") != 0 {
        return DRV_N_EXPERT_USED;
    }
    if drv_eq(name, "--gpu-layers") != 0 {
        return DRV_GPU_LAYERS;
    }
    if drv_eq(name, "--vram-cache-mb") != 0 {
        return DRV_VRAM_CACHE_MB;
    }
    if drv_eq(name, "--expert-hot-slots") != 0 {
        return DRV_EXPERT_HOT_SLOTS;
    }
    if drv_eq(name, "--temp") != 0 {
        return DRV_TEMP;
    }
    if drv_eq(name, "--top-k") != 0 {
        return DRV_TOP_K;
    }
    if drv_eq(name, "--top-p") != 0 {
        return DRV_TOP_P;
    }
    if drv_eq(name, "--seed") != 0 {
        return DRV_SEED;
    }
    if drv_eq(name, "--mtp") != 0 {
        return DRV_MTP;
    }
    if drv_eq(name, "--ngram") != 0 {
        return DRV_NGRAM;
    }
    if drv_eq(name, "--draft") != 0 {
        return DRV_DRAFT;
    }
    if drv_eq(name, "--mtp-p-min") != 0 {
        return DRV_MTP_P_MIN;
    }
    if drv_eq(name, "--ngram-min-match") != 0 {
        return DRV_NGRAM_MIN_MATCH;
    }
    if drv_eq(name, "--chatml") != 0 {
        return DRV_CHATML;
    }
    if drv_eq(name, "--no-think") != 0 {
        return DRV_NO_THINK;
    }
    if drv_eq(name, "--progress") != 0 {
        return DRV_PROGRESS;
    }
    if drv_eq(name, "--session") != 0 {
        return DRV_SESSION;
    }
    if drv_eq(name, "--csv") != 0 {
        return DRV_CSV;
    }
    if drv_eq(name, "--route-trace") != 0 {
        return DRV_ROUTE_TRACE;
    }
    if drv_eq(name, "--compute-trace") != 0 {
        return DRV_COMPUTE_TRACE;
    }
    if drv_eq(name, "--compute-trace-layers") != 0 {
        return DRV_COMPUTE_TRACE_LAYERS;
    }
    if drv_eq(name, "--io-trace") != 0 {
        return DRV_IO_TRACE;
    }
    if drv_eq(name, "--workload-capture") != 0 {
        return DRV_WORKLOAD_CAPTURE;
    }
    if drv_eq(name, "--workload-replay") != 0 {
        return DRV_WORKLOAD_REPLAY;
    }
    if drv_eq(name, "--moe-stream") != 0 {
        return DRV_MOE_STREAM;
    }
    if drv_eq(name, "--cache-mb") != 0 {
        return DRV_CACHE_MB;
    }
    if drv_eq(name, "--cache-floor-mb") != 0 {
        return DRV_CACHE_FLOOR_MB;
    }
    if drv_eq(name, "--cache-ceil-mb") != 0 {
        return DRV_CACHE_CEIL_MB;
    }
    if drv_eq(name, "--io-threads") != 0 {
        return DRV_IO_THREADS;
    }
    if drv_eq(name, "--no-odirect") != 0 {
        return DRV_NO_ODIRECT;
    }
    if drv_eq(name, "--row-stream") != 0 {
        return DRV_ROW_STREAM;
    }
    if drv_eq(name, "--row-stream-mb") != 0 {
        return DRV_ROW_STREAM_MB;
    }
    if drv_eq(name, "--dense-weights") != 0 {
        return DRV_DENSE_WEIGHTS;
    }
    if drv_eq(name, "--no-warm-dense") != 0 {
        return DRV_NO_WARM_DENSE;
    }
    if drv_eq(name, "--dense-odirect") != 0 {
        return DRV_DENSE_ODIRECT;
    }
    if drv_eq(name, "--load-all") != 0 {
        return DRV_LOAD_ALL;
    }
    if drv_eq(name, "--force-cache") != 0 {
        return DRV_FORCE_CACHE;
    }
    if drv_eq(name, "--overlap") != 0 {
        return DRV_OVERLAP;
    }
    if drv_eq(name, "--io-two-wave") != 0 {
        return DRV_IO_TWO_WAVE;
    }
    if drv_eq(name, "--prefetch") != 0 {
        return DRV_PREFETCH;
    }
    if drv_eq(name, "--prefetch-sync") != 0 {
        return DRV_PREFETCH_SYNC;
    }
    if drv_eq(name, "--drop-cold-experts") != 0 {
        return DRV_DROP_COLD_EXPERTS;
    }
    if drv_eq(name, "--expert-substitute") != 0 {
        return DRV_EXPERT_SUBSTITUTE;
    }
    if drv_eq(name, "--ppl") != 0 {
        return DRV_PPL;
    }
    if drv_eq(name, "--ppl-skip") != 0 {
        return DRV_PPL_SKIP;
    }
    if drv_eq(name, "--ppl-step") != 0 {
        return DRV_PPL_STEP;
    }
    if drv_eq(name, "--ppl-list") != 0 {
        return DRV_PPL_LIST;
    }
    if drv_eq(name, "--ppl-choices") != 0 {
        return DRV_PPL_CHOICES;
    }
    if drv_eq(name, "--drop-no-renorm") != 0 {
        return DRV_DROP_NO_RENORM;
    }
    if drv_eq(name, "--drop-in-prefill") != 0 {
        return DRV_DROP_IN_PREFILL;
    }
    if drv_eq(name, "--route-ahead") != 0 {
        return DRV_ROUTE_AHEAD;
    }
    if drv_eq(name, "--predict-log") != 0 {
        return DRV_PREDICT_LOG;
    }
    if drv_eq(name, "--predict-prefetch") != 0 {
        return DRV_PREDICT_PREFETCH;
    }
    if drv_eq(name, "--predict-spec-max") != 0 {
        return DRV_PREDICT_SPEC_MAX;
    }
    if drv_eq(name, "--list-archs") != 0 {
        return DRV_LIST_ARCHS;
    }
    if drv_eq(name, "-h") != 0 || drv_eq(name, "--help") != 0 {
        return DRV_HELP;
    }
    if drv_eq(name, "--version") != 0 {
        return DRV_VERSION;
    }
    return DRV_BAD;
}

export fn drv_flag_id(name: const u8[:]) -> i64 {
    return drv_flag_of(name);
}

fn drv_kind_of(id: i64) -> i64 {
    if id < 0 || id >= DRV_N_FLAGS {
        return DRV_BAD;
    }
    if id == DRV_M || id == DRV_P || id == DRV_CSV || id == DRV_ROUTE_TRACE ||
       id == DRV_COMPUTE_TRACE || id == DRV_IO_TRACE || id == DRV_WORKLOAD_CAPTURE ||
       id == DRV_WORKLOAD_REPLAY || id == DRV_PPL || id == DRV_PPL_LIST {
        return DRV_K_STRING;
    }
    if id == DRV_COMPUTE_TRACE_LAYERS {
        return DRV_K_STRING_TRUE;
    }
    if id == DRV_TEMP || id == DRV_TOP_P || id == DRV_MTP_P_MIN ||
       id == DRV_DROP_COLD_EXPERTS || id == DRV_EXPERT_SUBSTITUTE {
        return DRV_K_FLOAT;
    }
    if id == DRV_SEED {
        return DRV_K_U32;
    }
    if id == DRV_CHATML || id == DRV_PROGRESS || id == DRV_SESSION || id == DRV_MOE_STREAM ||
       id == DRV_ROW_STREAM || id == DRV_LOAD_ALL || id == DRV_FORCE_CACHE || id == DRV_OVERLAP ||
       id == DRV_IO_TWO_WAVE || id == DRV_PREFETCH_SYNC || id == DRV_PPL_STEP ||
       id == DRV_DROP_IN_PREFILL || id == DRV_PREDICT_LOG || id == DRV_PREDICT_PREFETCH {
        return DRV_K_TRUE;
    }
    if id == DRV_NO_THINK || id == DRV_NO_ODIRECT || id == DRV_DROP_NO_RENORM {
        return DRV_K_FALSE;
    }
    if id == DRV_CACHE_MB {
        return DRV_K_CACHE;
    }
    if id == DRV_DENSE_WEIGHTS {
        return DRV_K_DENSE;
    }
    if id == DRV_NO_WARM_DENSE {
        return DRV_K_DENSE_MMAP;
    }
    if id == DRV_DENSE_ODIRECT {
        return DRV_K_DENSE_ANON;
    }
    if id == DRV_PPL_CHOICES {
        return DRV_K_CSV;
    }
    if id == DRV_MTP {
        return DRV_K_DRAFT_MTP;
    }
    if id == DRV_NGRAM {
        return DRV_K_DRAFT_NGRAM;
    }
    if id == DRV_LIST_ARCHS || id == DRV_HELP || id == DRV_VERSION {
        return DRV_K_ACTION;
    }
    return DRV_K_INT;
}

export fn drv_kind(id: i64) -> i64 {
    return drv_kind_of(id);
}

// Whether the flag consumes the next argv token. Everything that is not a bare
// switch, an alias, a draft-source pick or an action takes a value; the native
// loop's `next()` then exits 1 when there is none left.
fn drv_span_of(id: i64) -> i64 {
    let k: i64 = drv_kind_of(id);
    if k == DRV_BAD {
        return DRV_BAD;
    }
    if k == DRV_K_TRUE || k == DRV_K_FALSE || k == DRV_K_ACTION ||
       k == DRV_K_DRAFT_MTP || k == DRV_K_DRAFT_NGRAM ||
       k == DRV_K_DENSE_MMAP || k == DRV_K_DENSE_ANON {
        return 0;
    }
    return 1;
}

export fn drv_span(id: i64) -> i64 {
    return drv_span_of(id);
}

fn drv_field_of(id: i64) -> i64 {
    if id < 0 || id >= DRV_N_FLAGS {
        return DRV_BAD;
    }
    if id == DRV_M {
        return DRV_F_MODEL;
    }
    if id == DRV_P {
        return DRV_F_PROMPT;
    }
    if id == DRV_N {
        return DRV_F_N_PREDICT;
    }
    if id == DRV_T {
        return DRV_F_N_THREADS;
    }
    if id == DRV_C {
        return DRV_F_N_CTX;
    }
    if id == DRV_UBATCH {
        return DRV_F_N_UBATCH;
    }
    if id == DRV_N_EXPERT_USED {
        return DRV_F_N_EXPERT_USED;
    }
    if id == DRV_GPU_LAYERS {
        return DRV_F_N_GPU_LAYERS;
    }
    if id == DRV_VRAM_CACHE_MB {
        return DRV_F_VRAM_CACHE_MB;
    }
    if id == DRV_EXPERT_HOT_SLOTS {
        return DRV_F_EXPERT_HOT_SLOTS;
    }
    if id == DRV_TEMP {
        return DRV_F_TEMP;
    }
    if id == DRV_TOP_K {
        return DRV_F_TOP_K;
    }
    if id == DRV_TOP_P {
        return DRV_F_TOP_P;
    }
    if id == DRV_SEED {
        return DRV_F_SEED;
    }
    if id == DRV_MTP || id == DRV_NGRAM {
        return DRV_F_SPEC_SOURCE;
    }
    if id == DRV_DRAFT {
        return DRV_F_DRAFT_MAX;
    }
    if id == DRV_MTP_P_MIN {
        return DRV_F_DRAFT_P_MIN;
    }
    if id == DRV_NGRAM_MIN_MATCH {
        return DRV_F_NGRAM_MIN_MATCH;
    }
    if id == DRV_CHATML {
        return DRV_F_CHATML;
    }
    if id == DRV_NO_THINK {
        return DRV_F_THINK;
    }
    if id == DRV_PROGRESS {
        return DRV_F_PROGRESS;
    }
    if id == DRV_SESSION {
        return DRV_F_SESSION;
    }
    if id == DRV_CSV {
        return DRV_F_CSV;
    }
    if id == DRV_ROUTE_TRACE {
        return DRV_F_ROUTE_TRACE;
    }
    if id == DRV_COMPUTE_TRACE || id == DRV_COMPUTE_TRACE_LAYERS {
        return DRV_F_COMPUTE_TRACE;
    }
    if id == DRV_IO_TRACE {
        return DRV_F_IO_TRACE;
    }
    if id == DRV_WORKLOAD_CAPTURE {
        return DRV_F_WORKLOAD_CAPTURE;
    }
    if id == DRV_WORKLOAD_REPLAY {
        return DRV_F_WORKLOAD_REPLAY;
    }
    if id == DRV_MOE_STREAM {
        return DRV_F_MOE_ENABLED;
    }
    if id == DRV_CACHE_MB {
        return DRV_F_CACHE_MB;
    }
    if id == DRV_CACHE_FLOOR_MB {
        return DRV_F_CACHE_FLOOR_MB;
    }
    if id == DRV_CACHE_CEIL_MB {
        return DRV_F_CACHE_CEIL_MB;
    }
    if id == DRV_IO_THREADS {
        return DRV_F_IO_THREADS;
    }
    if id == DRV_NO_ODIRECT {
        return DRV_F_O_DIRECT;
    }
    if id == DRV_ROW_STREAM {
        return DRV_F_ROW_STREAM;
    }
    if id == DRV_ROW_STREAM_MB {
        return DRV_F_ROW_STREAM_MB;
    }
    if id == DRV_DENSE_WEIGHTS || id == DRV_NO_WARM_DENSE || id == DRV_DENSE_ODIRECT {
        return DRV_F_DENSE_WEIGHTS;
    }
    if id == DRV_LOAD_ALL {
        return DRV_F_LOAD_ALL;
    }
    if id == DRV_FORCE_CACHE {
        return DRV_F_FORCE_CACHE;
    }
    if id == DRV_OVERLAP {
        return DRV_F_OVERLAP;
    }
    if id == DRV_IO_TWO_WAVE {
        return DRV_F_IO_TWO_WAVE;
    }
    if id == DRV_PREFETCH {
        return DRV_F_PREFETCH_LAYERS;
    }
    if id == DRV_PREFETCH_SYNC {
        return DRV_F_PREFETCH_SYNC;
    }
    if id == DRV_DROP_COLD_EXPERTS {
        return DRV_F_DROP_COLD_FRAC;
    }
    if id == DRV_EXPERT_SUBSTITUTE {
        return DRV_F_SUBSTITUTE_LAMBDA;
    }
    if id == DRV_PPL {
        return DRV_F_PPL;
    }
    if id == DRV_PPL_SKIP {
        return DRV_F_PPL_SKIP;
    }
    if id == DRV_PPL_STEP {
        return DRV_F_PPL_STEP;
    }
    if id == DRV_PPL_LIST {
        return DRV_F_PPL_LIST;
    }
    if id == DRV_PPL_CHOICES {
        return DRV_F_PPL_CHOICES;
    }
    if id == DRV_DROP_NO_RENORM {
        return DRV_F_DROP_RENORM;
    }
    if id == DRV_DROP_IN_PREFILL {
        return DRV_F_DROP_PREFILL;
    }
    if id == DRV_ROUTE_AHEAD {
        return DRV_F_ROUTE_AHEAD;
    }
    if id == DRV_PREDICT_LOG {
        return DRV_F_PREDICT_LOG;
    }
    if id == DRV_PREDICT_PREFETCH {
        return DRV_F_PREDICT_PREFETCH;
    }
    if id == DRV_PREDICT_SPEC_MAX {
        return DRV_F_PREDICT_SPEC_MAX;
    }
    return DRV_F_ACTION;
}

export fn drv_field(id: i64) -> i64 {
    return drv_field_of(id);
}

// The action a DRV_K_ACTION flag ends the run with, as an id the carrier maps to
// its own print and exit 0. -1 for anything else.
export fn drv_action(id: i64) -> i64 {
    if id == DRV_HELP {
        return DRV_ACT_HELP;
    }
    if id == DRV_VERSION {
        return DRV_ACT_VERSION;
    }
    if id == DRV_LIST_ARCHS {
        return DRV_ACT_LIST_ARCHS;
    }
    return DRV_BAD;
}

// The deprecated dense aliases carry their enum value in the flag, not in a
// value token. -1 when the flag needs a value (or is not a dense flag at all).
export fn drv_dense_alias(id: i64) -> i64 {
    if id == DRV_NO_WARM_DENSE {
        return 0; // Mmap
    }
    if id == DRV_DENSE_ODIRECT {
        return 2; // Anonymous
    }
    return DRV_BAD;
}

// ── refusals ───────────────────────────────────────────────────────────────
// The refusal code each refusal exits with, in one place. The native loop
// returns 2 only where the user asked for two contradictory things or named a
// value outside a closed vocabulary; a missing value and an unknown argument are
// 1, because the native prints usage there.
export fn drv_exit(kind: i64) -> i64 {
    if kind == 1 {
        return DRV_X_MISSING_VALUE;
    }
    if kind == 2 {
        return DRV_X_UNKNOWN_ARG;
    }
    if kind == 3 {
        return DRV_X_DRAFT_CONFLICT;
    }
    if kind == 4 {
        return DRV_X_DENSE_ENUM;
    }
    if kind == 5 {
        return DRV_X_WORKLOAD_CONFLICT;
    }
    if kind == 6 {
        return DRV_X_PPL_LIST_OPEN;
    }
    if kind == 7 {
        return DRV_X_PPL_FILE_OPEN;
    }
    if kind == 8 {
        return DRV_X_PPL_FAIL;
    }
    if kind == 9 {
        return DRV_X_USAGE;
    }
    return DRV_X_OK;
}

// main.cpp:659-668 — --mtp and --ngram are two sources for one verify loop, so
// asking for the other one after a source is already armed is a contradiction
// (exit 2), not a precedence question. cc_draft_pick owns the resulting source;
// this is the refusal it reports as -1.
export fn drv_draft_conflict(cur: i64, want: i64) -> i64 {
    if cur != 0 && cur != want {
        return 1;
    }
    return 0;
}

// main.cpp:876-883 — capture/replay apply to one-shot generation only, so they
// cannot be combined with a mode that never builds a GenerateRequest.
export fn drv_workload_conflict(session: i64, has_ppl: i64, has_list: i64,
                                has_capture: i64, has_replay: i64) -> i64 {
    if has_capture == 0 && has_replay == 0 {
        return 0;
    }
    if session != 0 || has_ppl != 0 || has_list != 0 {
        return 1;
    }
    return 0;
}

// main.cpp:880-885 — session first, then perplexity, then one-shot. Session mode
// wins even when a --ppl was typed, exactly like the native early return.
export fn drv_mode(session: i64, has_ppl: i64, has_list: i64) -> i64 {
    if session != 0 {
        return 1;
    }
    if has_ppl != 0 || has_list != 0 {
        return 2;
    }
    return 0;
}

// ── the --ppl command path ─────────────────────────────────────────────────
// main.cpp:891-899 — the list reader's per-line filter: trailing CR and spaces
// are trimmed, then an empty line or a '#' comment is dropped. Returns the
// trimmed length, or -1 to skip the line. A line of only spaces trims to 0 and
// is skipped; the '#' test is on the first character, which trimming never
// touches. The trim is a loop, not a single character: " \r \r" is one line.
export fn drv_list_keep(line: const u8[:]) -> i64 {
    var n: i64 = line.len;
    while n > 0 {
        let c: i64 = line[n - 1];
        if c == 13 || c == 32 {
            n = n - 1;
        } else {
            break;
        }
    }
    if n == 0 {
        return -1;
    }
    if line[0] == 35 {
        return -1;
    }
    return n;
}

// main.cpp:886-890 — one --ppl FILE, then every non-comment line of --ppl-list,
// in that order.
export fn drv_ppl_path_count(has_path: i64, list_lines: i64) -> i64 {
    var n: i64 = list_lines;
    if has_path != 0 {
        n = n + 1;
    }
    return n;
}

// The --ppl-choices split: `while (start <= cs.size())` with a comma search, so
// the field count is the comma count plus one and a trailing comma leaves an
// empty last choice (which tokenizes to nothing and fails its own gate, but the
// list is what the carrier builds).
export fn drv_csv_count(commas: i64) -> i64 {
    if commas < 0 {
        return 1;
    }
    return commas + 1;
}

// The index of the next comma at or after pos, or -1 when the rest of the string
// is the last field. The native loop calls find(',', start) and stops when the
// search fails, so the last field is the tail.
export fn drv_csv_next(s: const u8[:], pos: i64) -> i64 {
    var i: i64 = pos;
    while i < s.len {
        if s[i] == 44 {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

// main.cpp:924-927 — `pres.n_scored > 0 ? 100.0 * pres.n_top1 / pres.n_scored : 0.0`.
// The zero-scored case prints 0.0 rather than a division by zero, and the
// percentage is of the SCORED tokens, not of the generated ones.
export fn drv_ppl_pct(n_top1: i64, n_scored: i64) -> f64 {
    if n_scored > 0 {
        let num: f64 = n_top1;
        let den: f64 = n_scored;
        return 100.0 * num / den;
    }
    return 0.0;
}

// main.cpp:923 — the per-file label prints only when more than one text is being
// scored in the session; a single text needs no name.
export fn drv_ppl_label(n_paths: i64) -> i64 {
    if n_paths > 1 {
        return 1;
    }
    return 0;
}
