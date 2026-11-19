// The io-trace row and the three trace column counts, against the real sink and its headers.
#include "/home/end0/AI/FrankenMoE-CUDA/runtime/core/src/metrics/decode_trace_sink.cpp"
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

typedef int64_t (*fn_io)(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,uint8_t*,int64_t);
typedef int64_t (*fn_cols)(int64_t);

int main() {
    void * h = dlopen("/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin/libllama.so.0", RTLD_LAZY);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 2; }
    auto enc  = (fn_io) dlsym(h, "mt_io_row_encode");
    auto cols = (fn_cols) dlsym(h, "mt_trace_cols");
    if (!enc || !cols) { printf("missing door\n"); return 3; }

    // column counts from the native headers, counted from the literals the sinks print
    const char * io_header = "turn,phase,step,layer,expert,proj,lane,spec,offset,req_bytes,read_bytes,latency_ns";
    int io_cols = 1; for (const char * p = io_header; *p; ++p) if (*p == ',') ++io_cols;
    int fails = 0;
    if (cols(1) != io_cols) { ++fails; printf("IO COLS port=%lld native=%d\n", (long long) cols(1), io_cols); }

    std::mt19937 rng(1729);
    int cases = 0;
    for (int t = 0; t < 2000; ++t) {
        bmoe::IoTraceRow r;
        r.turn = (int)(rng() % 20); r.phase = (int)(rng() % 3); r.step = (int)(rng() % 200);
        r.layer = (int)(rng() % 40); r.expert = (int)(rng() % 200); r.proj = (int)(rng() % 3);
        r.lane = (int)(rng() % 4); r.spec = (unsigned)(rng() % 2);
        r.offset = (unsigned long long)(rng() * 4096u); r.req_bytes = (unsigned long long)(rng() * 65536u);
        r.read_bytes = (unsigned long long)(rng() * 65536u); r.latency_ns = (unsigned long long)(rng() * 1000000u);

        const char * path = "/tmp/io_row.txt";
        { FILE * f = fopen(path, "wb"); bmoe::CsvIoTraceSink sink(f); sink.on_rows(&r, 1); }
        std::string want;
        { FILE * f = fopen(path, "rb"); char buf[1024]; size_t got = fread(buf, 1, sizeof(buf), f); fclose(f); want.assign(buf, got); }

        uint8_t buf[1024]; memset(buf, 0, sizeof(buf));
        const int64_t n = enc(r.turn, r.phase, r.step, r.layer, r.expert, r.proj, r.lane, (int64_t) r.spec,
                              (int64_t) r.offset, (int64_t) r.req_bytes, (int64_t) r.read_bytes,
                              (int64_t) r.latency_ns, buf, (int64_t) sizeof(buf));
        ++cases;
        if (n != (int64_t) want.size() || memcmp(buf, want.data(), (size_t) n) != 0) {
            ++fails;
            if (fails <= 3) printf("ROW MISMATCH\n  port:   '%.*s'\n  native: '%s'\n", (int) n, (const char*) buf, want.c_str());
        }
    }
    printf("io row: cases=%d fails=%d io_cols=%d\n", cases, fails, io_cols);
    return fails ? 1 : 0;
}
