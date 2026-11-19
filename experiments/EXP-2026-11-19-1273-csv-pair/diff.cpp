// The encoder and the unescaper as a pair. There is no native reference for these two helpers - they
// serve the analysis tooling, and its consumer is not in the tree - so what is checked is the
// invariant between them, stated precisely:
//   * a field without a newline round-trips exactly;
//   * a field WITH a newline comes back with it as a space, because the encoder writes it that way
//     (EXP-2026-11-19-1267). A round-trip that returned the newline would mean one of the two
//     disagrees with the writer the sink actually uses.
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

struct sl_u8 { uint8_t * data; int64_t len; };
typedef int64_t (*fn_enc)(sl_u8, sl_u8);
typedef int64_t (*fn_unesc)(sl_u8, sl_u8);
typedef int64_t (*fn_cols)(sl_u8);

int main() {
    void * h = dlopen("/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin/libllama.so.0", RTLD_LAZY);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 2; }
    auto enc   = (fn_enc)   dlsym(h, "mt_field_encode");
    auto unesc = (fn_unesc) dlsym(h, "mt_unescape_field");
    auto cols  = (fn_cols)  dlsym(h, "mt_count_cols");
    if (!enc || !unesc || !cols) { printf("missing door\n"); return 3; }

    const char alpha[] = { 'a', 'b', ',', '"', '\r', '\t', ' ', '0', 'Z' };
    std::mt19937 rng(424242);
    int fails = 0, cases = 0, lossy = 0;
    for (int t = 0; t < 4000; ++t) {
        const int n = (int)(rng() % 8);
        std::string v;
        const bool with_newline = (rng() % 4 == 0);
        for (int i = 0; i < n; ++i) v += alpha[rng() % (sizeof(alpha) / 1)];
        if (with_newline) v.insert(v.begin() + (rng() % (v.size() + 1)), '\n');

        uint8_t enc_buf[256]; memset(enc_buf, 0, sizeof(enc_buf));
        sl_u8 sv{ (uint8_t *) v.data(), (int64_t) v.size() };
        sl_u8 so{ enc_buf, (int64_t) sizeof(enc_buf) };
        const int64_t en = enc(sv, so);

        uint8_t dec_buf[256]; memset(dec_buf, 0, sizeof(dec_buf));
        sl_u8 se{ enc_buf, en };
        sl_u8 sd{ dec_buf, (int64_t) sizeof(dec_buf) };
        const int64_t dn = unesc(se, sd);

        std::string want = v;
        for (char & c : want) if (c == '\n') c = ' ';
        ++cases;
        if (with_newline) ++lossy;
        if (dn != (int64_t) want.size() || memcmp(dec_buf, want.data(), (size_t) dn) != 0) {
            ++fails;
            if (fails <= 3) printf("ROUNDTRIP MISMATCH newline=%d v.size=%zu dn=%lld want=%zu\n",
                                   with_newline ? 1 : 0, v.size(), (long long) dn, want.size());
        }
        // a single encoded field is exactly one CSV column
        if (cols(se) != 1) {
            ++fails;
            if (fails <= 3) printf("COLS MISMATCH: encoded field counted as %lld columns\n", (long long) cols(se));
        }
    }
    printf("csv pair: cases=%d fails=%d (fields with a newline: %d)\n", cases, fails, lossy);
    return fails ? 1 : 0;
}
