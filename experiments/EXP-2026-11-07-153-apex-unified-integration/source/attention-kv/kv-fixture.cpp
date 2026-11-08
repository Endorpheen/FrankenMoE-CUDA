// worker-2/kv-fixture.cpp
//
// Differential fixture driver. It owns no model, no weights and no cache
// construction: the parent harness supplies an already-constructed
// llama_kv_cache* plus the ubatches for the journal. Every operation goes
// through kv-replay.cpp (real public API) so the trace can be diffed against
// kv-lifecycle.goose / kv-seq-control.goose running over kv-bridge.cpp.
//
// Build only together with the parent test harness (see kv-report.md).

#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
int64_t fg_kv_replay_find_slot(void * kv, void * ub, int64_t cont, int64_t * out, int64_t cap, int64_t * s0, int64_t * s1);
int64_t fg_kv_replay_prepare(void * kv, void * const * ubs, int64_t n);
int64_t fg_kv_replay_apply(void * kv, void * ub);
void    fg_kv_replay_clear(void * kv);
int64_t fg_kv_replay_cell_stats(void * kv, int64_t seq, int64_t * out6);
int64_t fg_kv_replay_is_empty(void * kv, int64_t seq, int64_t cell);
int64_t fg_kv_replay_pos(void * kv, int64_t seq, int64_t cell);
int64_t fg_kv_replay_has_shift(void * kv);
int64_t fg_kv_replay_can_shift(void * kv);
int64_t fg_kv_replay_seq_rm(void * kv, int64_t seq, int64_t p0, int64_t p1);
void    fg_kv_replay_seq_add(void * kv, int64_t seq, int64_t p0, int64_t p1, int64_t shift);
void    fg_kv_replay_seq_div(void * kv, int64_t seq, int64_t p0, int64_t p1, int64_t d);
int64_t fg_kv_replay_seq_pos_min(void * kv, int64_t seq);
int64_t fg_kv_replay_seq_pos_max(void * kv, int64_t seq);
void    fg_kv_replay_state_clear(void * kv, int64_t seq);

// Provided by the parent harness. Weak so this file links standalone for a
// syntax-only build.
void * fg_kv_fixture_cache(void) __attribute__((weak));
void * fg_kv_fixture_ubatch(int64_t idx) __attribute__((weak));
}

namespace {

void stats(const char * tag, void * kv, int64_t seq) {
    int64_t s[6] = {0, 0, 0, 0, 0, 0};
    fg_kv_replay_cell_stats(kv, seq, s);
    std::printf("%s used=%lld used_min=%lld used_max_p1=%lld seq_min=%lld seq_max=%lld seq_count=%lld\n",
            tag, (long long) s[0], (long long) s[1], (long long) s[2],
            (long long) s[3], (long long) s[4], (long long) s[5]);
}

void cells(const char * tag, void * kv, int64_t seq, int64_t n) {
    for (int64_t i = 0; i < n; ++i) {
        std::printf("%s cell[%lld] empty=%lld pos=%lld\n", tag, (long long) i,
                (long long) fg_kv_replay_is_empty(kv, seq, i),
                (long long) fg_kv_replay_pos(kv, seq, i));
    }
}

int run(void * kv, void ** ub) {
    int64_t out[8] = {0};
    int64_t s0 = -1, s1 = -1;

    // J1: first two tokens into an empty 4-cell cache.
    int64_t n = fg_kv_replay_find_slot(kv, ub[0], 0, out, 8, &s0, &s1);
    std::printf("J1 find n=%lld s0=%lld s1=%lld idx0=%lld idx1=%lld\n",
            (long long) n, (long long) s0, (long long) s1,
            (long long) out[0], (long long) out[1]);
    std::printf("J1 apply=%lld\n", (long long) fg_kv_replay_apply(kv, ub[0]));
    stats("J1", kv, 0);
    cells("J1", kv, 0, 4);

    // J2: next two tokens, contiguous after the head.
    n = fg_kv_replay_find_slot(kv, ub[1], 0, out, 8, &s0, &s1);
    std::printf("J2 find n=%lld idx0=%lld idx1=%lld\n", (long long) n, (long long) out[0], (long long) out[1]);
    std::printf("J2 apply=%lld\n", (long long) fg_kv_replay_apply(kv, ub[1]));
    stats("J2", kv, 0);

    // J3: full cache, one-token ubatch must fail.
    n = fg_kv_replay_find_slot(kv, ub[2], 0, out, 8, &s0, &s1);
    std::printf("J3 find n=%lld\n", (long long) n);

    // J4: prepare failure must roll the cache back.
    fg_kv_replay_clear(kv);
    void * pair_fail[2] = { ub[3], ub[4] };
    std::printf("J4 prepare=%lld\n", (long long) fg_kv_replay_prepare(kv, pair_fail, 2));
    stats("J4", kv, 0);

    // J5: prepare success still rolls the cache back.
    fg_kv_replay_clear(kv);
    void * pair_ok[2] = { ub[3], ub[1] };
    std::printf("J5 prepare=%lld\n", (long long) fg_kv_replay_prepare(kv, pair_ok, 2));
    stats("J5", kv, 0);

    // J6: seq_add / seq_div / seq_rm control methods.
    fg_kv_replay_clear(kv);
    std::printf("J6 apply=%lld\n", (long long) fg_kv_replay_apply(kv, ub[3]));
    stats("J6a", kv, 0);
    fg_kv_replay_seq_add(kv, 0, -1, -1, 3);
    std::printf("J6 has_shift=%lld seq_min=%lld seq_max=%lld\n",
            (long long) fg_kv_replay_has_shift(kv),
            (long long) fg_kv_replay_seq_pos_min(kv, 0),
            (long long) fg_kv_replay_seq_pos_max(kv, 0));
    stats("J6b", kv, 0);
    fg_kv_replay_seq_div(kv, 0, -1, -1, 2);
    stats("J6c", kv, 0);
    std::printf("J6 rm=%lld\n", (long long) fg_kv_replay_seq_rm(kv, 0, 0, 2));
    stats("J6d", kv, 0);
    cells("J6d", kv, 0, 4);

    // J7: state_clear(seq) drops the sequence and its exclusively-owned cells.
    fg_kv_replay_state_clear(kv, 0);
    stats("J7", kv, 0);

    std::printf("done can_shift=%lld\n", (long long) fg_kv_replay_can_shift(kv));
    return 0;
}

} // namespace

int main() {
    if (fg_kv_fixture_cache == nullptr || fg_kv_fixture_ubatch == nullptr) {
        std::printf("kv-fixture: no parent cache/ubatch factory; nothing run\n");
        return 0;
    }
    void * kv = fg_kv_fixture_cache();
    void * ub[5];
    for (int i = 0; i < 5; ++i) { ub[i] = fg_kv_fixture_ubatch(i); }
    if (!kv) { std::printf("kv-fixture: null cache\n"); return 0; }
    for (int i = 0; i < 5; ++i) { if (!ub[i]) { std::printf("kv-fixture: null ubatch %d\n", i); return 0; } }
    return run(kv, ub);
}
