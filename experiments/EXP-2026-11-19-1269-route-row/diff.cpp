// The route-trace row against the real sink. The weight goes through "%.6g" and a NaN produces the
// literal "nan", so this is a formatting decision, not just field order - exactly what a hand port
// gets wrong quietly.
#include "/home/end0/AI/FrankenMoE-CUDA/runtime/core/src/metrics/route_trace_sink.cpp"
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <random>
#include <string>

typedef int64_t (*fn_route)(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,double,int64_t,int64_t,int64_t,uint8_t*,int64_t);
typedef int64_t (*fn_kind)(double);

int main() {
    void * h = dlopen("/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin/libllama.so.0", RTLD_LAZY);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 2; }
    auto enc = (fn_route) dlsym(h, "mt_route_row_encode");
    auto kind = (fn_kind) dlsym(h, "mt_weight_kind");
    if (!enc || !kind) { printf("missing door\n"); return 3; }

    std::mt19937 rng(1618);
    std::uniform_real_distribution<double> d(0.0, 1.0);
    int fails = 0, cases = 0;
    for (int t = 0; t < 3000; ++t) {
        bmoe::RouteTraceRow r;
        r.turn = (int)(rng() % 20); r.phase = (int)(rng() % 3); r.step = (int)(rng() % 100);
        r.layer = (int)(rng() % 40); r.slot = (int)(rng() % 4); r.expert = (int)(rng() % 200);
        switch (rng() % 6) {
          case 0: r.weight = std::nan(""); break;
          case 1: r.weight = 0.0f; break;
          case 2: r.weight = 1e-7f; break;
          case 3: r.weight = 123456.0f; break;
          default: r.weight = (float) d(rng); break;
        }
        r.residency = (unsigned)(rng() % 3); r.expert_bytes = (unsigned long long)(rng() * 4096u); r.dropped = (unsigned)(rng() % 2);

        const char * path = "/tmp/route_row.txt";
        { FILE * f = fopen(path, "wb"); bmoe::CsvRouteTraceSink sink(f); sink.on_rows(&r, 1); }
        std::string want;
        { FILE * f = fopen(path, "rb"); char buf[1024]; size_t got = fread(buf, 1, sizeof(buf), f); fclose(f); want.assign(buf, got); }

        uint8_t buf[1024]; memset(buf, 0, sizeof(buf));
        const int64_t n = enc(r.turn, r.phase, r.step, r.layer, r.slot, r.expert, (double) r.weight,
                              (int64_t) r.residency, (int64_t) r.expert_bytes, (int64_t) r.dropped,
                              buf, (int64_t) sizeof(buf));
        ++cases;
        bool ok = n == (int64_t) want.size() && memcmp(buf, want.data(), (size_t) n) == 0;
        const int64_t k = kind((double) r.weight);
        const bool native_nan_arm = want.find(",nan,") != std::string::npos;
        if (k != (native_nan_arm ? 0 : 1)) ok = false;
        if (!ok) {
            ++fails;
            if (fails <= 3) printf("MISMATCH w=%.9g kind=%lld nanarm=%d\n  port:   '%.*s'\n  native: '%s'\n",
                                   (double) r.weight, (long long) k, native_nan_arm ? 1 : 0,
                                   (int) n, (const char*) buf, want.c_str());
        }
    }
    printf("route row: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
