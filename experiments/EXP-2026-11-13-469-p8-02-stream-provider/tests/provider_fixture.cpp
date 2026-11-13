// Differential fixture for the streaming content-provider decisions (EXP469).
// Right arm: transcription of the chunked_content_provider lambda and the
// stream/plain branches in process_handler_response. Left arm: the Goose module.
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <random>

#include "provider-gen.h"

extern "C" {
void fg469_st_i64(uint64_t, uint64_t, int64_t);
}

void fg469_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

static int cases = 0, checks = 0, fails = 0;
static void fail(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 10) printf("FAIL %s: %s\n", what, detail.c_str());
}

// native provider behaviour for one invocation
struct NativeStep { int action; bool done; bool keep_going; };
static NativeStep native_step(bool has_next, bool has_chunk, bool write_ok) {
    // actions match the Goose numbering
    if (has_chunk && !write_ok) return {1, false, false};
    if (has_next) return has_chunk ? NativeStep{2, false, true} : NativeStep{4, false, true};
    return has_chunk ? NativeStep{3, true, false} : NativeStep{5, true, false};
}

int main(void) {
    goose_init(0, 0);

    // stream predicate and the nginx header
    for (int np = 0; np < 2; ++np) {
        ++cases; ++checks;
        if (is_stream(np) != np) fail("is_stream", std::to_string(np));
        ++checks;
        if (accel_header(np ? 1 : 0) != (np ? 1 : 0)) fail("accel_header", std::to_string(np));
    }

    // single-step matrix
    for (int hn = 0; hn < 2; ++hn)
    for (int hc = 0; hc < 2; ++hc)
    for (int wo = 0; wo < 2; ++wo) {
        int64_t out[3] = {-1, -1, -1};
        step(hn, hc ? 17 : 0, wo, (uint64_t) (uintptr_t) out);
        const NativeStep want = native_step(hn, hc, wo);
        ++cases; ++checks;
        if (out[0] != want.action || out[1] != (want.done ? 1 : 0) || out[2] != (want.keep_going ? 1 : 0)) {
            char d[96];
            snprintf(d, sizeof d, "has_next=%d chunk=%d write=%d", hn, hc, wo);
            fail("step", d);
        }
    }

    // provider session: compare the whole outcome against the lambda
    std::mt19937 rng(469001);
    for (int round = 0; round < 400; ++round) {
        const int n = 1 + (int) (rng() % 12);
        std::vector<int> chunk(n), hn(n), wok(n);
        for (int i = 0; i < n; ++i) {
            hn[i] = (i == n - 1) ? 0 : (int) (rng() % 2);
            chunk[i] = (int) (rng() % 3);
            wok[i] = (int) (rng() % 5) != 0;
        }
        // native: walk the provider calls
        int written_native = 0, done_native = 0, aborted_native = 0, calls_native = 0;
        for (int i = 0; i < n; ++i) {
            ++calls_native;
            const NativeStep s = native_step(hn[i], chunk[i] != 0, wok[i]);
            if (s.action == 2 || s.action == 3) ++written_native;
            if (s.done) done_native = 1;
            if (s.action == 1) { aborted_native = 1; break; }
            if (!s.keep_going) break;
        }
        // goose
        int written = 0, done = 0, aborted = 0, calls = 0;
        for (int i = 0; i < n; ++i) {
            ++calls;
            int64_t out[3];
            step(hn[i], chunk[i] ? 7 : 0, wok[i], (uint64_t) (uintptr_t) out);
            if (out[0] == 2 || out[0] == 3) ++written;
            if (out[1]) done = 1;
            if (out[0] == 1) { aborted = 1; break; }
            if (!out[2]) break;
        }
        ++cases; ++checks;
        if (written != written_native || done != done_native || aborted != aborted_native || calls != calls_native)
            fail("session", "provider outcome differs");
    }

    // on_complete ordering marker: 1 plain, 2 streaming
    for (int st = 0; st < 2; ++st) {
        int64_t out[1] = {-1};
        complete_when(st, (uint64_t) (uintptr_t) out);
        ++cases; ++checks;
        if (out[0] != (st ? 2 : 1)) fail("complete_when", std::to_string(st));
    }

    printf("FG469_PROVIDER cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
