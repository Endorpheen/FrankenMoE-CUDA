// Differential gate for the Goose SWA KV cache policy.
//
// Oracle: a transcription of the size rule, the layer filters, the delegation results and the
// memory breakdown merge from llama-kv-cache-iswa.cpp.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>
#include <random>
#include <algorithm>

extern "C" uint8_t fg298dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg298k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg298k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg298k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg298k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg298k_fail(int64_t c) { std::fprintf(stderr, "FG298K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static int64_t ref_size_swa(int64_t size_base, int64_t n_swa, bool unified, int64_t n_seq_max,
                            int64_t n_ubatch, bool swa_full) {
    uint32_t size_swa = (uint32_t)std::min<int64_t>(size_base, n_swa * (unified ? n_seq_max : 1) + n_ubatch);
    size_swa = (size_swa + 255u) & ~255u;   // GGML_PAD
    if (swa_full) size_swa = (uint32_t)size_base;
    return size_swa;
}

static int64_t run_goose(int op, const std::vector<int64_t> & in, int64_t ret_in = 0) {
    for (int i = 0; i < 15; ++i) g_packet[0][i] = (i < (int)in.size()) ? in[i] : 0;
    g_packet[3][0] = 0;
    g_packet[3][1] = ret_in;
    if (!fg298dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(298011);
    static std::vector<int64_t> out(1 << 16);

    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t size_base = (int64_t)(rng() % 8192);
        const int64_t n_swa = (int64_t)(rng() % 2048);
        const bool unified = (rng() & 1) != 0;
        const int64_t n_seq_max = (int64_t)(rng() % 8);
        const int64_t n_ubatch = (int64_t)(rng() % 512);
        const bool swa_full = (rng() % 5) == 0;

        std::vector<int64_t> in(15, 0);
        in[0] = 0; in[1] = (int64_t)(uintptr_t)out.data();
        in[2] = size_base; in[3] = n_swa; in[4] = unified ? 1 : 0; in[5] = n_seq_max;
        in[6] = n_ubatch; in[7] = swa_full ? 1 : 0;
        const int64_t got = run_goose(0, in);
        check(got == ref_size_swa(size_base, n_swa, unified, n_seq_max, n_ubatch, swa_full), "size_swa", trial);
    }

    for (int trial = 0; trial < 500; ++trial) {
        const int n_layer = 1 + (int)(rng() % 8);
        std::vector<int64_t> swa_mask(n_layer), ext_mask(n_layer);
        const bool ext_present = (rng() % 3) == 0;
        for (int i = 0; i < n_layer; ++i) {
            swa_mask[i] = (rng() & 1);
            ext_mask[i] = (rng() & 1);
        }
        std::vector<int64_t> base, swa;
        for (int i = 0; i < n_layer; ++i) {
            const bool pass = ext_present ? (ext_mask[i] != 0) : true;
            base.push_back(pass && !swa_mask[i]);
            swa.push_back(pass && swa_mask[i]);
        }
        std::vector<int64_t> in(15, 0);
        in[0] = 1; in[1] = (int64_t)(uintptr_t)out.data();
        in[8] = n_layer;
        in[9] = (int64_t)(uintptr_t)swa_mask.data();
        in[10] = ext_present ? 1 : 0;
        in[11] = (int64_t)(uintptr_t)ext_mask.data();
        run_goose(1, in);
        for (int i = 0; i < n_layer; ++i) {
            check(out[2*i] == base[i], "base filter", trial);
            check(out[2*i+1] == swa[i], "swa filter", trial);
        }
    }

    for (int trial = 0; trial < 200; ++trial) {
        const int64_t rb = rng() & 1, rs = rng() & 1;
        std::vector<int64_t> in(15, 0);
        in[0] = 2; in[12] = rb; in[13] = rs;
        const int64_t want = (rb && rs) ? 1 : 0;
        check(run_goose(2, in) == want, "seq_rm and", trial);
    }

    for (int trial = 0; trial < 500; ++trial) {
        const int cb = (int)(rng() % 5);
        const int cs = (int)(rng() % 5);
        std::map<int64_t, int64_t> mb;
        std::vector<int64_t> kb(cb), vb(cb), ks(cs), vs(cs);
        // Each side is already a map, so its keys are unique; only overlaps merge.
        std::vector<int64_t> pool = {0, 1000, 2000, 3000, 4000, 5000};
        std::shuffle(pool.begin(), pool.end(), rng);
        for (int i = 0; i < cb; ++i) {
            kb[i] = pool[i]; vb[i] = (int64_t)(rng() % 100);
            mb[kb[i]] += vb[i];
        }
        std::shuffle(pool.begin(), pool.end(), rng);
        for (int i = 0; i < cs; ++i) {
            ks[i] = pool[i]; vs[i] = (int64_t)(rng() % 100);
            mb[ks[i]] += vs[i];
        }
        std::vector<int64_t> in(15, 0);
        in[0] = 3; in[1] = (int64_t)(uintptr_t)out.data();
        in[12] = (int64_t)(uintptr_t)kb.data();
        in[13] = (int64_t)(uintptr_t)vb.data();
        in[11] = (int64_t)(uintptr_t)ks.data();
        in[10] = (int64_t)(uintptr_t)vs.data();
        in[14] = cb;
        const int64_t n = run_goose(3, in, cs);
        check((size_t)n == mb.size(), "breakdown size", trial);
        size_t i = 0;
        for (const auto & kv : mb) {
            if (i >= (size_t)n) break;
            check(out[2*i] == kv.first, "breakdown key", trial);
            check(out[2*i+1] == kv.second, "breakdown value", trial);
            ++i;
        }
    }

    std::printf("FG298K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
