// Gate: the bridge must reproduce the flags, sizes, out-id order, sequence lists and row fields.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>

extern "C" int64_t fg349s_ubatch(int64_t n, int64_t n_batch, const int32_t * idxs, const int32_t * logits, const int32_t * nseqid,
                                 const int32_t * seqid, int64_t n_seq_max, const int32_t * is_embd, int64_t n_embd,
                                 int64_t has_embd_vec, int64_t has_state, int64_t has_token, int64_t has_embdv,
                                 int64_t n_pos, int32_t * out_sizes, int32_t * out_flags, int32_t * out_nseqid,
                                 int32_t * out_output, int32_t * out_type, int32_t * out_seq_unq,
                                 int32_t * out_seq_idx, int32_t * out_outids, int32_t * out_outids_n);
extern "C" int fg349s_enabled(void);

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 8) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

int main(void) {
    std::mt19937 rng(356011);
    check(fg349s_enabled() == 1, "enabled by default", 0);

    for (int trial = 0; trial < 5000; ++trial) {
        const int64_t n_batch = 1 + (int64_t)(rng() % 12);
        const int64_t n = 1 + (int64_t)(rng() % 8);
        const int64_t n_seq_max = 1 + (int64_t)(rng() % 5);
        const int64_t n_embd = 4 + (int64_t)(rng() % 8);
        const int64_t n_pos = 1 + (int64_t)(rng() % 3);
        const int64_t has_embd_vec = (rng() % 3) ? 1 : 0;
        const int64_t has_state = rng() & 1, has_token = rng() & 1, has_embdv = rng() & 1;

        std::vector<int32_t> idxs((size_t)n), logits((size_t)n_batch), nseqid((size_t)n_batch), seqid((size_t)n_batch*4, 0), embd((size_t)n_batch, 0);
        for (int64_t i = 0; i < n; ++i) idxs[(size_t)i] = (int32_t)(rng() % n_batch);
        for (int64_t i = 0; i < n_batch; ++i) {
            logits[(size_t)i] = rng() & 1;
            nseqid[(size_t)i] = 1 + (int32_t)(rng() % 2);
            for (int64_t j = 0; j < nseqid[(size_t)i]; ++j) seqid[(size_t)(i*4+j)] = (int32_t)(rng() % n_seq_max);
            embd[(size_t)i] = rng() & 1;
        }

        // native model
        int64_t n_embd_rows = 0;
        if (has_embd_vec) for (int64_t i = 0; i < n; ++i) n_embd_rows += embd[(size_t)idxs[(size_t)i]];
        const bool mixed_batch = has_embd_vec != 0;
        const bool mixed = mixed_batch && n_embd_rows > 0 && n_embd_rows < n;
        const bool use_token = has_token && !(mixed_batch && n_embd_rows == n);
        const bool use_embd = has_embdv && !(mixed_batch && n_embd_rows == 0);

        std::vector<int32_t> w_sizes(3), w_flags(4), w_nseqid((size_t)n), w_output((size_t)n), w_type((size_t)n, 0);
        std::vector<int32_t> w_unq((size_t)n_seq_max, -1), w_idx((size_t)n_seq_max, -1), w_out;
        w_sizes[0] = use_embd ? (int32_t)(n*n_embd) : 0;
        w_sizes[1] = has_state ? (int32_t)(n*n_embd) : 0;
        w_sizes[2] = (int32_t)(n*n_pos);
        w_flags[0] = mixed; w_flags[1] = use_token; w_flags[2] = use_embd; w_flags[3] = (int32_t)has_state;
        for (int64_t i = 0; i < n; ++i) {
            const int64_t idx = idxs[(size_t)i];
            w_nseqid[(size_t)i] = nseqid[(size_t)idx];
            w_output[(size_t)i] = logits[(size_t)idx];
            if (mixed) w_type[(size_t)i] = embd[(size_t)idx];
            if (logits[(size_t)idx]) w_out.push_back((int32_t)idx);
        }
        int32_t u = 0;
        for (int64_t s = 0; s < n_seq_max; ++s) {
            bool present = false;
            for (int64_t r = 0; r < n && !present; ++r) {
                const int64_t idx = idxs[(size_t)r];
                for (int64_t j = 0; j < nseqid[(size_t)idx]; ++j) if (seqid[(size_t)(idx*4+j)] == s) present = true;
            }
            if (present) { w_unq[(size_t)s] = u; w_idx[(size_t)s] = u; ++u; }
        }

        std::vector<int32_t> p_sizes(3, -1), p_flags(4, -1), p_nseqid((size_t)n, -1), p_output((size_t)n, -1);
        std::vector<int32_t> p_type((size_t)n, -1), p_unq((size_t)n_seq_max, -1), p_idx((size_t)n_seq_max, -1);
        std::vector<int32_t> p_out((size_t)n + 4, -1);
        int32_t p_outn = -1;
        fg349s_ubatch(n, n_batch, idxs.data(), logits.data(), nseqid.data(), seqid.data(), n_seq_max, embd.data(),
                      n_embd, has_embd_vec, has_state, has_token, has_embdv, n_pos, p_sizes.data(),
                      p_flags.data(), p_nseqid.data(), p_output.data(), mixed ? p_type.data() : nullptr,
                      p_unq.data(), p_idx.data(), p_out.data(), &p_outn);

        for (int i = 0; i < 3; ++i) check(p_sizes[(size_t)i] == w_sizes[(size_t)i], "size", trial);
        for (int i = 0; i < 4; ++i) check(p_flags[(size_t)i] == w_flags[(size_t)i], "flag", trial);
        for (int64_t i = 0; i < n; ++i) {
            check(p_nseqid[(size_t)i] == w_nseqid[(size_t)i], "n_seq_id", trial);
            check(p_output[(size_t)i] == w_output[(size_t)i], "output", trial);
            if (mixed) check(p_type[(size_t)i] == w_type[(size_t)i], "type", trial);
        }
        check(p_outn == (int32_t)w_out.size(), "out count", trial);
        for (size_t i = 0; i < w_out.size() && i < (size_t)p_outn; ++i) check(p_out[i] == w_out[i], "out id", trial);
        for (int64_t s = 0; s < n_seq_max; ++s) {
            check(p_unq[(size_t)s] == w_unq[(size_t)s], "unq", trial);
            check(p_idx[(size_t)s] == w_idx[(size_t)s], "seq idx", trial);
        }
    }

    std::printf("FG349S_ADAPTOR checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
