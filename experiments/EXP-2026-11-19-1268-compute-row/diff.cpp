// The compute-row encoder against the real sink path, byte for byte, plus the metrics column count
// against the real header. The row goes through the native sink (a FILE*) so the field escaping is
// exercised exactly as the shipped writer does it.
#include "/home/end0/AI/FrankenMoE-CUDA/runtime/core/src/metrics/decode_trace_sink.cpp"
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

struct sl_u8 { uint8_t * data; int64_t len; };
typedef int64_t (*fn_row)(int64_t,int64_t,int64_t,int64_t,int64_t,sl_u8,sl_u8,int64_t,int64_t,sl_u8);
typedef int64_t (*fn_cols)();

int main() {
    void * h = dlopen("/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin/libllama.so.0", RTLD_LAZY);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 2; }
    auto enc = (fn_row) dlsym(h, "mt_compute_row_encode");
    auto cols = (fn_cols) dlsym(h, "mt_metrics_cols");
    if (!enc || !cols) { printf("missing door\n"); return 3; }

    // column count: the native header has 29 names, so 29 commas and one newline
    const char * header =
        "step,steps,wall_ms,io_ms,compute_ms,read_bytes,cache_hit_pct,stall_ms,mgmt_ms,majflt,cpu_ms,"
        "dense_resident_frac,turn,majflt_mib,cache_budget_mib,rss_mib,rss_anon_mib,rss_file_mib,swap_mib,"
        "mem_available_mib,mem_free_mib,swap_free_mib,loop_overhead_ms,mtp_batch,mtp_draft_ms,drain_ms,"
        "adopt_ms,ra_issue_ms,ra_wd_ms";
    int native_cols = 1;
    for (const char * p = header; *p; ++p) if (*p == ',') ++native_cols;
    int fails = 0;
    if (cols() != native_cols) { ++fails; printf("COLS port=%lld native=%d\n", (long long) cols(), native_cols); }

    // rows: ten inputs, with op/name drawn from an alphabet that reaches every escaping rule
    const char alpha[] = { 'a', 'b', ',', '"', '\n', '\r', '\t', ' ', '0', 'Z' };
    std::mt19937 rng(2718);
    int cases = 0;
    for (int t = 0; t < 2000; ++t) {
        auto rnd = [&](int max) { std::string s; const int n = (int)(rng() % (max + 1)); for (int i = 0; i < n; ++i) s += alpha[rng() % 10]; return s; };
        bmoe::ComputeTraceRow r;
        r.turn = (int)(rng() % 20); r.phase = (int)(rng() % 3); r.step = (int)(rng() % 50);
        r.seq = (int)(rng() % 7); r.layer = (int)(rng() % 40);
        r.op = rnd(6); r.name = rnd(6);
        r.wall_ns = (unsigned long long)(rng() * 1000u); r.majflt = (unsigned long long)(rng() % 5);

        const char * path = "/tmp/compute_row.txt";
        { FILE * f = fopen(path, "wb"); bmoe::CsvComputeTraceSink sink(f); sink.on_rows(&r, 1); }
        std::string want;
        { FILE * f = fopen(path, "rb"); char buf[8192]; size_t got = fread(buf, 1, sizeof(buf), f); fclose(f); want.assign(buf, got); }

        uint8_t buf[8192]; memset(buf, 0, sizeof(buf));
        sl_u8 sop{ (uint8_t *) r.op.data(), (int64_t) r.op.size() };
        sl_u8 snm{ (uint8_t *) r.name.data(), (int64_t) r.name.size() };
        sl_u8 sout{ buf, (int64_t) sizeof(buf) };
        const int64_t n = enc(r.turn, r.phase, r.step, r.seq, r.layer, sop, snm, (int64_t) r.wall_ns, (int64_t) r.majflt, sout);
        ++cases;
        if (n != (int64_t) want.size() || memcmp(buf, want.data(), (size_t) n) != 0) {
            ++fails;
            if (fails <= 3) printf("ROW MISMATCH port(%lld) native(%zu)\n  port:   '%.*s'\n  native: '%s'\n",
                                   (long long) n, want.size(), (int) n, (const char*) buf, want.c_str());
        }
    }
    printf("compute row: cases=%d fails=%d cols=%d\n", cases, fails, native_cols);
    return fails ? 1 : 0;
}
