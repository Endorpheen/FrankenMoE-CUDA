// Differential gate for P10-08 S9: the scheduler eval sequence and the
// callbacks around it.
//
// Native arms:
//   - eval_loop below is a verbatim transcription of the C loop at
//     ggml-backend.cpp:1808-1837 (ask in index order, compute the chunk,
//     synchronize, observe, stop on a false observe, advance to j1). The port's
//     sched_run must agree with it on every want-mask up to six nodes and every
//     cancellation point, event for event and range for range;
//   - the two callback typedefs are classified with decltype against the real
//     headers;
//   - the log routing is measured on the reference libggml-base.so through
//     ggml_log_set/ggml_log_get, not assumed;
//   - the progress arm is a transcription of llama-model-loader.cpp:1570-1574
//     and 1722-1726.
//
// Port arm: the sched_ exports of source/sched-eval.goose.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

#include "ggml.h"
#include "ggml-backend.h"
#include "llama.h"

#include "sched_gen.h"

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

static void eq_str(const char * what, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        std::printf("FAIL %-58s want=[%s] got=[%s]\n", what, want.c_str(), got.c_str());
    }
}

static std::string port_str(int64_t (*fn)(int64_t, sl_u8), int64_t i) {
    uint8_t buf[512];
    std::memset(buf, 0xAA, sizeof buf);
    const sl_u8 s = { buf, (int64_t) sizeof buf };
    const int64_t n = fn(i, s);
    if (n < 0) {
        return "<refused>";
    }
    return std::string(reinterpret_cast<char *>(buf), static_cast<size_t>(n));
}

static int64_t call_run(int64_t n, int64_t mask, int64_t cancel_at, int64_t * ev, int64_t nev,
                        int64_t * comp, int64_t ncomp) {
    sl_i64 e = { ev, nev };
    sl_i64 c = { comp, ncomp };
    return sched_run(n, mask, cancel_at, e, c);
}

static int64_t call_shape(int64_t id, int64_t * out, int64_t cap) {
    sl_i64 s = { out, cap };
    return sched_cb_shape(id, s);
}

// ── the oracle: ggml-backend.cpp:1808-1837, transcribed ─────────────────────
struct Trace {
    std::vector<int> events;   // index * 2 + phase
    std::vector<int> ranges;   // start * 256 + end
    bool cancelled = false;
};

static Trace eval_loop(int n, int want_mask, int cancel_at) {
    Trace tr;
    auto wanted = [&](int i) { return (want_mask >> i) & 1; };
    for (int j0 = 0; j0 < n; j0++) {
        bool need = wanted(j0) != 0;
        tr.events.push_back(j0 * 2 + 0);
        int j1 = j0;
        while (!need && j1 < n - 1) {
            j1++;
            need = wanted(j1) != 0;
            tr.events.push_back(j1 * 2 + 0);
        }
        tr.ranges.push_back(j0 * 256 + (j1 + 1));
        if (need) {
            tr.events.push_back(j1 * 2 + 1);
            if (j1 == cancel_at) {
                tr.cancelled = true;
                break;
            }
        }
        j0 = j1;
    }
    return tr;
}

// llama-model-loader.cpp:1570-1574 and 1722-1726, transcribed.
struct Progress {
    int calls = 0;
    bool aborted = false;
    float last = -1.0f;
};

static Progress progress_loop(bool has_cb, int n_events, int abort_at) {
    Progress p;
    if (!has_cb) {
        return p;
    }
    for (int i = 0; i < n_events; i++) {
        p.calls++;
        p.last = (float) i / (float) (n_events > 0 ? n_events : 1);
        if (i == abort_at) {
            p.aborted = true;
            return p;
        }
    }
    p.calls++;
    p.last = 1.0f;
    return p;
}

// ── callback shapes ────────────────────────────────────────────────────────
enum : int { K_VOID = 0, K_BOOL = 1, K_INT = 2, K_SIZE = 3, K_FP = 4, K_PTR = 5, K_OTHER = 6 };

template <typename T> struct kind { static constexpr int v = K_OTHER; };
template <> struct kind<void> { static constexpr int v = K_VOID; };
template <> struct kind<bool> { static constexpr int v = K_BOOL; };
template <> struct kind<int> { static constexpr int v = K_INT; };
template <> struct kind<size_t> { static constexpr int v = K_SIZE; };
template <> struct kind<float> { static constexpr int v = K_FP; };
template <typename T> struct kind<T *> { static constexpr int v = K_PTR; };

template <typename F> struct shape;
template <typename R, typename... A> struct shape<R (*)(A...)> {
    static constexpr int arity = sizeof...(A);
    static constexpr int ret = kind<R>::v;
    static constexpr int par(int i) {
        if (i >= static_cast<int>(sizeof...(A))) {
            return -1;
        }
        constexpr int v[sizeof...(A) + 1] = { kind<A>::v..., -1 };
        return v[i];
    }
};

static void log_probe(enum ggml_log_level, const char *, void *) {}

int main() {
    goose_init(0, nullptr);

    // ── the eval machine, exhaustively ─────────────────────────────────────
    eq_int("sched_max_nodes", 32, sched_max_nodes());
    eq_int("phase ask", 0, sched_phase_ask());
    eq_int("phase observe", 1, sched_phase_observe());
    eq_int("ask false is not cancel", 0, sched_ask_false_cancels());

    int64_t ev[4 * 64];
    int64_t comp[64];
    for (int n = 0; n <= 6; n++) {
        cases++;
        const int masks = 1 << n;
        for (int mask = 0; mask < masks; mask++) {
            for (int cancel_at = -1; cancel_at < n; cancel_at++) {
                const Trace want = eval_loop(n, mask, cancel_at);
                const int64_t st = call_run(n, mask, cancel_at, ev, 4 * 64, comp, 64);
                values++;
                const int64_t want_st = want.cancelled ? 1 : 0;
                if (st != want_st) {
                    fails++;
                    std::printf("FAIL n=%d mask=%d cancel=%d status want=%lld got=%lld\n",
                                n, mask, cancel_at, (long long) want_st, (long long) st);
                    continue;
                }
                values++;
                if (ev[0] != (int64_t) want.events.size()) {
                    fails++;
                    std::printf("FAIL n=%d mask=%d cancel=%d event count want=%zu got=%lld\n",
                                n, mask, cancel_at, want.events.size(), (long long) ev[0]);
                    continue;
                }
                for (size_t i = 0; i < want.events.size(); i++) {
                    values++;
                    if (ev[1 + i] != want.events[i]) {
                        fails++;
                        std::printf("FAIL n=%d mask=%d cancel=%d event %zu want=%d got=%lld\n",
                                    n, mask, cancel_at, i, want.events[i], (long long) ev[1 + i]);
                        break;
                    }
                }
                values++;
                if (comp[0] != (int64_t) want.ranges.size()) {
                    fails++;
                    std::printf("FAIL n=%d mask=%d cancel=%d range count want=%zu got=%lld\n",
                                n, mask, cancel_at, want.ranges.size(), (long long) comp[0]);
                    continue;
                }
                for (size_t i = 0; i < want.ranges.size(); i++) {
                    values++;
                    if (comp[1 + i] != want.ranges[i]) {
                        fails++;
                        std::printf("FAIL n=%d mask=%d cancel=%d range %zu want=%d got=%lld\n",
                                    n, mask, cancel_at, i, want.ranges[i], (long long) comp[1 + i]);
                        break;
                    }
                }
            }
        }
    }

    // Boundary cases that the exhaustive run above cannot express.
    eq_int("n=-1 refused", -1, call_run(-1, 0, -1, ev, 4 * 64, comp, 64));
    eq_int("n=33 refused", -1, call_run(33, 0, -1, ev, 4 * 64, comp, 64));
    eq_int("cancel=-2 refused", -1, call_run(4, 0, -2, ev, 4 * 64, comp, 64));
    eq_int("cancel=n refused", -1, call_run(4, 0, 4, ev, 4 * 64, comp, 64));
    eq_int("empty event buffer refused", -1, call_run(2, 0, -1, nullptr, 0, comp, 64));

    // ── callback shapes ────────────────────────────────────────────────────
    eq_int("sched_cb_count", 2, sched_cb_count());
    cases++;
    {
        using EF = ggml_backend_sched_eval_callback;
        using BF = ggml_backend_eval_callback;
        using PF = llama_progress_callback;
        using LF = ggml_log_callback;
        static_assert(shape<EF>::arity == 3 && shape<EF>::ret == K_BOOL, "eval shape");
        static_assert(shape<BF>::arity == 4 && shape<BF>::ret == K_BOOL, "backend eval shape");
        static_assert(shape<PF>::arity == 2 && shape<PF>::ret == K_BOOL, "progress shape");
        static_assert(shape<LF>::arity == 3 && shape<LF>::ret == K_VOID, "log shape");
        for (int id = 0; id < 2; id++) {
            cases++;
            int64_t got[8];
            const int64_t n = call_shape(id, got, 8);
            const int ar = id == 0 ? 3 : 4;
            static const int PAR0[4] = {K_PTR, K_BOOL, K_PTR, -1};
            static const int PAR1[4] = {K_INT, K_PTR, K_PTR, K_PTR};
            const int * want_par = id == 0 ? PAR0 : PAR1;
            values++;
            if (n != 2 + ar) {
                fails++;
                std::printf("FAIL cb %d shape length want=%d got=%lld\n", id, 2 + ar, (long long) n);
                continue;
            }
            eq_int("cb arity", ar, got[0]);
            eq_int("cb returns bool", K_BOOL, got[1]);
            for (int j = 0; j < ar; j++) {
                eq_int("cb param class", want_par[j], got[2 + j]);
            }
        }
        const char * names[2] = {"ggml_backend_sched_eval_callback", "ggml_backend_eval_callback"};
        for (int id = 0; id < 2; id++) {
            char tag[64];
            std::snprintf(tag, sizeof tag, "cb name %d", id);
            eq_str(tag, names[id], port_str(sched_cb_name, id));
        }
    }
    eq_int("cb 2 refused", -1, call_shape(2, nullptr, 0));

    // ── log levels and routing, against the reference library ──────────────
    eq_int("sched_level_count", 6, sched_level_count());
    cases++;
    {
        const char * names[6] = {"GGML_LOG_LEVEL_NONE", "GGML_LOG_LEVEL_DEBUG", "GGML_LOG_LEVEL_INFO",
                                 "GGML_LOG_LEVEL_WARN", "GGML_LOG_LEVEL_ERROR", "GGML_LOG_LEVEL_CONT"};
        const int vals[6] = {GGML_LOG_LEVEL_NONE, GGML_LOG_LEVEL_DEBUG, GGML_LOG_LEVEL_INFO,
                             GGML_LOG_LEVEL_WARN, GGML_LOG_LEVEL_ERROR, GGML_LOG_LEVEL_CONT};
        for (int i = 0; i < 6; i++) {
            cases++;
            eq_str("level name", names[i], port_str(sched_level_name, i));
            eq_int("level value", vals[i], sched_level_value(i));
        }
        eq_int("log cb arity", 3, sched_log_cb_arity());
        eq_int("log cb returns void", 1, sched_log_cb_void());

        // The reference library is the oracle for the routing rule.
        ggml_log_callback got_cb = reinterpret_cast<ggml_log_callback>(0x1);
        void * got_ud = reinterpret_cast<void *>(0x1);
        ggml_log_get(&got_cb, &got_ud);
        cases++;
        // The library starts on its built-in default callback, which writes to
        // stderr; the slot is never null.
        values++;
        if (got_cb == nullptr) {
            fails++;
            std::printf("FAIL the library starts without a default log callback\n");
        }
        const ggml_log_callback default_cb = got_cb;
        eq_int("default callback means stderr", 1, sched_log_route(0, 0));
        eq_int("unset log goes to stderr", 1, sched_log_route(0, 0));
        eq_int("null set goes to stderr", 1, sched_log_route(1, 1));

        static int sink = 0;
        ggml_log_set(log_probe, &sink);
        ggml_log_get(&got_cb, &got_ud);
        values++;
        if (got_cb != log_probe || got_ud != &sink) {
            fails++;
            std::printf("FAIL ggml_log_set did not store the callback\n");
        }
        eq_int("installed callback receives the line", 0, sched_log_route(1, 0));
        ggml_log_set(nullptr, nullptr);
        ggml_log_get(&got_cb, &got_ud);
        values++;
        if (got_cb != default_cb || got_ud != nullptr) {
            fails++;
            std::printf("FAIL ggml_log_set(nullptr) did not reinstall the default callback\n");
        }
        eq_int("null set restores the default", 1, sched_log_null_restores_default());
    }
    eq_int("level -1 refused", -1, sched_level_value(-1));

    // ── progress ───────────────────────────────────────────────────────────
    for (int has = 0; has <= 1; has++) {
        cases++;
        for (int n = 0; n <= 5; n++) {
            for (int abort_at = -1; abort_at < n; abort_at++) {
                const Progress want = progress_loop(has != 0, n, abort_at);
                const int64_t got = sched_progress_calls(has, n, abort_at);
                values++;
                if (got != want.calls) {
                    fails++;
                    std::printf("FAIL progress has=%d n=%d abort=%d calls want=%d got=%lld\n",
                                has, n, abort_at, want.calls, (long long) got);
                }
                const int64_t st = sched_progress_status(has, n, abort_at);
                eq_int("progress status matches abort", want.aborted ? 1 : 0, st);
            }
        }
    }
    eq_int("progress n=-1 refused", -1, sched_progress_calls(1, -1, -1));
    eq_int("progress abort=n refused", -1, sched_progress_calls(1, 3, 3));
    cases++;
    {
        // The final call is at 1.0 and is absent after an abort.
        const Progress ok = progress_loop(true, 3, -1);
        values++;
        if (ok.last != 1.0f) {
            fails++;
            std::printf("FAIL the successful load does not end at 1.0\n");
        }
        eq_int("final value is 1.0", 1, sched_progress_final_value() == 1.0 ? 1 : 0);
        eq_int("final skipped after abort", 0, sched_progress_final_after_abort());
        const Progress ab = progress_loop(true, 3, 1);
        values++;
        if (ab.calls != 2 || !ab.aborted) {
            fails++;
            std::printf("FAIL aborting load reached %d calls (want 2)\n", ab.calls);
        }
    }

    std::printf("FG659_SCHED cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
