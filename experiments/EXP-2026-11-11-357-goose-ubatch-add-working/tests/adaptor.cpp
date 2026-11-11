// Gate: the bridge must reproduce the working candidate's ubatch_add decisions.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>

extern "C" int64_t fg357s_ubatch(int64_t n, int64_t n_batch, const int32_t * idxs, const int32_t * logits,
                                 const int32_t * nseqid, const int32_t * seqid, int64_t n_seq_max,
                                 int64_t has_embd, int64_t n_embd, int64_t n_pos, int32_t * sizes,
                                 int32_t * row_nseqid, int32_t * row_output, int32_t * out_ids,
                                 int32_t * out_ids_n, int32_t * unq);
extern "C" int fg357s_enabled(void);

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(357011);
    check(fg357s_enabled() == 1, "enabled by default", 0);

    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t n_batch = 1 + (int64_t)(rng() % 12);
        const int64_t n = 1 + (int64_t)(rng() % 8);
        const int64_t n_seq_max = 1 + (int64_t)(rng() % 5);
        const int64_t n_embd = 4 + (int64_t)(rng() % 8);
        const int64_t n_pos = 1 + (int64_t)(rng() % 3);
        const int64_t has_embd = rng() & 1;

        std::vector<int32_t> idxs((size_t)n), logits((size_t)n_batch), nseqid((size_t)n_batch), seqid((size_t)n_batch*4, 0);
        for (int64_t i = 0; i < n; ++i) idxs[(size_t)i] = (int32_t)(rng() % n_batch);
        for (int64_t i = 0; i < n_batch; ++i) {
            logits[(size_t)i] = rng() & 1;
            nseqid[(size_t)i] = 1 + (int32_t)(rng() % 2);
            for (int64_t j = 0; j < nseqid[(size_t)i]; ++j) seqid[(size_t)(i*4+j)] = (int32_t)(rng() % n_seq_max);
        }

        std::vector<int32_t> w_sizes(2), w_ns((size_t)n), w_out((size_t)n), w_unq((size_t)n_seq_max, -1), w_ids;
        w_sizes[0] = has_embd ? (int32_t)(n*n_embd) : 0;
        w_sizes[1] = (int32_t)(n*n_pos);
        for (int64_t i = 0; i < n; ++i) {
            const int64_t idx = idxs[(size_t)i];
            w_ns[(size_t)i] = nseqid[(size_t)idx];
            w_out[(size_t)i] = logits[(size_t)idx];
            if (logits[(size_t)idx]) w_ids.push_back((int32_t)idx);
        }
        int32_t u = 0;
        for (int64_t s = 0; s < n_seq_max; ++s) {
            bool present = false;
            for (int64_t r = 0; r < n && !present; ++r) {
                const int64_t idx = idxs[(size_t)r];
                for (int64_t j = 0; j < nseqid[(size_t)idx]; ++j) if (seqid[(size_t)(idx*4+j)] == s) present = true;
            }
            if (present) w_unq[(size_t)s] = u++;
        }

        std::vector<int32_t> p_sizes(2, -1), p_ns((size_t)n, -1), p_out((size_t)n, -1);
        std::vector<int32_t> p_unq((size_t)n_seq_max, -2), p_ids((size_t)n + 4, -1);
        int32_t p_ids_n = -1;
        fg357s_ubatch(n, n_batch, idxs.data(), logits.data(), nseqid.data(), seqid.data(), n_seq_max,
                      has_embd, n_embd, n_pos, p_sizes.data(), p_ns.data(), p_out.data(), p_ids.data(),
                      &p_ids_n, p_unq.data());
        check(p_sizes[0] == w_sizes[0] && p_sizes[1] == w_sizes[1], "sizes", trial);
        for (int64_t i = 0; i < n; ++i) {
            check(p_ns[(size_t)i] == w_ns[(size_t)i], "n_seq_id", trial);
            check(p_out[(size_t)i] == w_out[(size_t)i], "output", trial);
        }
        check(p_ids_n == (int32_t)w_ids.size(), "out count", trial);
        for (size_t i = 0; i < w_ids.size() && i < (size_t)p_ids_n; ++i) check(p_ids[i] == w_ids[i], "out id", trial);
        for (int64_t s = 0; s < n_seq_max; ++s) check(p_unq[(size_t)s] == w_unq[(size_t)s], "unq", trial);
    }

    std::printf("FG357S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
