// Differential gate for P10-08 S8: ggml-cpp.h and the carrier headers.
//
// Native arms:
//   - the seven typedefs are classified against the real header with
//     std::is_same, their size and deleter emptiness with <type_traits>;
//   - every deleter is exercised for real: the fixture defines the seven
//     release functions as linker-wrapped counters, so "which function, how
//     many times, with which pointer" is measured, not assumed;
//   - every carrier default and pod layout is read from a compiled-in struct
//     at run time;
//   - bmoe::version() is compiled twice, with and without BMOE_VERSION, in
//     tests/version_probe.cpp.
//
// Port arm: the cpph_ exports of source/cpp-headers.goose.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "gguf.h"
#include "ggml-cpp.h"

#include "bmoe/config.h"
#include "bmoe/session.h"
#include "bmoe/version.h"

#include "cpph_gen.h"

static int cases = 0;
static int values = 0;
static int fails = 0;

static void eq_int(const char * what, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        std::printf("FAIL %-58s want=%lld got=%lld\n", what, (long long) want, (long long) got);
    }
}

static void eq_real(const char * what, float want, double got) {
    values++;
    if (want != static_cast<float>(got)) {
        fails++;
        std::printf("FAIL %-58s want=%g got=%g\n", what, (double) want, got);
    }
}

static void eq_str(const char * what, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        std::printf("FAIL %-58s want=[%s] got=[%s]\n", what, want.c_str(), got.c_str());
    }
}

static int64_t call_str(int64_t (*fn)(int64_t, sl_u8), int64_t i, uint8_t * buf, int64_t cap) {
    const sl_u8 s = { buf, cap };
    return fn(i, s);
}

static std::string port_str(int64_t (*fn)(int64_t, sl_u8), int64_t i) {
    uint8_t buf[512];
    std::memset(buf, 0xAA, sizeof buf);
    const int64_t n = call_str(fn, i, buf, (int64_t) sizeof buf);
    if (n < 0) {
        return "<refused>";
    }
    return std::string(reinterpret_cast<char *>(buf), static_cast<size_t>(n));
}

static std::string port_enum_text(int64_t e, int64_t m) {
    uint8_t buf[128];
    std::memset(buf, 0xAA, sizeof buf);
    const sl_u8 s = { buf, (int64_t) sizeof buf };
    const int64_t n = cpph_enum_text(e, m, s);
    if (n < 0) {
        return "<refused>";
    }
    return std::string(reinterpret_cast<char *>(buf), static_cast<size_t>(n));
}

static std::string port_ver(int64_t defined) {
    uint8_t buf[128];
    std::memset(buf, 0xAA, sizeof buf);
    const sl_u8 s = { buf, (int64_t) sizeof buf };
    const char * text = "9.9.9-test";
    const int64_t n = cpph_version(defined, sl_u8{ const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(text)),
                                                    (int64_t) std::strlen(text) }, s);
    if (n < 0) {
        return "<refused>";
    }
    return std::string(reinterpret_cast<char *>(buf), static_cast<size_t>(n));
}

// ── release-function counters ───────────────────────────────────────────────
// The linker renames every reference to these seven to __wrap_<name>, so the
// real library is not needed and is not called: only the choice of function,
// its call count and the pointer it received are under test.
static int g_calls[7];
static const void * g_last[7];

extern "C" void __wrap_ggml_free(struct ggml_context * p) { g_calls[0]++; g_last[0] = p; }
extern "C" void __wrap_gguf_free(struct gguf_context * p) { g_calls[1]++; g_last[1] = p; }
extern "C" void __wrap_ggml_gallocr_free(ggml_gallocr_t p) { g_calls[2]++; g_last[2] = p; }
extern "C" void __wrap_ggml_backend_free(ggml_backend_t p) { g_calls[3]++; g_last[3] = p; }
extern "C" void __wrap_ggml_backend_buffer_free(ggml_backend_buffer_t p) { g_calls[4]++; g_last[4] = p; }
extern "C" void __wrap_ggml_backend_event_free(ggml_backend_event_t p) { g_calls[5]++; g_last[5] = p; }
extern "C" void __wrap_ggml_backend_sched_free(ggml_backend_sched_t p) { g_calls[6]++; g_last[6] = p; }

static void reset_counts() {
    std::memset(g_calls, 0, sizeof g_calls);
    std::memset((void *) g_last, 0, sizeof g_last);
}

static int total_calls() {
    int n = 0;
    for (int c : g_calls) {
        n += c;
    }
    return n;
}

// ── the seven typedefs, checked against the real header ─────────────────────
static_assert(std::is_same<ggml_context_ptr, std::unique_ptr<ggml_context, ggml_context_deleter>>::value, "context ptr");
static_assert(std::is_same<gguf_context_ptr, std::unique_ptr<gguf_context, gguf_context_deleter>>::value, "gguf ptr");
static_assert(std::is_same<ggml_gallocr_ptr, std::unique_ptr<ggml_gallocr, ggml_gallocr_deleter>>::value, "gallocr ptr");
static_assert(std::is_same<ggml_backend_ptr, std::unique_ptr<ggml_backend, ggml_backend_deleter>>::value, "backend ptr");
static_assert(std::is_same<ggml_backend_buffer_ptr, std::unique_ptr<ggml_backend_buffer, ggml_backend_buffer_deleter>>::value, "buffer ptr");
static_assert(std::is_same<ggml_backend_event_ptr, std::unique_ptr<ggml_backend_event, ggml_backend_event_deleter>>::value, "event ptr");
static_assert(std::is_same<ggml_backend_sched_ptr, std::unique_ptr<ggml_backend_sched, ggml_backend_sched_deleter>>::value, "sched ptr");

static const char * const NATIVE_PTR[7] = {
    "ggml_context_ptr", "gguf_context_ptr", "ggml_gallocr_ptr", "ggml_backend_ptr",
    "ggml_backend_buffer_ptr", "ggml_backend_event_ptr", "ggml_backend_sched_ptr",
};
static const char * const NATIVE_DEL[7] = {
    "ggml_context_deleter", "gguf_context_deleter", "ggml_gallocr_deleter", "ggml_backend_deleter",
    "ggml_backend_buffer_deleter", "ggml_backend_event_deleter", "ggml_backend_sched_deleter",
};
static const char * const NATIVE_FREE[7] = {
    "ggml_free", "gguf_free", "ggml_gallocr_free", "ggml_backend_free",
    "ggml_backend_buffer_free", "ggml_backend_event_free", "ggml_backend_sched_free",
};

static const bool NATIVE_EMPTY[7] = {
    std::is_empty<ggml_context_deleter>::value,
    std::is_empty<gguf_context_deleter>::value,
    std::is_empty<ggml_gallocr_deleter>::value,
    std::is_empty<ggml_backend_deleter>::value,
    std::is_empty<ggml_backend_buffer_deleter>::value,
    std::is_empty<ggml_backend_event_deleter>::value,
    std::is_empty<ggml_backend_sched_deleter>::value,
};

static const bool NATIVE_VOID[7] = {
    std::is_same<decltype(std::declval<ggml_context_deleter>()(std::declval<ggml_context *>())), void>::value,
    std::is_same<decltype(std::declval<gguf_context_deleter>()(std::declval<gguf_context *>())), void>::value,
    std::is_same<decltype(std::declval<ggml_gallocr_deleter>()(std::declval<ggml_gallocr_t>())), void>::value,
    std::is_same<decltype(std::declval<ggml_backend_deleter>()(std::declval<ggml_backend_t>())), void>::value,
    std::is_same<decltype(std::declval<ggml_backend_buffer_deleter>()(std::declval<ggml_backend_buffer_t>())), void>::value,
    std::is_same<decltype(std::declval<ggml_backend_event_deleter>()(std::declval<ggml_backend_event_t>())), void>::value,
    std::is_same<decltype(std::declval<ggml_backend_sched_deleter>()(std::declval<ggml_backend_sched_t>())), void>::value,
};

// One typed exercise per wrapper: construct, observe no call, release, reset,
// destroy, and check which counter moved.
template <typename Ptr, typename T>
static void exercise(int id, T * sentinel) {
    reset_counts();
    {
        Ptr p(sentinel);
        values++;
        if (p.get() != sentinel || total_calls() != 0) {
            fails++;
            std::printf("FAIL wrapper %d releases at construction or loses the pointer\n", id);
        }
        values++;
        if ((int64_t) p.get() != (int64_t) sentinel) {
            fails++;
            std::printf("FAIL wrapper %d get() differs\n", id);
        }
    }
    char tag[80];
    std::snprintf(tag, sizeof tag, "wrapper %d destroy live", id);
    eq_int(tag, 1, g_calls[id]);
    eq_int("no neighbour releaser", 0, total_calls() - g_calls[id]);
    values++;
    if (g_last[id] != (const void *) sentinel) {
        fails++;
        std::printf("FAIL wrapper %d released a different pointer\n", id);
    }

    reset_counts();
    {
        Ptr empty;
        values++;
        if (empty.get() != nullptr) {
            fails++;
            std::printf("FAIL wrapper %d default is not empty\n", id);
        }
    }
    std::snprintf(tag, sizeof tag, "wrapper %d destroy empty", id);
    eq_int(tag, 0, total_calls());

    reset_counts();
    {
        Ptr p(sentinel);
        T * raw = p.release();
        values++;
        if (raw != sentinel) {
            fails++;
            std::printf("FAIL wrapper %d release() returned another pointer\n", id);
        }
    }
    std::snprintf(tag, sizeof tag, "wrapper %d release then destroy", id);
    eq_int(tag, 0, total_calls());

    reset_counts();
    {
        Ptr a(sentinel);
        Ptr b(std::move(a));
        values++;
        if (b.get() != sentinel || a.get() != nullptr) {
            fails++;
            std::printf("FAIL wrapper %d move did not transfer the handle\n", id);
        }
    }
    std::snprintf(tag, sizeof tag, "wrapper %d move then destroy both", id);
    eq_int(tag, 1, total_calls());
    eq_int("move releaser is the right one", 1, g_calls[id]);
}

extern "C" const char * probe_version_defined();
extern "C" const char * probe_version_plain();

int main() {
    goose_init(0, nullptr);

    // ── the wrappers ────────────────────────────────────────────────────────
    eq_int("cpph_wrap_count", 7, cpph_wrap_count());
    cases++;
    for (int i = 0; i < 7; i++) {
        cases++;
        char tag[64];
        std::snprintf(tag, sizeof tag, "ptr name %d", i);
        eq_str(tag, NATIVE_PTR[i], port_str(cpph_wrap_ptr, i));
        std::snprintf(tag, sizeof tag, "deleter name %d", i);
        eq_str(tag, NATIVE_DEL[i], port_str(cpph_wrap_deleter, i));
        std::snprintf(tag, sizeof tag, "free name %d", i);
        eq_str(tag, NATIVE_FREE[i], port_str(cpph_free_name, i));
        std::snprintf(tag, sizeof tag, "wrap %d calls its own free", i);
        eq_int(tag, i, cpph_wrap_free(i));
        std::snprintf(tag, sizeof tag, "wrap %d is unique_ptr", i);
        eq_int(tag, 0, cpph_wrap_kind(i));
        std::snprintf(tag, sizeof tag, "deleter %d is empty", i);
        eq_int(tag, NATIVE_EMPTY[i] ? 1 : 0, cpph_deleter_empty() == 0 ? 0 : 1);
        std::snprintf(tag, sizeof tag, "deleter %d returns void", i);
        eq_int(tag, NATIVE_VOID[i] ? 1 : 0, cpph_deleter_void());
        std::snprintf(tag, sizeof tag, "deleter %d makes one call", i);
        eq_int(tag, 1, cpph_deleter_calls());
        std::snprintf(tag, sizeof tag, "sizeof wrapper %d", i);
        eq_int(tag, (int64_t) sizeof(std::unique_ptr<ggml_context, ggml_context_deleter>), cpph_wrapper_size());
    }
    eq_int("cpph_free_count", 7, cpph_free_count());
    eq_int("ptr 7 refused", -1, call_str(cpph_wrap_ptr, 7, nullptr, 0));
    eq_int("free 7 refused", -1, cpph_wrap_free(7));

    cases++;
    {
        reset_counts();
        ggml_context_ptr p(reinterpret_cast<ggml_context *>(0x1001));
        p.reset();
        eq_int("context reset releases once", 1, g_calls[0]);
        eq_int("context reset releases nothing else", 0, total_calls() - g_calls[0]);
        eq_int("context reset keeps the pointer", (int64_t) 0x1001, (int64_t) g_last[0]);
        p.reset();
        eq_int("second reset on empty releases nothing", 0, total_calls() - g_calls[0]);
    }
    cases++;
    exercise<ggml_context_ptr>(0, reinterpret_cast<ggml_context *>(0x2001));
    exercise<gguf_context_ptr>(1, reinterpret_cast<gguf_context *>(0x2002));
    exercise<ggml_gallocr_ptr>(2, reinterpret_cast<ggml_gallocr *>(0x2003));
    exercise<ggml_backend_ptr>(3, reinterpret_cast<ggml_backend *>(0x2004));
    exercise<ggml_backend_buffer_ptr>(4, reinterpret_cast<ggml_backend_buffer *>(0x2005));
    exercise<ggml_backend_event_ptr>(5, reinterpret_cast<ggml_backend_event *>(0x2006));
    exercise<ggml_backend_sched_ptr>(6, reinterpret_cast<ggml_backend_sched *>(0x2007));

    // The port's stated call counts, as opposed to the measured ones.
    eq_int("destroy live calls", 1, cpph_destroy_calls(1));
    eq_int("destroy empty calls", 0, cpph_destroy_calls(0));
    eq_int("release calls", 0, cpph_release_calls());
    eq_int("move total calls", 1, cpph_move_calls());
    eq_int("include count", 5, cpph_include_count());
    eq_int("cpp only", 1, cpph_cpp_only());

    // ── carrier enums ──────────────────────────────────────────────────────
    eq_int("cpph_enum_count", 3, cpph_enum_count());
    cases++;
    {
        const int dense[4] = {
            (int) bmoe::DenseWeightsMode::Mmap,
            (int) bmoe::DenseWeightsMode::Warmed,
            (int) bmoe::DenseWeightsMode::Anonymous,
            (int) bmoe::DenseWeightsMode::Pinned,
        };
        const char * dense_txt[4] = {"Mmap", "Warmed", "Anonymous", "Pinned"};
        const int draft[3] = {
            (int) bmoe::DraftSource::none,
            (int) bmoe::DraftSource::mtp,
            (int) bmoe::DraftSource::ngram,
        };
        const char * draft_txt[3] = {"none", "mtp", "ngram"};
        const int think[3] = {
            (int) bmoe::ThinkControl::Template,
            (int) bmoe::ThinkControl::Prefill,
            (int) bmoe::ThinkControl::None,
        };
        const char * think_txt[3] = {"Template", "Prefill", "None"};
        for (int m = 0; m < 4; m++) {
            char tag[64];
            std::snprintf(tag, sizeof tag, "DenseWeightsMode member %d", m);
            eq_int(tag, dense[m], cpph_enum_value(0, m));
            std::snprintf(tag, sizeof tag, "DenseWeightsMode text %d", m);
            eq_str(tag, dense_txt[m], port_enum_text(0, m));
        }
        for (int m = 0; m < 3; m++) {
            char tag[64];
            std::snprintf(tag, sizeof tag, "DraftSource member %d", m);
            eq_int(tag, draft[m], cpph_enum_value(1, m));
            std::snprintf(tag, sizeof tag, "DraftSource text %d", m);
            eq_str(tag, draft_txt[m], port_enum_text(1, m));
            std::snprintf(tag, sizeof tag, "ThinkControl member %d", m);
            eq_int(tag, think[m], cpph_enum_value(2, m));
            std::snprintf(tag, sizeof tag, "ThinkControl text %d", m);
            eq_str(tag, think_txt[m], port_enum_text(2, m));
        }
    }

    // ── carrier defaults ───────────────────────────────────────────────────
    const bmoe::MoeStreamConfig moe{};
    const bmoe::SpecConfig spec{};
    const bmoe::SamplingConfig smp{};
    const bmoe::RunConfig run{};
    const bmoe::SessionConfig sess{};
    struct Row {
        const char * name;
        int kind;
        long long num;
        float real;
        const char * txt;
    };
    const Row rows[] = {
        {"MoeStreamConfig.cache_auto", 0, moe.cache_auto, 0, ""},
        {"MoeStreamConfig.cache_ceil_mb", 0, moe.cache_ceil_mb, 0, ""},
        {"MoeStreamConfig.cache_floor_mb", 0, moe.cache_floor_mb, 0, ""},
        {"MoeStreamConfig.cache_mb", 0, moe.cache_mb, 0, ""},
        {"MoeStreamConfig.dense_weights", 0, (int) moe.dense_weights, 0, ""},
        {"MoeStreamConfig.drop_cold_frac", 1, 0, moe.drop_cold_frac, ""},
        {"MoeStreamConfig.drop_low_topk_warn", 0, bmoe::MoeStreamConfig::drop_low_topk_warn, 0, ""},
        {"MoeStreamConfig.drop_prefill", 0, moe.drop_prefill, 0, ""},
        {"MoeStreamConfig.drop_renorm", 0, moe.drop_renorm, 0, ""},
        {"MoeStreamConfig.enabled", 0, moe.enabled, 0, ""},
        {"MoeStreamConfig.force_cache", 0, moe.force_cache, 0, ""},
        {"MoeStreamConfig.io_threads", 0, moe.io_threads, 0, ""},
        {"MoeStreamConfig.io_threads_max", 0, bmoe::MoeStreamConfig::io_threads_max, 0, ""},
        {"MoeStreamConfig.io_two_wave", 0, moe.io_two_wave, 0, ""},
        {"MoeStreamConfig.load_all", 0, moe.load_all, 0, ""},
        {"MoeStreamConfig.o_direct", 0, moe.o_direct, 0, ""},
        {"MoeStreamConfig.overlap", 0, moe.overlap, 0, ""},
        {"MoeStreamConfig.predict_log", 0, moe.predict_log, 0, ""},
        {"MoeStreamConfig.predict_prefetch", 0, moe.predict_prefetch, 0, ""},
        {"MoeStreamConfig.predict_spec_max", 0, moe.predict_spec_max, 0, ""},
        {"MoeStreamConfig.prefetch_layers", 0, moe.prefetch_layers, 0, ""},
        {"MoeStreamConfig.prefetch_layers_max", 0, bmoe::MoeStreamConfig::prefetch_layers_max, 0, ""},
        {"MoeStreamConfig.prefetch_sync", 0, moe.prefetch_sync, 0, ""},
        {"MoeStreamConfig.route_ahead", 0, moe.route_ahead, 0, ""},
        {"MoeStreamConfig.route_ahead_max", 0, bmoe::MoeStreamConfig::route_ahead_max, 0, ""},
        {"MoeStreamConfig.row_stream", 0, moe.row_stream, 0, ""},
        {"MoeStreamConfig.row_stream_mb", 0, moe.row_stream_mb, 0, ""},
        {"MoeStreamConfig.substitute_lambda", 1, 0, moe.substitute_lambda, ""},
        {"RunConfig.chatml", 0, run.chatml, 0, ""},
        {"RunConfig.compute_trace_layers", 0, run.compute_trace_layers, 0, ""},
        {"RunConfig.expert_hot_slots", 0, run.expert_hot_slots, 0, ""},
        {"RunConfig.model_path", 2, 0, 0, run.model_path.c_str()},
        {"RunConfig.n_ctx", 0, run.n_ctx, 0, ""},
        {"RunConfig.n_expert_used", 0, run.n_expert_used, 0, ""},
        {"RunConfig.n_gpu_layers", 0, run.n_gpu_layers, 0, ""},
        {"RunConfig.n_predict", 0, run.n_predict, 0, ""},
        {"RunConfig.n_threads", 0, run.n_threads, 0, ""},
        {"RunConfig.n_ubatch", 0, run.n_ubatch, 0, ""},
        {"RunConfig.prompt", 2, 0, 0, run.prompt.c_str()},
        {"RunConfig.progress", 0, run.progress, 0, ""},
        {"RunConfig.think", 0, run.think, 0, ""},
        {"RunConfig.vram_cache_mb", 0, run.vram_cache_mb, 0, ""},
        {"RunConfig.workload_capture_path", 2, 0, 0, run.workload_capture_path.c_str()},
        {"RunConfig.workload_replay_path", 2, 0, 0, run.workload_replay_path.c_str()},
        {"SamplingConfig.seed", 0, (long long) smp.seed, 0, ""},
        {"SamplingConfig.temp", 1, 0, smp.temp, ""},
        {"SamplingConfig.top_k", 0, smp.top_k, 0, ""},
        {"SamplingConfig.top_p", 1, 0, smp.top_p, ""},
        {"SessionConfig.chatml", 0, sess.chatml, 0, ""},
        {"SessionConfig.compute_trace_layers", 0, sess.compute_trace_layers, 0, ""},
        {"SessionConfig.expert_hot_slots", 0, sess.expert_hot_slots, 0, ""},
        {"SessionConfig.n_batch", 0, sess.n_batch, 0, ""},
        {"SessionConfig.n_ctx", 0, sess.n_ctx, 0, ""},
        {"SessionConfig.n_threads", 0, sess.n_threads, 0, ""},
        {"SpecConfig.draft_max", 0, spec.draft_max, 0, ""},
    };
    eq_int("cpph_default_count", (int64_t) (sizeof(rows) / sizeof(rows[0])), cpph_default_count());
    cases++;
    for (const auto & r : rows) {
        cases++;
        int idx = -1;
        for (int i = 0; i < cpph_default_count(); i++) {
            if (port_str(cpph_default_name, i) == r.name) {
                idx = i;
                break;
            }
        }
        values++;
        if (idx < 0) {
            fails++;
            std::printf("FAIL default %-44s missing from port\n", r.name);
            continue;
        }
        eq_int((std::string("default kind ") + r.name).c_str(), r.kind, cpph_default_kind(idx));
        if (r.kind == 0) {
            eq_int((std::string("default ") + r.name).c_str(), r.num, cpph_default_int(idx));
            eq_int((std::string("default text refusal ") + r.name).c_str(), -1, call_str(cpph_default_text, idx, nullptr, 0));
        } else if (r.kind == 1) {
            eq_real((std::string("default ") + r.name).c_str(), r.real, cpph_default_real(idx));
            eq_int((std::string("default int refusal ") + r.name).c_str(), -1, cpph_default_int(idx));
        } else {
            eq_str((std::string("default ") + r.name).c_str(), r.txt, port_str(cpph_default_text, idx));
            eq_int((std::string("default int refusal ") + r.name).c_str(), -1, cpph_default_int(idx));
        }
    }
    eq_int("default name -1 refused", -1, call_str(cpph_default_name, -1, nullptr, 0));
    eq_int("default real on int refused", -1, (int64_t) cpph_default_real(0));

    // ── pod layouts ────────────────────────────────────────────────────────
    struct LRow {
        const char * name;
        long long val;
    };
    const LRow layout[] = {
        {"sizeof:SamplingConfig", (long long) sizeof(bmoe::SamplingConfig)},
        {"sizeof:MoeStreamConfig", (long long) sizeof(bmoe::MoeStreamConfig)},
        {"sizeof:SpecConfig", (long long) sizeof(bmoe::SpecConfig)},
        {"offsetof:MoeStreamConfig.cache_floor_mb", (long long) offsetof(bmoe::MoeStreamConfig, cache_floor_mb)},
        {"offsetof:MoeStreamConfig.io_threads", (long long) offsetof(bmoe::MoeStreamConfig, io_threads)},
        {"offsetof:MoeStreamConfig.dense_weights", (long long) offsetof(bmoe::MoeStreamConfig, dense_weights)},
        {"offsetof:SpecConfig.draft_max", (long long) offsetof(bmoe::SpecConfig, draft_max)},
        {"offsetof:SpecConfig.ngram_max_match", (long long) offsetof(bmoe::SpecConfig, ngram_max_match)},
    };
    eq_int("cpph_layout_count", 8, cpph_layout_count());
    cases++;
    for (int i = 0; i < 8; i++) {
        cases++;
        eq_str((std::string("layout name ") + std::to_string(i)).c_str(), layout[i].name,
               port_str(cpph_layout_name, i));
        eq_int((std::string("layout ") + layout[i].name).c_str(), layout[i].val, cpph_layout_value(i));
    }
    eq_int("layout 8 refused", -1, cpph_layout_value(8));

    // ── SpecConfig predicates and version() ────────────────────────────────
    cases++;
    for (int source = 0; source < 3; source++) {
        bmoe::SpecConfig s{};
        s.source = static_cast<bmoe::DraftSource>(source);
        eq_int("spec enabled matches header", s.enabled() ? 1 : 0, cpph_spec_enabled(source));
        eq_int("spec is_mtp matches header", s.is_mtp() ? 1 : 0, cpph_spec_is_mtp(source));
        eq_int("spec is_ngram matches header", s.is_ngram() ? 1 : 0, cpph_spec_is_ngram(source));
    }

    cases++;
    {
        eq_str("version with BMOE_VERSION", probe_version_defined(), port_ver(1));
        eq_str("version without BMOE_VERSION", probe_version_plain(), port_ver(0));
        eq_str("version fallback text", "unknown", port_ver(0));
    }

    std::printf("FG659_CPPH cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
