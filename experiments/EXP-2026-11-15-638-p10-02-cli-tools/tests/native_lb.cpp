// Native arm of the P10-02 S1 gate: the real llama-bench translation unit from
// the base-commit canon, included so the file's static decisions are callable.
//
// The tool's `main` is renamed and dropped by --gc-sections; only the functions
// below are reachable. The four stubs exist because the translation unit's
// static initialisers read host facts (build id, device count, CPU thread
// count) that the gate does not exercise:
//   llama_commit / llama_build_number  - test::build_commit initialisers
//   common_cpu_get_num_math            - cmd_params_defaults.n_threads
//   llama_max_devices                  - cmd_params_defaults.tensor_split
//
// The row text of the markdown printer (size/params/test/t/s) has no function
// of its own: print_test needs a `test` object, whose constructor needs a
// model. Those four rows are transcribed below from lines 2065-2105.
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#define main llama_bench_bench_main
#include "tools/llama-bench/llama-bench.cpp"
#undef main

const char * llama_commit(void) {
    return "0000000";
}
int llama_build_number(void) {
    return 0;
}
int32_t common_cpu_get_num_math() {
    return 8;
}
// Reached only by the native GGML_ABORT guard on an out-of-range enum, which
// the gate never passes.
void ggml_abort(const char * file, int line, const char * fmt, ...) {
    fprintf(stderr, "ggml_abort at %s:%d: %s\n", file, line, fmt);
    abort();
}
size_t llama_max_devices(void) {
    return 16;
}

extern "C" {

int nat_lb_out_from(const char * s) {
    output_formats f;
    return output_format_from_str(s, f) ? (int) f : -1;
}

const char * nat_lb_out_str(int v) {
    return output_format_str((output_formats) v);
}

const char * nat_lb_split_str(int v) {
    return split_mode_str((llama_split_mode) v);
}

const char * nat_lb_lazy_str(int v) {
    return lazy_mode_str((llama_lazy_mode) v);
}

uint64_t nat_lb_avg_u64(const uint64_t * v, int64_t n) {
    return avg(std::vector<uint64_t>(v, v + n));
}

uint64_t nat_lb_stdev_u64(const uint64_t * v, int64_t n) {
    return stdev(std::vector<uint64_t>(v, v + n));
}

// test::get_ts: 1e9 * (n_prompt + n_gen) / sample_ns, one value per sample.
static std::vector<double> ts_of(const uint64_t * v, int64_t n, int64_t ntok) {
    std::vector<double> ts;
    for (int64_t i = 0; i < n; i++) {
        ts.push_back(1e9 * ntok / v[i]);
    }
    return ts;
}

double nat_lb_avg_ts(const uint64_t * v, int64_t n, int64_t ntok) {
    return avg(ts_of(v, n, ntok));
}

double nat_lb_stdev_ts(const uint64_t * v, int64_t n, int64_t ntok) {
    return stdev(ts_of(v, n, ntok));
}

int nat_lb_field_type(const char * f) {
    return (int) test::get_field_type(f);
}

int64_t nat_lb_escape_json(const char * s, char * out, int64_t cap) {
    std::string e = escape_json(s);
    int64_t n = (int64_t) e.size();
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, e.data(), (size_t) n);
    out[n] = 0;
    return n;
}

int64_t nat_lb_json_value(const char * f, const char * v, char * out, int64_t cap) {
    std::string e = format_json_value(f, v);
    int64_t n = (int64_t) e.size();
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, e.data(), (size_t) n);
    out[n] = 0;
    return n;
}

int64_t nat_lb_csv(const char * s, char * out, int64_t cap) {
    std::string e = csv_printer::escape_csv(s);
    int64_t n = (int64_t) e.size();
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, e.data(), (size_t) n);
    out[n] = 0;
    return n;
}

int nat_lb_md_width(const char * f) {
    return markdown_printer::get_field_width(f);
}

const char * nat_lb_md_name(const char * f) {
    static std::string s;
    s = markdown_printer::get_field_display_name(f);
    return s.c_str();
}

const char * nat_lb_sql_type(const char * f) {
    static std::string s;
    s = sql_printer::get_sql_field_type(f);
    return s.c_str();
}

int64_t nat_lb_md_size(uint64_t bytes, char * out, int64_t cap) {
    char buf[128];
    if (bytes < 1024ull * 1024 * 1024) {
        snprintf(buf, sizeof buf, "%.2f MiB", bytes / 1024.0 / 1024.0);
    } else {
        snprintf(buf, sizeof buf, "%.2f GiB", bytes / 1024.0 / 1024.0 / 1024.0);
    }
    int64_t n = (int64_t) strlen(buf);
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, buf, (size_t) n);
    out[n] = 0;
    return n;
}

int64_t nat_lb_md_params(uint64_t p, char * out, int64_t cap) {
    char buf[128];
    if (p < 1000ull * 1000 * 1000) {
        snprintf(buf, sizeof buf, "%.2f M", p / 1e6);
    } else {
        snprintf(buf, sizeof buf, "%.2f B", p / 1e9);
    }
    int64_t n = (int64_t) strlen(buf);
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, buf, (size_t) n);
    out[n] = 0;
    return n;
}

int64_t nat_lb_md_test(int np, int ng, int nd, char * out, int64_t cap) {
    char buf[128];
    if (np > 0 && ng == 0) {
        snprintf(buf, sizeof buf, "pp%d", np);
    } else if (ng > 0 && np == 0) {
        snprintf(buf, sizeof buf, "tg%d", ng);
    } else {
        snprintf(buf, sizeof buf, "pp%d+tg%d", np, ng);
    }
    if (nd > 0) {
        int len = (int) strlen(buf);
        snprintf(buf + len, sizeof buf - len, " @ d%d", nd);
    }
    int64_t n = (int64_t) strlen(buf);
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, buf, (size_t) n);
    out[n] = 0;
    return n;
}

int64_t nat_lb_md_ts(double avg_ts, double sd_ts, char * out, int64_t cap) {
    char buf[128];
    snprintf(buf, sizeof buf, "%.2f \xC2\xB1 %.2f", avg_ts, sd_ts);
    int64_t n = (int64_t) strlen(buf);
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, buf, (size_t) n);
    out[n] = 0;
    return n;
}

int64_t nat_lb_tensor_split(const float * v, int ndev, char * out, int64_t cap) {
    std::string s;
    int max_nonzero = 0;
    for (int i = 0; i < ndev; i++) {
        if (v[i] > 0) {
            max_nonzero = i;
        }
    }
    for (int i = 0; i <= max_nonzero; i++) {
        char buf[32];
        snprintf(buf, sizeof buf, "%.2f", v[i]);
        s += buf;
        if (i < max_nonzero) {
            s += "/";
        }
    }
    int64_t n = (int64_t) s.size();
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, s.data(), (size_t) n);
    out[n] = 0;
    return n;
}

// 0 ok, -1 invalid range format, -2 invalid range, -3 out_of_range.
int64_t nat_lb_range(const char * s, int allow_neg, int64_t * out, int64_t cap) {
    try {
        std::vector<int> v = parse_int_range(s, allow_neg != 0);
        int64_t n = (int64_t) v.size();
        for (int64_t i = 0; i < n && i < cap; i++) {
            out[i] = v[(size_t) i];
        }
        return n;
    } catch (const std::out_of_range &) {
        return -3;
    } catch (const std::invalid_argument & e) {
        return std::string(e.what()) == "invalid range" ? -2 : -1;
    }
}

// Scalar fields of cmd_params_defaults, in the order the Goose module names.
int64_t nat_lb_defaults(int64_t * out, int64_t cap) {
    if (cap < 28) {
        return -1;
    }
    out[0]  = cmd_params_defaults.n_prompt.front();
    out[1]  = cmd_params_defaults.n_gen.front();
    out[2]  = cmd_params_defaults.n_depth.front();
    out[3]  = cmd_params_defaults.n_batch.front();
    out[4]  = cmd_params_defaults.n_ubatch.front();
    out[5]  = cmd_params_defaults.poll.front();
    out[6]  = cmd_params_defaults.n_gpu_layers.front();
    out[7]  = cmd_params_defaults.n_cpu_moe.front();
    out[8]  = (int64_t) cmd_params_defaults.split_mode.front();
    out[9]  = (int64_t) cmd_params_defaults.load_mode.front();
    out[10] = (int64_t) cmd_params_defaults.lazy_mode.front();
    out[11] = cmd_params_defaults.main_gpu.front();
    out[12] = cmd_params_defaults.no_kv_offload.front() ? 1 : 0;
    out[13] = (int64_t) cmd_params_defaults.flash_attn.front();
    out[14] = cmd_params_defaults.embeddings.front() ? 1 : 0;
    out[15] = cmd_params_defaults.no_op_offload.front() ? 1 : 0;
    out[16] = cmd_params_defaults.no_host.front() ? 1 : 0;
    out[17] = (int64_t) cmd_params_defaults.fit_params_target.front();
    out[18] = (int64_t) cmd_params_defaults.fit_params_min_ctx.front();
    out[19] = cmd_params_defaults.reps;
    out[20] = cmd_params_defaults.delay;
    out[21] = (int64_t) cmd_params_defaults.output_format;
    out[22] = (int64_t) cmd_params_defaults.output_format_stderr;
    out[23] = cmd_params_defaults.verbose ? 1 : 0;
    out[24] = cmd_params_defaults.progress ? 1 : 0;
    out[25] = cmd_params_defaults.no_warmup ? 1 : 0;
    out[26] = (int64_t) cmd_params_defaults.type_k.front();
    out[27] = (int64_t) cmd_params_defaults.type_v.front();
    return 28;
}

const char * nat_lb_default_cpu_mask(void) {
    return cmd_params_defaults.cpu_mask.front().c_str();
}

} // extern "C"
