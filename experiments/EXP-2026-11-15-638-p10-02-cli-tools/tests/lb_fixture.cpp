// Differential gate for P10-02 S1 (llama-bench report decisions).
//
// Native arm: tests/native_lb.cpp includes the base-commit llama-bench.cpp, so
// every comparison below calls the tool's own static function. Port arm: the
// lb_ exports of source/llama-bench-report.goose.
//
// The gate asserts both the return values and the exact text the report rows
// print. The four row helpers (size/params/test/t/s) and the tensor_split row
// are transcriptions inside the native arm, because print_test needs a `test`
// object whose constructor needs a model; the print formats are copied verbatim
// and cited there.
#include "lb.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" int64_t fg637_fmt2(sl_u8 out, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.2f", x);
}

// Native accessors from tests/native_lb.cpp.
extern "C" int nat_lb_out_from(const char *);
extern "C" const char * nat_lb_out_str(int);
extern "C" const char * nat_lb_split_str(int);
extern "C" const char * nat_lb_lazy_str(int);
extern "C" uint64_t nat_lb_avg_u64(const uint64_t *, int64_t);
extern "C" uint64_t nat_lb_stdev_u64(const uint64_t *, int64_t);
extern "C" double nat_lb_avg_ts(const uint64_t *, int64_t, int64_t);
extern "C" double nat_lb_stdev_ts(const uint64_t *, int64_t, int64_t);
extern "C" int nat_lb_field_type(const char *);
extern "C" int64_t nat_lb_escape_json(const char *, char *, int64_t);
extern "C" int64_t nat_lb_json_value(const char *, const char *, char *, int64_t);
extern "C" int64_t nat_lb_csv(const char *, char *, int64_t);
extern "C" int nat_lb_md_width(const char *);
extern "C" const char * nat_lb_md_name(const char *);
extern "C" const char * nat_lb_sql_type(const char *);
extern "C" int64_t nat_lb_md_size(uint64_t, char *, int64_t);
extern "C" int64_t nat_lb_md_params(uint64_t, char *, int64_t);
extern "C" int64_t nat_lb_md_test(int, int, int, char *, int64_t);
extern "C" int64_t nat_lb_md_ts(double, double, char *, int64_t);
extern "C" int64_t nat_lb_tensor_split(const float *, int, char *, int64_t);
extern "C" int64_t nat_lb_range(const char *, int, int64_t *, int64_t);
extern "C" int64_t nat_lb_defaults(int64_t *, int64_t);
extern "C" const char * nat_lb_default_cpu_mask(void);

static int cases = 0;
static int values = 0;
static int fails = 0;

static sl_u8 SL(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

static std::string got_str(int64_t n, const char * b) {
    if (n < 0) {
        return "!" + std::to_string(n);
    }
    return std::string(b, (size_t) n);
}

static void fail(const char * name, const std::string & want, const std::string & got) {
    fails++;
    printf("FAIL %-34s want=[%s] got=[%s]\n", name, want.c_str(), got.c_str());
}

static void eq_str(const char * name, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fail(name, want, got);
    }
}

static void eq_int(const char * name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fail(name, std::to_string(want), std::to_string(got));
    }
}

static void eq_f64(const char * name, double want, double got) {
    values++;
    uint64_t a = 0, b = 0;
    memcpy(&a, &want, 8);
    memcpy(&b, &got, 8);
    if (a != b) {
        char w[64], g[64];
        snprintf(w, sizeof w, "%.17g", want);
        snprintf(g, sizeof g, "%.17g", got);
        fail(name, w, g);
    }
}

static std::string port_str2(int64_t (*f)(sl_u8, sl_u8), const std::string & s) {
    char b[256];
    memset(b, 0x5a, sizeof b);
    int64_t n = f(SL(s), sl_u8{(uint8_t *) b, 256});
    return got_str(n, b);
}

int main() {
    goose_init(0, nullptr);
    char b[512];
    memset(b, 0, sizeof b);

    // ── output format names (219-252) ─────────────────────────────────────
    const char * fmts[] = {"none", "csv", "json", "jsonl", "md", "sql", "MD", "jsonlines", "", "md "};
    for (const char * f : fmts) {
        cases++;
        eq_int("lb_out_from_str", nat_lb_out_from(f), lb_out_from_str(SL(f)));
    }
    for (int v = 0; v < 6; v++) {
        cases++;
        eq_str("lb_out_str", nat_lb_out_str(v), got_str(lb_out_str(v, sl_u8{(uint8_t *) b, 256}), b));
    }

    // ── split / lazy names (254-287) ──────────────────────────────────────
    for (int v = 0; v < 4; v++) {
        cases++;
        eq_str("lb_split_str", nat_lb_split_str(v), got_str(lb_split_str(v, sl_u8{(uint8_t *) b, 256}), b));
    }
    for (int v = 0; v < 3; v++) {
        cases++;
        eq_str("lb_lazy_str", nat_lb_lazy_str(v), got_str(lb_lazy_str(v, sl_u8{(uint8_t *) b, 256}), b));
    }

    // ── aggregation (102-121) ─────────────────────────────────────────────
    {
        std::vector<std::vector<uint64_t>> sets = {
            {}, {5}, {1, 2}, {1, 2, 3, 4}, {1000000000, 2000000000, 3000000000},
            {1, 1, 1, 1, 1, 1, 2}, {999999937, 1000000007, 999999999},
        };
        for (const auto & s : sets) {
            cases++;
            int64_t p[16];
            for (size_t i = 0; i < s.size(); i++) {
                p[i] = (int64_t) s[i];
            }
            sl_i64 ps{p, (int64_t) s.size()};
            static uint64_t dummy[1] = {0};
            const uint64_t * data = s.empty() ? dummy : s.data();
            eq_int("lb_avg_u64", (int64_t) nat_lb_avg_u64(data, (int64_t) s.size()), lb_avg_u64(ps));
            eq_int("lb_stdev_u64", (int64_t) nat_lb_stdev_u64(data, (int64_t) s.size()), lb_stdev_u64(ps));
            for (int64_t ntok : {1LL, 128LL, 640LL}) {
                eq_f64("lb_avg_ts", nat_lb_avg_ts(data, (int64_t) s.size(), ntok), lb_avg_ts(ps, ntok));
                eq_f64("lb_stdev_ts", nat_lb_stdev_ts(data, (int64_t) s.size(), ntok), lb_stdev_ts(ps, ntok));
            }
        }
    }

    // ── field types, widths and names ─────────────────────────────────────
    const char * fields[] = {
        "build_commit", "build_number", "cpu_info", "gpu_info", "backends", "model_filename", "model_type",
        "model_size", "model_n_params", "n_batch", "n_ubatch", "n_threads", "cpu_mask", "cpu_strict", "poll",
        "type_k", "type_v", "n_gpu_layers", "n_cpu_moe", "split_mode", "main_gpu", "no_kv_offload",
        "flash_attn", "devices", "tensor_split", "tensor_buft_overrides", "load_mode", "lazy_mode",
        "embeddings", "no_op_offload", "no_host", "fit_target", "fit_min_ctx", "n_prompt", "n_gen",
        "n_depth", "test_time", "avg_ns", "stddev_ns", "avg_ts", "stddev_ts", "f16_kv", "model", "size",
        "params", "backend", "test", "t/s", "ts", "unknown_field",
    };
    for (const char * f : fields) {
        cases++;
        eq_int("lb_field_type", nat_lb_field_type(f), lb_field_type(SL(f)));
        if (std::string(f) != "unknown_field") {
        }
        eq_int("lb_md_width", nat_lb_md_width(f), lb_md_width(SL(f)));
        eq_str("lb_md_name", nat_lb_md_name(f), port_str2(lb_md_name, f));
        eq_str("lb_sql_type", nat_lb_sql_type(f), port_str2(lb_sql_type, f));
    }

    // ── JSON / CSV escaping (1770-1812) ───────────────────────────────────
    {
        std::vector<std::string> esc = {
            "", "abc", "a\"b", "a\\b", "\n\t\x01\x1f", "\x7f", "\x80\xff", "q\"\\\x1f\xe2\x82\xac",
        };
        for (const auto & s : esc) {
            cases++;
            char nb[256];
            int64_t nn = nat_lb_escape_json(s.c_str(), nb, sizeof nb);
            int64_t pn = lb_json_escape(SL(s), sl_u8{(uint8_t *) b, 256});
            eq_str("lb_json_escape", got_str(nn, nb), got_str(pn, b));
        }
        const char * fv[][2] = {
            {"model_filename", "x\"y"}, {"cpu_strict", "0"}, {"cpu_strict", "1"},
            {"n_prompt", "512"}, {"avg_ts", "1.5"}, {"no_kv_offload", "0"},
        };
        for (auto & f : fv) {
            cases++;
            char nb[256];
            int64_t nn = nat_lb_json_value(f[0], f[1], nb, sizeof nb);
            int64_t pn = lb_json_value(SL(f[0]), SL(f[1]), sl_u8{(uint8_t *) b, 256});
            eq_str("lb_json_value", got_str(nn, nb), got_str(pn, b));
        }
        std::vector<std::string> csv = {"", "a", "a\"b", "a,b", "\"\""};
        for (const auto & s : csv) {
            cases++;
            char nb[256];
            int64_t nn = nat_lb_csv(s.c_str(), nb, sizeof nb);
            int64_t pn = lb_csv_escape(SL(s), sl_u8{(uint8_t *) b, 256});
            eq_str("lb_csv_escape", got_str(nn, nb), got_str(pn, b));
        }
    }

    // ── markdown rows (2065-2105) ─────────────────────────────────────────
    {
        const uint64_t sizes[] = {0, 1, 1048575, 1048576, 1073741823, 1073741824, 21713463136ull, 5368709120ull, 123456789012ull};
        for (uint64_t s : sizes) {
            cases++;
            char nb[128];
            int64_t nn = nat_lb_md_size(s, nb, sizeof nb);
            int64_t pn = lb_md_size((int64_t) s, sl_u8{(uint8_t *) b, 256});
            eq_str("lb_md_size", got_str(nn, nb), got_str(pn, b));
        }
        const uint64_t params[] = {0, 999999, 1000000, 123456789, 999999999, 1000000000, 7000000000ull, 21713463136ull};
        for (uint64_t p : params) {
            cases++;
            char nb[128];
            int64_t nn = nat_lb_md_params(p, nb, sizeof nb);
            int64_t pn = lb_md_params((int64_t) p, sl_u8{(uint8_t *) b, 256});
            eq_str("lb_md_params", got_str(nn, nb), got_str(pn, b));
        }
        struct { int np, ng, nd; } ts[] = {{512, 0, 0}, {0, 128, 0}, {512, 128, 0}, {512, 128, 4}, {0, 0, 0}, {1, 1, 1}};
        for (auto & t : ts) {
            cases++;
            char nb[128];
            int64_t nn = nat_lb_md_test(t.np, t.ng, t.nd, nb, sizeof nb);
            int64_t pn = lb_md_test(t.np, t.ng, t.nd, sl_u8{(uint8_t *) b, 256});
            eq_str("lb_md_test", got_str(nn, nb), got_str(pn, b));
        }
        struct { double a, s; } tsd[] = {{12.34, 0.56}, {1.0, 0.0}, {999.999, 10.005}, {0.0, 0.0}};
        for (auto & t : tsd) {
            cases++;
            char nb[128];
            int64_t nn = nat_lb_md_ts(t.a, t.s, nb, sizeof nb);
            int64_t pn = lb_md_ts(t.a, t.s, sl_u8{(uint8_t *) b, 256});
            eq_str("lb_md_ts", got_str(nn, nb), got_str(pn, b));
        }
        std::vector<std::vector<float>> sp = {{0, 0, 0}, {1, 0, 0}, {1, 0.5f, 0.25f}, {0.5f, 1, 0}, {1, 1, 0}};
        for (auto & s : sp) {
            cases++;
            std::vector<float>  pad(16, 0.0f);
            std::vector<double> d(16, 0.0);
            for (size_t i = 0; i < s.size(); i++) {
                pad[i] = s[i];
                d[i]   = (double) s[i];
            }
            char nb[128];
            int64_t nn = nat_lb_tensor_split(pad.data(), 16, nb, sizeof nb);
            sl_f64 pd{(double *) d.data(), (int64_t) d.size()};
            int64_t pn = lb_tensor_split(pd, 16, sl_u8{(uint8_t *) b, 256});
            eq_str("lb_tensor_split", got_str(nn, nb), got_str(pn, b));
        }
    }

    // ── parse_int_range (293-335) ─────────────────────────────────────────
    {
        const char * rs[] = {
            "512", "1-5", "1-9+2", "1-9*2", "1,3,5", "1-3,7", "", "5-3", "abc", "1,,2",
            "1-3+0", "1-9*1", "01-03", "1-3|2", "-1", "-3-2", "-1,2", "-1--5", "1-3,", " 1",
            "1-2-3", "0-0", "0-8+3", "2-100*2", "1-2,4-5+1",
        };
        for (const char * r : rs) {
            for (int neg = 0; neg < 2; neg++) {
                cases++;
                int64_t nv[64];
                int64_t nn = nat_lb_range(r, neg, nv, 64);
                int64_t pv[64];
                int64_t pn = lb_range(SL(r), neg, sl_i64{pv, 64});
                eq_int("lb_range.count", nn, pn);
                if (nn >= 0 && pn >= 0) {
                    for (int64_t i = 0; i < nn && i < pn; i++) {
                        eq_int("lb_range.value", nv[i], pv[i]);
                    }
                }
            }
        }
    }

    // ── cmd_params_defaults (380-423) ─────────────────────────────────────
    {
        int64_t nv[28];
        int64_t nn = nat_lb_defaults(nv, 28);
        int64_t pv[28];
        int64_t pn = lb_defaults(sl_i64{pv, 28});
        eq_int("lb_defaults.count", nn, pn);
        for (int64_t i = 0; i < nn && i < pn; i++) {
            eq_int("lb_defaults.value", nv[i], pv[i]);
        }
        cases++;
        eq_str("lb_default_cpu_mask", nat_lb_default_cpu_mask(),
               got_str(lb_default_cpu_mask(sl_u8{(uint8_t *) b, 256}), b));
    }

    printf("FG637_LB cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
