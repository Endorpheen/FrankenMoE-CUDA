#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>

extern "C" int64_t fg352s_classify(int64_t n, const int32_t * is_tok, const int32_t * has_embd,
                                   const int32_t * has_state, const int32_t * id, int64_t n_vocab,
                                   const int32_t * nseqid, const int32_t * seqid, int64_t n_seq_max,
                                   int64_t allow_mixed, int32_t * flags);
extern "C" int fg352s_enabled(void);

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(370011);
    check(fg352s_enabled() == 1, "enabled by default", 0);
    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t n = 1 + (int64_t)(rng() % 6);
        const int64_t n_vocab = 4 + (int64_t)(rng() % 8);
        const int64_t n_seq_max = 1 + (int64_t)(rng() % 4);
        const int64_t allow = rng() & 1;
        std::vector<int32_t> t((size_t)n), e((size_t)n), s((size_t)n), id((size_t)n), ns((size_t)n), sq((size_t)n*4, 0);
        for (int64_t i = 0; i < n; ++i) {
            t[(size_t)i] = (rng() % 5 == 0) ? 0 : 1;
            e[(size_t)i] = rng() & 1;
            s[(size_t)i] = (rng() % 6 == 0) ? 1 : 0;
            id[(size_t)i] = (int32_t)((int64_t)(rng() % (n_vocab + 2)) - 1);
            ns[(size_t)i] = (int32_t)(rng() % 3);
            for (int64_t j = 0; j < ns[(size_t)i]; ++j) sq[(size_t)(i*4+j)] = (int32_t)(rng() % (n_seq_max + 1));
        }
        int64_t code = 0; int32_t mixed = 0;
        int64_t nto = 0, neo = 0, nb = 0;
        const int64_t st0 = s[0];
        for (int64_t i = 0; i < n && !code; ++i) {
            if (!t[(size_t)i] && !e[(size_t)i]) { code = 1; break; }
            if (s[(size_t)i] != st0) { code = 2; break; }
            if (t[(size_t)i] && !e[(size_t)i]) ++nto;
            if (e[(size_t)i] && !t[(size_t)i]) ++neo;
            if (t[(size_t)i] && e[(size_t)i]) ++nb;
        }
        bool has_token = false, has_embd = false;
        if (!code) {
            if (nb > 0 && nb != n) code = 3;
            else {
                mixed = (nto > 0 && neo > 0);
                if (mixed && !allow) code = 4;
                else {
                    has_token = nto > 0 || nb > 0;
                    has_embd = neo > 0 || nb > 0;
                    if (has_token) {
                        for (int64_t i = 0; i < n && !code; ++i) {
                            const bool marked = mixed && e[(size_t)i];
                            if (!marked && (id[(size_t)i] < 0 || id[(size_t)i] >= n_vocab)) code = 5;
                        }
                    }
                    if (!code) for (int64_t i = 0; i < n && !code; ++i)
                        for (int64_t j = 0; j < ns[(size_t)i]; ++j) {
                            const int32_t sid = sq[(size_t)(i*4+j)];
                            if (sid < 0 || sid >= n_seq_max) { code = 6; break; }
                        }
                }
            }
        }
        int32_t flags[4] = {-1,-1,-1,-1};
        const int64_t got = fg352s_classify(n, t.data(), e.data(), s.data(), id.data(), n_vocab,
                                            ns.data(), sq.data(), n_seq_max, allow, flags);
        check(got == code, "code", trial);
        if (!code) {
            check(flags[0] == (mixed ? 1 : 0), "mixed", trial);
            check(flags[1] == (has_token ? 1 : 0), "has_token", trial);
            check(flags[2] == (has_embd ? 1 : 0), "has_embd", trial);
            check(flags[3] == (int32_t) st0, "has_state", trial);
        }
    }
    std::printf("FG352S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
