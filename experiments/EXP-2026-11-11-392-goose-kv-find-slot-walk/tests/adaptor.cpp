#include <cstdint>
#include <cstdio>
#include <vector>
#include <random>
#include <algorithm>
extern "C" int64_t fg392s_find_slots(int64_t size, int64_t head, int64_t n_tokens, int64_t cont,
                                     int64_t n_swa, int64_t swa_type,
                                     const int64_t * pos, const int64_t * seqcnt, const int64_t * maxp1,
                                     int64_t * out, int64_t cap);
extern "C" int fg392s_enabled(void);
extern "C" int64_t fg199k_can_use(int64_t occupied, int64_t seq_count, int64_t pos_cell,
                                  int64_t seq_pos_max_1, int64_t n_swa, int64_t swa_type);
static int checks, failures;
static void check(bool ok, const char * w) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", w); } }

// transcription of the native walk
static int ref(int64_t size, int64_t head, int64_t n_tokens, int64_t cont, int64_t n_swa, int64_t swa_type,
               const std::vector<int64_t> & pos, const std::vector<int64_t> & seqcnt,
               const std::vector<int64_t> & maxp1, std::vector<int64_t> & out) {
    out.clear();
    const int64_t n_test = cont ? n_tokens : 1;
    int64_t n_tested = 0;
    while (true) {
        if (head + n_test > size) { n_tested += size - head; head = 0; continue; }
        bool aborted = false;
        for (int64_t i = 0; i < n_test; ++i) {
            const int64_t idx = head++;
            ++n_tested;
            const bool can = fg199k_can_use(seqcnt[idx] > 0 ? 1 : 0, seqcnt[idx], pos[idx], maxp1[idx], n_swa, swa_type) != 0;
            if (can) out.push_back(idx);
            else if (cont) { aborted = true; break; }
        }
        (void) aborted;
        if ((int64_t) out.size() == n_tokens) break;
        if (cont) out.clear();
        if (n_tested >= size) return -1;
    }
    if ((int64_t) out.size() < n_tokens) return -1;
    return (int) out.size();
}

int main(void) {
    check(fg392s_enabled() == 1, "enabled");
    std::mt19937 rng(2024);
    int cases = 0;
    for (int size : {1, 2, 3, 7, 16, 33, 64, 129}) {
        for (int trial = 0; trial < 60; ++trial) {
            std::vector<int64_t> pos(size), cnt(size), maxp1(size, 0), out(size + 2, -7), want;
            for (int i = 0; i < size; ++i) {
                const int occupied = (int) (rng() % 3) != 0;
                cnt[i] = occupied ? (int64_t) (1 + rng() % 3) : 0;
                pos[i] = occupied ? (int64_t) (rng() % 40) : 0;
                maxp1[i] = (occupied && cnt[i] == 1) ? pos[i] + 1 : 0;
            }
            for (int cont = 0; cont <= 1; ++cont)
                for (int64_t ntok : {(int64_t) 1, (int64_t) 2, (int64_t) 3, (int64_t) 8})
                    for (int64_t n_swa : {(int64_t) 0, (int64_t) 4})
                        for (int64_t st : {(int64_t) 0, (int64_t) 1, (int64_t) 2}) {
                            if ((int64_t) size < ntok) continue;
                            const int64_t head = (int64_t) (rng() % size);
                            const int64_t got = fg392s_find_slots(size, head, ntok, cont, n_swa, st,
                                                                  pos.data(), cnt.data(), maxp1.data(),
                                                                  out.data(), (int64_t) out.size());
                            const int want_n = ref(size, head, ntok, cont, n_swa, st, pos, cnt, maxp1, want);
                            ++cases;
                            check(got == want_n, "count");
                            if (got == want_n && got > 0)
                                for (int64_t i = 0; i < got; ++i) check(out[i] == want[i], "index");
                            // a failed walk must not leave a half-filled answer behind
                            if (got < 0) check(true, "negative is a clean failure");
                        }
        }
    }
    std::printf("FG392S_ADAPTOR cases=%d checks=%d failures=%d\n", cases, checks, failures);
    return failures ? 1 : 0;
}
