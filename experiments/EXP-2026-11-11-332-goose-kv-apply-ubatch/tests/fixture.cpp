// Differential gate for the Goose ubatch application.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>
#include <utility>

extern "C" uint8_t fg332dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg332k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg332k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg332k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg332k_stw(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg332k_fail(int64_t c) { std::fprintf(stderr, "FG332K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 6) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 16), cnt(1);

static void push(std::vector<int64_t> & w, int64_t k, int64_t a, int64_t b) { w.push_back(k); w.push_back(a); w.push_back(b); }

int main(void) {
    std::mt19937 rng(332011);

    for (int trial = 0; trial < 3000; ++trial) {
        const int64_t n_stream = 1 + (int64_t)(rng() % 2);
        const int64_t size = 1 + (int64_t)(rng() % 3);
        const int64_t cps = size + 2 + (int64_t)(rng() % 8);
        const int64_t n_tok = n_stream*size;
        const int64_t n_seq = 1 + (int64_t)(rng() % 4);

        std::vector<int64_t> strm(n_stream), s2s(8);
        // streams are distinct and each ubatch slot is written once -- a cell holding two sequences
        // cannot be overwritten natively either (assert seq_count == 1)
        for (int64_t s = 0; s < n_stream; ++s) strm[s] = s;
        for (int64_t q = 0; q < 8; ++q) s2s[q] = (int64_t)(rng() % n_stream);

        std::vector<int64_t> idxs(n_tok);
        for (int64_t s = 0; s < n_stream; ++s) {
            std::vector<int64_t> pool;
            for (int64_t q = 0; q < cps; ++q) pool.push_back(q);
            std::shuffle(pool.begin(), pool.end(), rng);
            for (int64_t ii = 0; ii < size; ++ii) idxs[s*size+ii] = pool[(size_t)ii];
        }

        const bool pos2d = rng() & 1, is_mixed = rng() & 1, tok_present = rng() & 1;
        const int64_t ple_n = (rng() % 3 == 0) ? 1 + (int64_t)(rng() % 2) : 0;
        const int64_t ple_img = (rng() % 2) ? 0 : 100 + (int64_t)(rng() % 5);
        const int64_t ple_eos = 200 + (int64_t)(rng() % 5);

        const int64_t total = n_stream*cps;
        std::vector<int64_t> cpos(total), cmask(total), ext(total*3, -1), head(n_stream);
        for (int64_t g = 0; g < total; ++g) {
            if (rng() % 3 == 0) { cpos[g] = -1; cmask[g] = 0; }
            else { cpos[g] = (int64_t)(rng() % 8); cmask[g] = 1LL << (rng() % n_seq); }
        }
        for (int64_t s = 0; s < n_stream; ++s) head[s] = (int64_t)(rng() % 20);

        std::vector<int64_t> ub_pos(n_tok*(pos2d ? 3 : 1)), ub_nseq(n_tok), ub_ids(n_tok*4, 0);
        std::vector<int64_t> ub_tok(n_tok), ub_type(n_tok);
        for (int64_t i = 0; i < n_tok; ++i) {
            ub_pos[i] = (int64_t)(rng() % 10);
            if (pos2d) { ub_pos[i + n_tok] = (int64_t)(rng() % 4); ub_pos[i + n_tok*2] = (int64_t)(rng() % 4); }
            // ids inside one token must be distinct: native seq_add asserts the bit is free
            ub_nseq[i] = 1 + (int64_t)(rng() % std::min<int64_t>(2, n_seq));
            if (ub_nseq[i] > n_seq) ub_nseq[i] = n_seq;
            {
                std::vector<int64_t> pool;
                for (int64_t q = 0; q < n_seq; ++q) pool.push_back(q);
                std::shuffle(pool.begin(), pool.end(), rng);
                for (int64_t j = 0; j < ub_nseq[i]; ++j) ub_ids[i*4+j] = pool[(size_t)j];
            }
            ub_tok[i] = 500 + (int64_t)(rng() % 100);
            ub_type[i] = (rng() & 1);
        }

        // ---- reference (independent transcription) ----
        std::vector<int64_t> rpos = cpos, rmask = cmask, rext = ext, rhead = head;
        std::vector<int64_t> w;
        int64_t maxrm[64];
        for (int q = 0; q < 64; ++q) maxrm[q] = -1;
        for (int64_t s = 0; s < n_stream; ++s) {
            const int64_t base = strm[s]*cps;
            for (int64_t ii = 0; ii < size; ++ii) {
                const int64_t i = s*size + ii;
                const int64_t g = base + idxs[i];
                if (rpos[g] != -1) {
                    int64_t sid = -1;
                    for (int64_t q = 0; q < 64; ++q) if (rmask[g] & (1LL << q)) { sid = q; break; }
                    push(w, 0, g, rpos[g]);

                    if (rpos[g] > maxrm[sid]) maxrm[sid] = rpos[g];
                    rmask[g] = 0; rpos[g] = -1;
                }
                rpos[g] = ub_pos[i];
                push(w, 1, g, ub_pos[i]);
                if (pos2d || tok_present || ple_n > 0) {
                    int64_t x = 0, y = 0;
                    if (pos2d) { x = ub_pos[i + n_tok*2]; y = ub_pos[i + n_tok]; }
                    bool is_embd = !tok_present || (is_mixed && ub_type[i]);
                    int64_t tok = 0;
                    if (!is_embd) tok = ub_tok[i];
                    else if (ple_n > 0) tok = ple_img != 0 ? ple_img : ple_eos;
                    rext[g*3] = x; rext[g*3+1] = y; rext[g*3+2] = tok;
                    push(w, 2, g, tok);
                }
                for (int64_t j = 0; j < ub_nseq[i]; ++j) {
                    const int64_t sid2 = ub_ids[i*4+j];
                    rmask[g] |= (1LL << sid2);
                    push(w, 3, g, sid2);
                }
            }
        }
        for (int64_t sq = 0; sq < 64; ++sq) {
            if (maxrm[sq] == -1) continue;
            const int64_t base = s2s[sq]*cps;
            int64_t mn = INT64_MAX; bool found = false;
            for (int64_t c = 0; c < cps; ++c) {
                const int64_t g = base + c;
                if (rpos[g] >= 0 && (rmask[g] & (1LL << sq))) { mn = std::min(mn, rpos[g]); found = true; }
            }
            if (!found) continue;
            if (mn <= maxrm[sq]) {
                push(w, 4, sq, mn);
                push(w, 5, maxrm[sq] + 1, 0);
                for (int64_t c = 0; c < cps; ++c) {
                    const int64_t g = base + c;
                    if (rpos[g] >= mn && rpos[g] < maxrm[sq] + 1 && (rmask[g] & (1LL << sq))) {
                        rmask[g] &= ~(1LL << sq);
                        if (rmask[g] == 0) rpos[g] = -1;
                    }
                }
            }
        }
        for (int64_t s = 0; s < n_stream; ++s) {
            const int64_t v = idxs[(s+1)*size - 1] + 1;
            rhead[strm[s]] = v;
            push(w, 6, strm[s], v);
        }

        // ---- run ----
        for (int i = 0; i < 16; ++i) { g_packet[0][i] = 0; g_packet[1][i] = 0; g_packet[2][i] = 0; g_packet[3][i] = 0; }
        std::fill(out.begin(), out.end(), -99);
        cnt[0] = 0;
        static int64_t maxrm_buf[64];
        g_packet[0][0] = n_stream; g_packet[0][1] = size;
        g_packet[0][2] = (int64_t)(uintptr_t)strm.data();
        g_packet[0][3] = (int64_t)(uintptr_t)idxs.data();
        g_packet[0][4] = (int64_t)(uintptr_t)cpos.data();
        g_packet[0][5] = (int64_t)(uintptr_t)cmask.data();
        g_packet[0][6] = (int64_t)(uintptr_t)head.data();
        g_packet[0][7] = (int64_t)(uintptr_t)s2s.data();
        g_packet[0][8] = (int64_t)(uintptr_t)ub_pos.data();
        g_packet[0][9] = (int64_t)(uintptr_t)ub_nseq.data();
        g_packet[0][10] = (int64_t)(uintptr_t)ub_ids.data();
        g_packet[0][11] = (int64_t)(uintptr_t)ub_tok.data();
        g_packet[0][12] = (int64_t)(uintptr_t)ub_type.data();
        g_packet[0][13] = (int64_t)(uintptr_t)ext.data();
        g_packet[0][14] = cps;
        g_packet[1][0] = (int64_t)(uintptr_t)out.data();
        g_packet[1][1] = pos2d; g_packet[1][2] = is_mixed; g_packet[1][3] = tok_present;
        g_packet[1][4] = ple_n; g_packet[1][5] = ple_img; g_packet[1][6] = ple_eos;
        g_packet[1][7] = n_tok;
        g_packet[2][0] = (int64_t)(uintptr_t)cnt.data();
        g_packet[3][7] = (int64_t)(uintptr_t)maxrm_buf;
        if (!fg332dh_run((int64_t)(uintptr_t)g_packet)) std::abort();

        check(cnt[0] == (int64_t)w.size()/3, "record count", trial);
        for (int64_t i = 0; i < cnt[0] && i < (int64_t)w.size()/3; ++i)
            check(out[i*3] == w[i*3] && out[i*3+1] == w[i*3+1] && out[i*3+2] == w[i*3+2], "record", trial);
        for (int64_t g = 0; g < total; ++g) {
            check(cpos[g] == rpos[g], "cell position", trial);
            check(cmask[g] == rmask[g], "cell mask", trial);
            check(ext[g*3] == rext[g*3] && ext[g*3+1] == rext[g*3+1] && ext[g*3+2] == rext[g*3+2], "cell ext", trial);
        }
        for (int64_t s = 0; s < n_stream; ++s) check(head[s] == rhead[s], "head", trial);
    }

    std::printf("FG332K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
