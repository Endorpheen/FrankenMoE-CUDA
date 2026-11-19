// The CSV field encoder against the real write_csv_field, byte for byte. The native writer goes
// through a FILE*, so the harness writes to a temp file and reads it back - no transcription of the
// rules anywhere, which matters here: the newline-inside-quotes behaviour is easy to get wrong.
#include "/home/end0/AI/FrankenMoE-CUDA/runtime/core/src/metrics/decode_trace_sink.cpp"
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

struct sl_u8 { uint8_t * data; int64_t len; };
typedef int64_t (*fn_enc)(sl_u8, sl_u8);

int main() {
    void * h = dlopen("/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin/libllama.so.0", RTLD_LAZY);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 2; }
    auto enc = (fn_enc) dlsym(h, "mt_field_encode");
    if (!enc) { printf("missing door mt_field_encode\n"); return 3; }

    // the alphabet is chosen so every rule is reachable: comma, quote, newline, plus ordinary bytes
    const char alpha[] = { 'a', 'b', ',', '"', '\n', '\r', '\t', ' ', '0', 'Z' };
    std::mt19937 rng(31415);
    int fails = 0, cases = 0;
    for (int t = 0; t < 4000; ++t) {
        const int n = (int)(rng() % 9);
        std::string v;
        for (int i = 0; i < n; ++i) v += alpha[rng() % (sizeof(alpha) / 1)];

        // native: through a FILE*
        const char * path = "/tmp/csv_field.txt";
        { FILE * f = fopen(path, "wb"); bmoe::write_csv_field(f, v); fclose(f); }
        std::string want;
        { FILE * f = fopen(path, "rb"); char buf[256]; size_t got = fread(buf, 1, sizeof(buf), f); fclose(f); want.assign(buf, got); }

        uint8_t buf[256]; memset(buf, 0, sizeof(buf));
        sl_u8 sin{ (uint8_t *) v.data(), (int64_t) v.size() };
        sl_u8 sout{ buf, (int64_t) sizeof(buf) };
        const int64_t n2 = enc(sin, sout);
        ++cases;
        if (n2 != (int64_t) want.size() || memcmp(buf, want.data(), (size_t) n2) != 0) {
            ++fails;
            if (fails <= 5) {
                printf("MISMATCH in='");
                for (unsigned char c : v) printf(c < 32 ? "\\x%02x" : "%c", c);
                printf("' port(%lld)='", (long long) n2);
                for (int64_t i = 0; i < n2; ++i) printf(buf[i] < 32 ? "\\x%02x" : "%c", buf[i]);
                printf("' native(%zu)='", want.size());
                for (unsigned char c : want) printf(c < 32 ? "\\x%02x" : "%c", c);
                printf("'\n");
            }
        }
    }
    printf("csv field: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
