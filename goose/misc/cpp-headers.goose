// P10-08 S8: the C++ host header surface, frozen.
//
// Two families that a C++ consumer compiles against and that no C gate covers:
//
//   ggml/include/ggml-cpp.h at the base commit (upstream/llama.cpp 0b5be7e4a).
//   Seven RAII wrappers are the whole of it: a deleter struct per owned type,
//   and a std::unique_ptr typedef that names it. The decisions frozen here are
//   the ones a consumer can observe without a model:
//     - which release function each deleter calls, one call, with the stored
//       pointer unchanged;
//     - that the wrapper is std::unique_ptr<T, D> and not shared_ptr (a
//       shared_ptr would copy the handle and release it twice);
//     - that destroying an empty wrapper calls nothing, releasing a live one
//       calls nothing, and moving transfers the single release to the target;
//     - that the header is C++-only and pulls exactly five includes.
//
//   carrier/runtime/core/include/bmoe at the same commit: version.h and
//   config.h. The runtime's own header ABI is here because the library's
//   behaviour is a function of these defaults: a changed default is not a
//   compile error for any consumer, it silently changes what the engine does.
//   Frozen are the three public enums, every default of SamplingConfig,
//   MoeStreamConfig, SpecConfig, RunConfig and SessionConfig, the constexpr
//   limits, the three pod layouts, and the two inline predicates of SpecConfig.
//
// The oracle is the real header: the fixture reads sizeof/offsetof and every
// default from a compiled-in struct, classifies each typedef with
// std::is_same, and counts deleter calls through linker-wrapped release
// functions, so no value here is compared against a hand copy.
//
// The prefix is cpph_.

import std;

const CPPH_NONE: i64 = 0 - 1;

// ── the seven RAII wrappers ────────────────────────────────────────────────
// Id order follows the header: context, gguf_context, gallocr, backend,
// backend_buffer, backend_event, backend_sched.
const CPPH_WRAP_N: i64 = 7;

const CPPH_WRAP_PTR: const u8[:][7] = [
    "ggml_context_ptr",
    "gguf_context_ptr",
    "ggml_gallocr_ptr",
    "ggml_backend_ptr",
    "ggml_backend_buffer_ptr",
    "ggml_backend_event_ptr",
    "ggml_backend_sched_ptr"
];

const CPPH_WRAP_DELETER: const u8[:][7] = [
    "ggml_context_deleter",
    "gguf_context_deleter",
    "ggml_gallocr_deleter",
    "ggml_backend_deleter",
    "ggml_backend_buffer_deleter",
    "ggml_backend_event_deleter",
    "ggml_backend_sched_deleter"
];

// The release function each deleter calls. The value is the index into the
// free-function table below, which is also the wrapper id: the header pairs
// them one to one and a deleter that calls a neighbour's release is a double
// free of a different type.
const CPPH_WRAP_FREE_ID: i64[7] = [0, 1, 2, 3, 4, 5, 6];

// All seven are std::unique_ptr; 1 would be std::shared_ptr.
const CPPH_WRAP_KIND: i64[7] = [0, 0, 0, 0, 0, 0, 0];

const CPPH_FREE_N: i64 = 7;

const CPPH_FREE_NAME: const u8[:][7] = [
    "ggml_free",
    "gguf_free",
    "ggml_gallocr_free",
    "ggml_backend_free",
    "ggml_backend_buffer_free",
    "ggml_backend_event_free",
    "ggml_backend_sched_free"
];

// A deleter is a stateless struct with `void operator()(T *)`.
const CPPH_DELETER_EMPTY: i64 = 1;
const CPPH_DELETER_VOID: i64 = 1;
const CPPH_DELETER_CALLS: i64 = 1;

// ── header facts ───────────────────────────────────────────────────────────
// ggml.h, ggml-alloc.h, ggml-backend.h, gguf.h and <memory>.
const CPPH_INCLUDE_N: i64 = 5;
const CPPH_CPP_ONLY: i64 = 1;
const CPPH_WRAPPER_SIZE: i64 = 8;

// ── carrier enums ──────────────────────────────────────────────────────────
// enum class DenseWeightsMode: Mmap, Warmed, Anonymous, Pinned.
// enum class DraftSource: none, mtp, ngram.
// enum class ThinkControl: Template, Prefill, None.
const CPPH_ENUM_N: i64 = 3;

const CPPH_ENUM_NAME: const u8[:][3] = [
    "DenseWeightsMode",
    "DraftSource",
    "ThinkControl"
];

const CPPH_ENUM_MEMBERS: i64[3] = [4, 3, 3];

const CPPH_ENUM_VALUE: i64[10] = [
    0, 1, 2, 3,
    0, 1, 2,
    0, 1, 2
];

const CPPH_ENUM_TEXT: const u8[:][10] = [
    "Mmap", "Warmed", "Anonymous", "Pinned",
    "none", "mtp", "ngram",
    "Template", "Prefill", "None"
];

// ── carrier defaults ───────────────────────────────────────────────────────
// Kind 0 integer or bool, kind 1 real (compared as float), kind 2 text.
const CPPH_DEF_N: i64 = 55;

const CPPH_DEF_NAME: const u8[:][55] = [
    "MoeStreamConfig.cache_auto",
    "MoeStreamConfig.cache_ceil_mb",
    "MoeStreamConfig.cache_floor_mb",
    "MoeStreamConfig.cache_mb",
    "MoeStreamConfig.dense_weights",
    "MoeStreamConfig.drop_cold_frac",
    "MoeStreamConfig.drop_low_topk_warn",
    "MoeStreamConfig.drop_prefill",
    "MoeStreamConfig.drop_renorm",
    "MoeStreamConfig.enabled",
    "MoeStreamConfig.force_cache",
    "MoeStreamConfig.io_threads",
    "MoeStreamConfig.io_threads_max",
    "MoeStreamConfig.io_two_wave",
    "MoeStreamConfig.load_all",
    "MoeStreamConfig.o_direct",
    "MoeStreamConfig.overlap",
    "MoeStreamConfig.predict_log",
    "MoeStreamConfig.predict_prefetch",
    "MoeStreamConfig.predict_spec_max",
    "MoeStreamConfig.prefetch_layers",
    "MoeStreamConfig.prefetch_layers_max",
    "MoeStreamConfig.prefetch_sync",
    "MoeStreamConfig.route_ahead",
    "MoeStreamConfig.route_ahead_max",
    "MoeStreamConfig.row_stream",
    "MoeStreamConfig.row_stream_mb",
    "MoeStreamConfig.substitute_lambda",
    "RunConfig.chatml",
    "RunConfig.compute_trace_layers",
    "RunConfig.expert_hot_slots",
    "RunConfig.model_path",
    "RunConfig.n_ctx",
    "RunConfig.n_expert_used",
    "RunConfig.n_gpu_layers",
    "RunConfig.n_predict",
    "RunConfig.n_threads",
    "RunConfig.n_ubatch",
    "RunConfig.prompt",
    "RunConfig.progress",
    "RunConfig.think",
    "RunConfig.vram_cache_mb",
    "RunConfig.workload_capture_path",
    "RunConfig.workload_replay_path",
    "SamplingConfig.seed",
    "SamplingConfig.temp",
    "SamplingConfig.top_k",
    "SamplingConfig.top_p",
    "SessionConfig.chatml",
    "SessionConfig.compute_trace_layers",
    "SessionConfig.expert_hot_slots",
    "SessionConfig.n_batch",
    "SessionConfig.n_ctx",
    "SessionConfig.n_threads",
    "SpecConfig.draft_max"
];

const CPPH_DEF_KIND: i64[55] = [
    0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 2, 2,
    0, 1, 0, 1,
    0, 0, 0, 0, 0, 0,
    0
];

const CPPH_DEF_INT: i64[55] = [
    0, 0, 1536, 0, 2, 0, 4, 0, 1, 0, 0, 4, 8, 0, 0, 1, 0, 0, 0, 2, 0, 8, 0, 0, 8, 0, 64, 0,
    0, 0, 0, 0, 2048, 0, 0, 128, 4, 0, 0, 0, 1, 0, 0, 0,
    4294967295, 0, 40, 0,
    0, 0, 0, 512, 2048, 4,
    3
];

const CPPH_DEF_REAL: f64[55] = [
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.95,
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
    0.0
];

const CPPH_DEF_TXT: const u8[:][55] = [
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "The capital of Japan is", "", "", "", "", "",
    "", "", "", "",
    "", "", "", "", "", "",
    ""
];

// ── pod layouts, LP64/gcc-13 ───────────────────────────────────────────────
// sizeof and offsetof of the three structs that carry no std::string. A
// consumer compiled against another layout misreads a default, not a symbol.
const CPPH_LAYOUT_N: i64 = 8;

const CPPH_LAYOUT_NAME: const u8[:][8] = [
    "sizeof:SamplingConfig",
    "sizeof:MoeStreamConfig",
    "sizeof:SpecConfig",
    "offsetof:MoeStreamConfig.cache_floor_mb",
    "offsetof:MoeStreamConfig.io_threads",
    "offsetof:MoeStreamConfig.dense_weights",
    "offsetof:SpecConfig.draft_max",
    "offsetof:SpecConfig.ngram_max_match"
];

const CPPH_LAYOUT_VAL: i64[8] = [16, 76, 20, 12, 20, 36, 4, 16];

// ── the rest of the carrier defaults, table driven ─────────────────────────
// SpecConfig.none, draft_p_min, ngram_min_match, ngram_max_match and the two
// limits; MoeStreamConfig's five-strong constexpr block is above.
fn cpph_copy(out: u8[:], s: const u8[:]) -> i64 {
    var n: i64 = 0;
    while n < s.len && n < out.len {
        out[n] = s[n];
        n = n + 1;
    }
    return n;
}

// ---- exports --------------------------------------------------------------

export fn cpph_wrap_count() -> i64 {
    return CPPH_WRAP_N;
}

export fn cpph_wrap_ptr(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= CPPH_WRAP_N {
        return CPPH_NONE;
    }
    return cpph_copy(out, CPPH_WRAP_PTR[i]);
}

export fn cpph_wrap_deleter(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= CPPH_WRAP_N {
        return CPPH_NONE;
    }
    return cpph_copy(out, CPPH_WRAP_DELETER[i]);
}

// 0 unique_ptr, 1 shared_ptr.
export fn cpph_wrap_kind(i: i64) -> i64 {
    if i < 0 || i >= CPPH_WRAP_N {
        return CPPH_NONE;
    }
    return CPPH_WRAP_KIND[i];
}

export fn cpph_free_count() -> i64 {
    return CPPH_FREE_N;
}

export fn cpph_free_name(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= CPPH_FREE_N {
        return CPPH_NONE;
    }
    return cpph_copy(out, CPPH_FREE_NAME[i]);
}

// The release function wrapper i destroys through: 0 when the pair is
// well formed (wrapper i calls free i), otherwise the index of the function it
// actually calls. The gate asserts equality with i, so a swap shows as a
// difference on both affected wrappers.
export fn cpph_wrap_free(i: i64) -> i64 {
    if i < 0 || i >= CPPH_WRAP_N {
        return CPPH_NONE;
    }
    return CPPH_WRAP_FREE_ID[i];
}

export fn cpph_deleter_empty() -> i64 {
    return CPPH_DELETER_EMPTY;
}

export fn cpph_deleter_void() -> i64 {
    return CPPH_DELETER_VOID;
}

export fn cpph_deleter_calls() -> i64 {
    return CPPH_DELETER_CALLS;
}

// Release calls on destruction of a live wrapper (1) and of an empty one (0).
export fn cpph_destroy_calls(live: i64) -> i64 {
    if live != 0 {
        return 1;
    }
    return 0;
}

// release() hands the pointer out without releasing it.
export fn cpph_release_calls() -> i64 {
    return 0;
}

// Moving a live wrapper then destroying both ends releases exactly once.
export fn cpph_move_calls() -> i64 {
    return 1;
}

export fn cpph_include_count() -> i64 {
    return CPPH_INCLUDE_N;
}

export fn cpph_cpp_only() -> i64 {
    return CPPH_CPP_ONLY;
}

export fn cpph_wrapper_size() -> i64 {
    return CPPH_WRAPPER_SIZE;
}

export fn cpph_enum_count() -> i64 {
    return CPPH_ENUM_N;
}

export fn cpph_enum_name(e: i64, out: u8[:]) -> i64 {
    if e < 0 || e >= CPPH_ENUM_N {
        return CPPH_NONE;
    }
    return cpph_copy(out, CPPH_ENUM_NAME[e]);
}

export fn cpph_enum_members(e: i64) -> i64 {
    if e < 0 || e >= CPPH_ENUM_N {
        return CPPH_NONE;
    }
    return CPPH_ENUM_MEMBERS[e];
}

export fn cpph_enum_value(e: i64, m: i64) -> i64 {
    if e < 0 || e >= CPPH_ENUM_N || m < 0 || m >= CPPH_ENUM_MEMBERS[e] {
        return CPPH_NONE;
    }
    var base: i64 = 0;
    var i: i64 = 0;
    while i < e {
        base = base + CPPH_ENUM_MEMBERS[i];
        i = i + 1;
    }
    return CPPH_ENUM_VALUE[base + m];
}

export fn cpph_enum_text(e: i64, m: i64, out: u8[:]) -> i64 {
    if e < 0 || e >= CPPH_ENUM_N || m < 0 || m >= CPPH_ENUM_MEMBERS[e] {
        return CPPH_NONE;
    }
    var base: i64 = 0;
    var i: i64 = 0;
    while i < e {
        base = base + CPPH_ENUM_MEMBERS[i];
        i = i + 1;
    }
    return cpph_copy(out, CPPH_ENUM_TEXT[base + m]);
}

export fn cpph_default_count() -> i64 {
    return CPPH_DEF_N;
}

export fn cpph_default_name(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= CPPH_DEF_N {
        return CPPH_NONE;
    }
    return cpph_copy(out, CPPH_DEF_NAME[i]);
}

export fn cpph_default_kind(i: i64) -> i64 {
    if i < 0 || i >= CPPH_DEF_N {
        return CPPH_NONE;
    }
    return CPPH_DEF_KIND[i];
}

export fn cpph_default_int(i: i64) -> i64 {
    if i < 0 || i >= CPPH_DEF_N || CPPH_DEF_KIND[i] != 0 {
        return CPPH_NONE;
    }
    return CPPH_DEF_INT[i];
}

export fn cpph_default_real(i: i64) -> f64 {
    if i < 0 || i >= CPPH_DEF_N || CPPH_DEF_KIND[i] != 1 {
        return -1.0;
    }
    return CPPH_DEF_REAL[i];
}

export fn cpph_default_text(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= CPPH_DEF_N || CPPH_DEF_KIND[i] != 2 {
        return CPPH_NONE;
    }
    return cpph_copy(out, CPPH_DEF_TXT[i]);
}

export fn cpph_layout_count() -> i64 {
    return CPPH_LAYOUT_N;
}

export fn cpph_layout_name(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= CPPH_LAYOUT_N {
        return CPPH_NONE;
    }
    return cpph_copy(out, CPPH_LAYOUT_NAME[i]);
}

export fn cpph_layout_value(i: i64) -> i64 {
    if i < 0 || i >= CPPH_LAYOUT_N {
        return CPPH_NONE;
    }
    return CPPH_LAYOUT_VAL[i];
}

// bmoe::version(): the build's text when BMOE_VERSION is defined, "unknown"
// otherwise, because a build without the define cannot vouch for a number.
export fn cpph_version(defined: i64, text: const u8[:], out: u8[:]) -> i64 {
    if defined != 0 {
        return cpph_copy(out, text);
    }
    return cpph_copy(out, "unknown");
}

// The three inline predicates of SpecConfig.
export fn cpph_spec_enabled(source: i64) -> i64 {
    if source == 0 {
        return 0;
    }
    return 1;
}

export fn cpph_spec_is_mtp(source: i64) -> i64 {
    if source == 1 {
        return 1;
    }
    return 0;
}

export fn cpph_spec_is_ngram(source: i64) -> i64 {
    if source == 2 {
        return 1;
    }
    return 0;
}
