// RunConfig <-> cfg_validate bridge (FG_GOOSE_CONFIG=ON only).
//
// The module owns the decision chain; this file owns the two things a flat
// export cannot express: reading the native struct into the documented layout
// and calling the one entry point.
//
// Layout contract (goose/cli/config-validate.goose, "Flat layout" comment):
//   iv[0..7]   n_predict, n_threads, n_ctx, n_ubatch, n_expert_used,
//              vram_cache_mb, expert_hot_slots, n_gpu_layers
//   iv[8..10]  model_path / workload_capture_path / workload_replay_path present
//   iv[11..15] sampling.top_k, spec.source, spec.draft_max,
//              spec.ngram_min_match, spec.ngram_max_match
//   iv[16]     moe.enabled
//   iv[17..31] the rest of MoeStreamConfig in header order
//   fv[0..4]   sampling.temp, sampling.top_p, spec.draft_p_min,
//              moe.drop_cold_frac, moe.substitute_lambda

#include "fg-goose-config-bridge.h"
#include "goose-host.h" // fg_goose_host_ensure_init

#include <mutex>
#include <type_traits>

#include "config-slice.h" // generated: cfg_validate, goose_init, sl_i64/sl_f64

namespace bmoe {
namespace {

// The native comparisons are single-precision (float fields compared against
// float literals). A double in the flat layout would round differently at the
// [0, 1] boundaries, so the bridge refuses to compile if a field it forwards
// stops being a float: the cast below is then a real narrowing, not a no-op.
template <typename T>
constexpr bool is_float32() {
    return std::is_same<typename std::decay<T>::type, float>::value;
}
static_assert(is_float32<decltype(std::declval<RunConfig>().sampling.temp)>(), "sampling.temp must stay float");
static_assert(is_float32<decltype(std::declval<RunConfig>().sampling.top_p)>(), "sampling.top_p must stay float");
static_assert(is_float32<decltype(std::declval<RunConfig>().spec.draft_p_min)>(), "spec.draft_p_min must stay float");
static_assert(is_float32<decltype(std::declval<RunConfig>().moe.drop_cold_frac)>(), "moe.drop_cold_frac must stay float");
static_assert(is_float32<decltype(std::declval<RunConfig>().moe.substitute_lambda)>(), "moe.substitute_lambda must stay float");

// Narrow to single precision, then widen: exactly what the native code compares.
inline double narrow(float v) { return static_cast<double>(v); }

inline int64_t flag(bool v) { return v ? 1 : 0; }

// Explicit, name-based mapping of DraftSource to the module's CFG_SRC_* codes.
// A static_cast would silently follow the enum's order; this does not.
inline int64_t source_code(DraftSource s) {
    switch (s) {
    case DraftSource::none: return 0;  // CFG_SRC_NONE
    case DraftSource::mtp: return 1;   // CFG_SRC_MTP
    case DraftSource::ngram: return 2; // CFG_SRC_NGRAM
    }
    return -1; // unknown enumerator: the module rejects the layout (short/out of range)
}

long long g_goose_calls = 0;

void ensure_init() {
    // M1: one start-up for the whole runtime. The per-stage guard of the
    // accepted card would be a tenth pthread_once in this link.
    fg_goose_host_ensure_init();
}

} // namespace

void fg_config_to_flat(const RunConfig & cfg, int64_t iv[32], double fv[5]) {
    // Integers. This mirror of the native struct is the whole seam; a field
    // added to RunConfig needs a line here and an index in the module.
    iv[0] = cfg.n_predict;
    iv[1] = cfg.n_threads;
    iv[2] = cfg.n_ctx;
    iv[3] = cfg.n_ubatch;
    iv[4] = cfg.n_expert_used;
    iv[5] = cfg.vram_cache_mb;
    iv[6] = cfg.expert_hot_slots;
    iv[7] = cfg.n_gpu_layers;
    iv[8] = flag(!cfg.model_path.empty());
    iv[9] = flag(!cfg.workload_capture_path.empty());
    iv[10] = flag(!cfg.workload_replay_path.empty());
    iv[11] = cfg.sampling.top_k;
    iv[12] = source_code(cfg.spec.source);
    iv[13] = cfg.spec.draft_max;
    iv[14] = cfg.spec.ngram_min_match;
    iv[15] = cfg.spec.ngram_max_match;
    iv[16] = flag(cfg.moe.enabled);
    iv[17] = cfg.moe.io_threads;
    iv[18] = cfg.moe.cache_mb;
    iv[19] = flag(cfg.moe.cache_auto);
    iv[20] = cfg.moe.cache_floor_mb;
    iv[21] = cfg.moe.cache_ceil_mb;
    iv[22] = cfg.moe.prefetch_layers;
    iv[23] = flag(cfg.moe.io_two_wave);
    iv[24] = flag(cfg.moe.predict_log);
    iv[25] = flag(cfg.moe.predict_prefetch);
    iv[26] = cfg.moe.predict_spec_max;
    iv[27] = cfg.moe.route_ahead;
    iv[28] = flag(cfg.moe.row_stream);
    iv[29] = cfg.moe.row_stream_mb;
    iv[30] = flag(cfg.moe.overlap);
    iv[31] = flag(cfg.moe.force_cache);

    // Floats: float -> double, never double -> double.
    fv[0] = narrow(cfg.sampling.temp);
    fv[1] = narrow(cfg.sampling.top_p);
    fv[2] = narrow(cfg.spec.draft_p_min);
    fv[3] = narrow(cfg.moe.drop_cold_frac);
    fv[4] = narrow(cfg.moe.substitute_lambda);
}

long long fg_config_validate_flat(const int64_t iv[32], const double fv[5]) {
    ensure_init();
    ++g_goose_calls;
    // The module takes mutable slices; it does not write through them.
    sl_i64 ci{const_cast<int64_t *>(iv), 32};
    sl_f64 cf{const_cast<double *>(fv), 5};
    return cfg_validate(ci, cf);
}

long long fg_config_goose_calls() { return g_goose_calls; }

} // namespace bmoe
