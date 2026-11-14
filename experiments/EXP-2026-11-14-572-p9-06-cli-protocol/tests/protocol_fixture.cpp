// Gate for the CLI line protocol (P9-06 S2).
//
// The expected stdout is built with snprintf from the SAME format strings the
// carrier prints (runtime/cli/main.cpp:103, 107-118, 249-255, 310, 327, 337-350),
// so the comparison is libc printf against the Goose emitter, not the emitter
// against itself. The stdin reader is checked through the parsed command fields.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>

#include "g.h"

extern "C" double gs_json_strtod(sl_u8 s) {
    std::string tmp((const char *) s.data, (size_t) s.len);
    return std::strtod(tmp.c_str(), nullptr);
}

static int cases = 0, checks = 0, fails = 0;

static sl_u8 S(const char * s) {
    return sl_u8{(uint8_t *) s, (int64_t) strlen(s)};
}

static std::string vis(const std::string & s, size_t max = 200) {
    std::string r;
    size_t n = 0;
    for (unsigned char c : s) {
        if (n++ > max) { r += "..."; break; }
        if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c == '"') r += "\\\"";
        else if (c >= 32 && c < 127) r.push_back((char) c);
        else { char b[8]; snprintf(b, sizeof b, "\\x%02X", c); r += b; }
    }
    return r;
}

// ── the scripted fake engine: one token's metrics, mirroring TokenMetrics.
struct Tok {
    double   wall_ms, io_ms, compute_ms, mgmt_ms, stall_ms, read_mb, hit_pct, cpu_ms, dense_frac;
    long long majflt;
    std::string reasoning, text;
};

struct Script {
    long long id = 0;
    bool      cancelled = false;
    std::vector<Tok> toks;
};

// main.cpp:44-76 json_escape, so the oracle applies the same wire escaping.
static std::string jesc(const std::string & in) {
    std::string o;
    for (char ch : in) {
        unsigned char c = (unsigned char) ch;
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) {
                    char b[8];
                    snprintf(b, sizeof b, "\\u%04x", c);
                    o += b;
                } else {
                    o += ch;
                }
        }
    }
    return o;
}

// ── oracle lines, built with the carrier's format strings.
static std::string o_begin(long long id) {
    char b[128];
    snprintf(b, sizeof b, "BMOE_BEGIN {\"id\":%d}\n", (int) id);
    return b;
}

static std::string o_load(double mb, double ms) {
    char b[128];
    snprintf(b, sizeof b, "BMOE_LOAD {\"mb\":%.2f,\"ms\":%.1f}\n", mb, ms);
    return b;
}

static std::string o_progress(const Tok & t, long long step, long long steps, bool reset,
                              const std::string & dr, const std::string & dt) {
    char b[1024];
    snprintf(b, sizeof b,
             "BMOE_PROGRESS {\"step\":%d,\"steps\":%d,\"wall_ms\":%.1f,\"io_ms\":%.1f,"
             "\"compute_ms\":%.1f,\"mgmt_ms\":%.1f,\"stall_ms\":%.1f,\"read_mb\":%.2f,"
             "\"cache_hit_pct\":%.1f,\"majflt\":%llu,\"cpu_ms\":%.1f,\"dense_resident_frac\":%.3f,"
             "%s\"delta_reasoning\":\"%s\",\"delta_text\":\"%s\"}\n",
             (int) step, (int) steps, t.wall_ms, t.io_ms, t.compute_ms, t.mgmt_ms, t.stall_ms,
             t.read_mb, t.hit_pct, (unsigned long long) t.majflt, t.cpu_ms, t.dense_frac,
             reset ? "\"reset\":1," : "", jesc(dr).c_str(), jesc(dt).c_str());
    return b;
}

static std::string o_done(const Script & s, const std::vector<Tok> & toks) {
    char b[2048];
    const double tok_s = 42.125, prefill_s = 0.5, load_s = 1.25, hit = 76.0;
    const double comp = 3.5, io = 1.0, res = 512, budget = 2048, read = 12.5;
    const double stall = 0.25, mgmt = 0.125, majflt = 0.5, cpu = 8.25;
    const double pcpu = 0.75, pread = 4.5, pio = 0.125, pstall = 0.0625, pmgmt = 0.03125;
    const double demand = 3.5, mtp_s = 0.5, loop = 0.015625;
    const int n_prompt = 17, n_past = 17 + (int) toks.size();
    snprintf(b, sizeof b,
             "BMOE_DONE {\"id\":%d,\"cancelled\":%s,\"tokens\":%d,\"tok_s\":%.3f,\"prefill_s\":%.3f,"
             "\"prefill_tps\":%.2f,\"load_s\":%.3f,\"cache_hit_pct\":%.1f,\"n_prompt\":%d,\"n_past\":%d,"
             "\"compute_s_tok\":%.4f,\"io_s_tok\":%.4f,\"cache_resident_mib\":%.0f,\"cache_budget_mib\":%.0f,"
             "\"read_mib\":%.1f,\"stall_s_tok\":%.4f,\"mgmt_s_tok\":%.4f,\"majflt_tok\":%.2f,\"cpu_s_tok\":%.4f,"
             "\"prefill_cpu_s\":%.3f,\"prefill_read_mib\":%.1f,\"prefill_io_s\":%.3f,"
             "\"prefill_stall_s\":%.3f,\"prefill_mgmt_s\":%.3f,"
             "\"token_demand_mib\":%.1f,\"mtp_drafted\":%lld,\"mtp_accepted\":%lld,\"mtp_decodes\":%lld,"
             "\"mtp_draft_s_tok\":%.4f,\"drafted_steps\":%lld,\"loop_overhead_s_tok\":%.4f,"
             "\"reasoning\":\"%s\",\"text\":\"%s\"}\n",
             (int) s.id, s.cancelled ? "true" : "false", (int) toks.size(), tok_s, prefill_s,
             prefill_s > 0 ? n_prompt / prefill_s : 0.0, load_s, hit, n_prompt, n_past, comp, io,
             res, budget, read, stall, mgmt, majflt, cpu, pcpu, pread, pio, pstall, pmgmt, demand,
             3LL, 1LL, 2LL, mtp_s, 4LL, loop,
             toks.empty() ? "" : jesc(toks.back().reasoning).c_str(),
             toks.empty() ? "" : jesc(toks.back().text).c_str());
    return b;
}

static std::string o_error(long long id, bool fatal, const std::string & msg) {
    char b[512];
    snprintf(b, sizeof b, "BMOE_ERROR {\"id\":%d,\"fatal\":%s,\"msg\":\"%s\"}\n",
             (int) id, fatal ? "true" : "false", jesc(msg).c_str());
    return b;
}

// ── the port under test.
struct Env {
    std::vector<int64_t> st = std::vector<int64_t>(8, 0);
    std::vector<int64_t> pst = std::vector<int64_t>(4, 0);
    std::vector<uint8_t> prompt = std::vector<uint8_t>(1 << 16, 0);
    std::vector<uint8_t> snap = std::vector<uint8_t>(1 << 16, 0);
    std::vector<uint8_t> delta = std::vector<uint8_t>(1 << 16, 0);
    std::vector<uint8_t> out = std::vector<uint8_t>(1 << 16, 0);
};

static int64_t read_line(Env & e, const std::string & line, int64_t dnp, int64_t dth) {
    sl_u8 line_s{(uint8_t *) line.data(), (int64_t) line.size()};
    sl_u8 prompt_s{e.prompt.data(), (int64_t) e.prompt.size()};
    sl_i64 st_s{e.st.data(), (int64_t) e.st.size()};
    return cli_read_line(line_s, dnp, dth, prompt_s, st_s);
}

static std::string emit_load(Env & e, double mb, double ms) {
    sl_u8 o_s{e.out.data(), (int64_t) e.out.size()};
    int64_t len = cli_emit_load(mb, ms, o_s);
    return std::string((const char *) e.out.data(), (size_t) len);
}

static std::string emit_progress(Env & e, const Tok & t, long long step, long long steps,
                                 const char * dr, int64_t drl, const char * dt, int64_t dtl,
                                 bool reset) {
    int64_t n[3] = {step, steps, t.majflt};
    double m[9] = {t.wall_ms, t.io_ms, t.compute_ms, t.mgmt_ms, t.stall_ms, t.read_mb,
                   t.hit_pct, t.cpu_ms, t.dense_frac};
    sl_i64 n_s{n, 3};
    sl_f64 m_s{m, 9};
    sl_u8 dr_s{(uint8_t *) dr, drl};
    sl_u8 dt_s{(uint8_t *) dt, dtl};
    sl_u8 o_s{e.out.data(), (int64_t) e.out.size()};
    int64_t len = cli_emit_progress(n_s, m_s, reset ? 1 : 0, dr_s, dt_s, o_s);
    return std::string((const char *) e.out.data(), (size_t) len);
}

static std::string emit_done(Env & e, const Script & s) {
    int64_t n[9] = {s.id, s.cancelled ? 1 : 0, (int64_t) s.toks.size(), 17,
                    17 + (int64_t) s.toks.size(), 3, 1, 2, 4};
    double m[22] = {42.125, 0.5, 34.0, 1.25, 76.0, 3.5, 1.0, 512, 2048, 12.5,
                    0.25, 0.125, 0.5, 8.25, 0.75, 4.5, 0.125, 0.0625, 0.03125, 3.5,
                    0.5, 0.015625};
    sl_i64 n_s{n, 9};
    sl_f64 m_s{m, 22};
    const std::string & reas = s.toks.empty() ? std::string() : s.toks.back().reasoning;
    const std::string & text = s.toks.empty() ? std::string() : s.toks.back().text;
    sl_u8 r_s{(uint8_t *) reas.data(), (int64_t) reas.size()};
    sl_u8 t_s{(uint8_t *) text.data(), (int64_t) text.size()};
    sl_u8 o_s{e.out.data(), (int64_t) e.out.size()};
    int64_t len = cli_emit_done(n_s, m_s, r_s, t_s, o_s);
    return std::string((const char *) e.out.data(), (size_t) len);
}

// One generation: BEGIN, per-token LOAD/PROGRESS (delta rules), DONE.
static std::string run_gen(Env & e, const Script & s, long long id) {
    std::string out = o_begin(id);
    e.pst[0] = 0;
    e.pst[1] = 0;
    for (size_t i = 0; i < s.toks.size(); i++) {
        const Tok & t = s.toks[i];
        if (t.read_mb > 0.0 || t.io_ms > 0.0) out += emit_load(e, t.read_mb, t.io_ms);

        sl_u8 r_s{(uint8_t *) t.reasoning.data(), (int64_t) t.reasoning.size()};
        sl_u8 x_s{(uint8_t *) t.text.data(), (int64_t) t.text.size()};
        sl_u8 snap_s{e.snap.data(), (int64_t) e.snap.size()};
        sl_u8 d_s{e.delta.data(), (int64_t) e.delta.size()};
        sl_i64 p_s{e.pst.data(), (int64_t) e.pst.size()};
        int64_t reset = cli_progress_delta(r_s, x_s, snap_s, p_s, d_s);
        const char * dr = (const char *) e.delta.data();
        const char * dt = dr + e.pst[2];
        std::string line = emit_progress(e, t, (long long) i + 1, (long long) s.toks.size(),
                                        dr, e.pst[2], dt, e.pst[3], reset != 0);
        out += line;
    }
    Script d = s;
    d.id = id;
    out += emit_done(e, d);
    return out;
}

struct Case {
    const char *             name;
    std::vector<std::string> lines;
    std::vector<Script>      gens;
};

static void run_case(const Case & c, const std::vector<std::string> & expected_lines,
                     const std::vector<std::string> & expected_prompts,
                     const std::vector<int> & expected_think,
                     const std::vector<int> & expected_np) {
    ++cases;
    Env e;
    std::string got;
    size_t gi = 0;
    std::vector<std::string> prompts;
    std::vector<int> thinks, nps;
    for (const std::string & line : c.lines) {
        int64_t kind = read_line(e, line, 64, 1);
        if (kind == 3) continue;
        if (kind == 1) continue; // cancel: no stdout
        if (kind == 2) break;    // close
        prompts.push_back(std::string((const char *) e.prompt.data(), (size_t) e.st[5]));
        thinks.push_back((int) e.st[3]);
        nps.push_back((int) e.st[2]);
        if (gi < c.gens.size()) {
            got += run_gen(e, c.gens[gi], (long long) e.st[1]);
            gi++;
        }
    }
    std::string want;
    for (const auto & l : expected_lines) want += l;

    std::string d;
    ++checks;
    if (got != want) d += " stdout";
    if (!expected_prompts.empty()) {
        ++checks;
        if (prompts != expected_prompts) d += " prompts";
    }
    if (!expected_think.empty()) {
        ++checks;
        if (thinks != expected_think) d += " think";
    }
    if (!expected_np.empty()) {
        ++checks;
        if (nps != expected_np) d += " n_predict";
    }
    if (d.empty()) {
        printf("CASE %2d OK   %s\n", cases, c.name);
    } else {
        ++fails;
        printf("CASE %2d FAIL %s%s\n", cases, c.name, d.c_str());
        if (got != want) {
            size_t i = 0;
            while (i < got.size() && i < want.size() && got[i] == want[i]) i++;
            size_t st = i > 60 ? i - 60 : 0;
            printf("  first diff at %zu  want=[%s]\n  got =[%s]\n", i,
                   vis(want.substr(st, 160)).c_str(), vis(got.substr(st, 160)).c_str());
        }
        if (!expected_prompts.empty() && prompts != expected_prompts) {
            for (size_t i = 0; i < prompts.size() && i < expected_prompts.size(); i++) {
                if (prompts[i] != expected_prompts[i]) {
                    printf("  prompt %zu want=[%s] got=[%s]\n", i,
                           vis(expected_prompts[i]).c_str(), vis(prompts[i]).c_str());
                }
            }
        }
    }
}

static Tok tok(double wall, double io, double comp, double mgmt, double stall, double read,
               double hit, double cpu, double dense, long long majflt,
               const char * reas, const char * text) {
    Tok t;
    t.wall_ms = wall; t.io_ms = io; t.compute_ms = comp; t.mgmt_ms = mgmt;
    t.stall_ms = stall; t.read_mb = read; t.hit_pct = hit; t.cpu_ms = cpu;
    t.dense_frac = dense; t.majflt = majflt; t.reasoning = reas; t.text = text;
    return t;
}

int main() {
    goose_init(0, 0);

    // 1: close only.
    run_case({"close only", {"{\"cmd\":\"close\"}"}, {}}, {}, {}, {}, {});

    // 2: EOF with no lines.
    run_case({"EOF, no input", {}, {}}, {}, {}, {}, {});

    // 3: garbage lines are skipped.
    run_case({"garbage lines skipped",
              {"hello", "{}", "{\"cmd\":\"nope\"}", "{\"cmd\":", "{\"nope\":1}"}, {}},
             {}, {}, {}, {});

    // 4: cancel is recognised and produces nothing.
    {
        Env e;
        int64_t kind = read_line(e, "{\"cmd\":\"cancel\"}", 64, 1);
        ++cases;
        ++checks;
        if (kind == 1) {
            printf("CASE %2d OK   cancel is kind 1\n", cases);
        } else {
            ++fails;
            printf("CASE %2d FAIL cancel kind=%lld\n", cases, (long long) kind);
        }
    }

    // 5: a full generation: BEGIN, two tokens, DONE.
    {
        Script s;
        s.id = 1;
        s.toks = {tok(9.5, 1.0, 3.0, 0.1, 0.0, 12.5, 76.0, 8.0, 1.0, 0, "", ""),
                  tok(8.0, 0.0, 4.0, 0.0, 0.0, 0.0, 76.0, 7.5, 1.0, 1, "", "Hi")};
        std::vector<std::string> exp = {
            o_begin(1),
            o_load(12.5, 1.0),
            o_progress(s.toks[0], 1, 2, false, "", ""),
            o_progress(s.toks[1], 2, 2, false, "", "Hi"),
            o_done(s, s.toks),
        };
        run_case({"generation, two tokens", {"{\"cmd\":\"generate\",\"id\":1,\"prompt\":\"hi\"}"},
                  {s}}, exp, {"hi"}, {1}, {64});
    }

    // 6: prompt escapes are decoded.
    run_case({"prompt escapes decoded",
              {"{\"cmd\":\"generate\",\"id\":2,\"prompt\":\"a\\\"b\\nc\\u0041\"}"}, {}},
             {}, {"a\"b\ncA"}, {}, {});

    // 7: think and n_predict overrides.
    {
        Script s;
        s.id = 3;
        s.toks = {tok(1.0, 0.0, 0.5, 0.0, 0.0, 0.0, -1.0, 0.5, 0.0, 0, "t", "x")};
        std::vector<std::string> exp = {o_begin(3), o_progress(s.toks[0], 1, 1, false, "t", "x"),
                                       o_done(s, s.toks)};
        run_case({"think false and n_predict",
                  {"{\"cmd\":\"generate\",\"id\":3,\"prompt\":\"x\",\"think\":false,\"n_predict\":5}"},
                  {s}}, exp, {"x"}, {0}, {5});
    }

    // 8: defaults for missing fields.
    {
        Env e;
        int64_t kind = read_line(e, "{\"cmd\":\"generate\"}", 64, 1);
        ++cases;
        ++checks;
        bool ok = kind == 0 && e.st[1] == 0 && e.st[2] == 64 && e.st[3] == 1 && e.st[4] == 1 &&
                  e.st[5] == 0;
        if (ok) {
            printf("CASE %2d OK   defaults for missing fields\n", cases);
        } else {
            ++fails;
            printf("CASE %2d FAIL defaults kind=%lld id=%lld np=%lld think=%lld clear=%lld len=%lld\n",
                   cases, (long long) kind, (long long) e.st[1], (long long) e.st[2],
                   (long long) e.st[3], (long long) e.st[4], (long long) e.st[5]);
        }
    }

    // 9: READY line.
    {
        Env e;
        char b[512];
        snprintf(b, sizeof b,
                 "BMOE_READY {\"load_s\":%.3f,\"arch\":\"%s\",\"n_ctx\":%d,\"think_ctl\":\"%s\","
                 "\"n_expert_used\":%d}\n", 1.25, "qwen3", 2048, "none", 8);
        sl_u8 o_s{e.out.data(), (int64_t) e.out.size()};
        int64_t len = cli_emit_ready(1.25, S("qwen3"), 2048, S("none"), 8, o_s);
        std::string got((const char *) e.out.data(), (size_t) len);
        ++cases;
        ++checks;
        if (got == b) {
            printf("CASE %2d OK   ready line\n", cases);
        } else {
            ++fails;
            printf("CASE %2d FAIL ready line\n  want=[%s]\n  got =[%s]\n", cases, vis(b).c_str(),
                   vis(got).c_str());
        }
    }

    // 10: LOAD only when the token read something.
    {
        Script s;
        s.id = 4;
        s.toks = {tok(2.0, 1.5, 0.25, 0.0, 0.0, 3.25, 50.0, 1.0, 0.5, 0, "", "a"),
                  tok(1.0, 0.0, 0.5, 0.0, 0.0, 0.0, 50.0, 0.5, 0.5, 0, "", "ab")};
        std::vector<std::string> exp = {
            o_begin(4),
            o_load(3.25, 1.5),
            o_progress(s.toks[0], 1, 2, false, "", "a"),
            o_progress(s.toks[1], 2, 2, false, "", "b"),
            o_done(s, s.toks),
        };
        run_case({"load only when bytes were read",
                  {"{\"cmd\":\"generate\",\"id\":4,\"prompt\":\"p\"}"}, {s}},
                 exp, {"p"}, {}, {});
    }

    // 11: deltas grow by suffix, no reset.
    {
        Script s;
        s.id = 5;
        s.toks = {tok(1.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 0, "", "hello wor"),
                  tok(1.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 0, "", "hello world")};
        std::vector<std::string> exp = {
            o_begin(5),
            o_progress(s.toks[0], 1, 2, false, "", "hello wor"),
            o_progress(s.toks[1], 2, 2, false, "", "ld"),
            o_done(s, s.toks),
        };
        run_case({"deltas grow by suffix",
                  {"{\"cmd\":\"generate\",\"id\":5,\"prompt\":\"p\"}"}, {s}}, exp, {}, {}, {});
    }

    // 12: reset when the closing tag moves text into reasoning.
    {
        Script s;
        s.id = 6;
        s.toks = {tok(1.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 0, "", "Hello wor"),
                  tok(1.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 0, "Hello world", "")};
        std::vector<std::string> exp = {
            o_begin(6),
            o_progress(s.toks[0], 1, 2, false, "", "Hello wor"),
            o_progress(s.toks[1], 2, 2, true, "Hello world", ""),
            o_done(s, s.toks),
        };
        run_case({"reset when text becomes reasoning",
                  {"{\"cmd\":\"generate\",\"id\":6,\"prompt\":\"p\"}"}, {s}}, exp, {}, {}, {});
    }

    // 13: cancel mid-generation, DONE carries cancelled:true.
    {
        Script s;
        s.id = 7;
        s.cancelled = true;
        s.toks = {tok(1.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 0, "", "partial")};
        std::vector<std::string> exp = {o_begin(7), o_progress(s.toks[0], 1, 1, false, "", "partial"),
                                       o_done(s, s.toks)};
        run_case({"cancel mid-generation",
                  {"{\"cmd\":\"generate\",\"id\":7,\"prompt\":\"p\"}", "{\"cmd\":\"cancel\"}"},
                  {s}}, exp, {"p"}, {}, {});
    }

    // 14: recoverable errors.
    {
        Env e;
        std::string out;
        for (const char * m : {"empty prompt", "prompt exceeds the session n_ctx by 3"}) {
            sl_u8 m_s{(uint8_t *) m, (int64_t) strlen(m)};
            int64_t fatal = cli_error_recoverable(m_s) == 1 ? 0 : 1;
            sl_u8 o_s{e.out.data(), (int64_t) e.out.size()};
            int64_t len = cli_emit_error(9, fatal, m_s, o_s);
            out += std::string((const char *) e.out.data(), (size_t) len);
        }
        std::string want = o_error(9, false, "empty prompt") +
                           o_error(9, false, "prompt exceeds the session n_ctx by 3");
        ++cases;
        ++checks;
        if (out == want) {
            printf("CASE %2d OK   recoverable errors\n", cases);
        } else {
            ++fails;
            printf("CASE %2d FAIL recoverable errors\n  want=[%s]\n  got =[%s]\n", cases,
                   vis(want).c_str(), vis(out).c_str());
        }
    }

    // 15: fatal error.
    {
        Env e;
        const char * m = "decode failed at step 7";
        sl_u8 m_s{(uint8_t *) m, (int64_t) strlen(m)};
        int64_t fatal = cli_error_recoverable(m_s) == 1 ? 0 : 1;
        sl_u8 o_s{e.out.data(), (int64_t) e.out.size()};
        int64_t len = cli_emit_error(10, fatal, m_s, o_s);
        std::string out((const char *) e.out.data(), (size_t) len);
        std::string want = o_error(10, true, m);
        ++cases;
        ++checks;
        if (out == want) {
            printf("CASE %2d OK   fatal error\n", cases);
        } else {
            ++fails;
            printf("CASE %2d FAIL fatal error\n  want=[%s]\n  got =[%s]\n", cases, vis(want).c_str(),
                   vis(out).c_str());
        }
    }

    // 16: close stops the session, the next line never runs.
    {
        Script s;
        s.id = 11;
        s.toks = {tok(1.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 0, "", "x")};
        run_case({"close stops the loop",
                  {"{\"cmd\":\"close\"}", "{\"cmd\":\"generate\",\"id\":11,\"prompt\":\"p\"}"},
                  {s}},
                 {}, {}, {}, {});
    }

    // 17: quotes and newlines are escaped on the wire.
    {
        Script s;
        s.id = 12;
        s.toks = {tok(1.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 0, "a\"b", "c\nd")};
        std::vector<std::string> exp = {
            o_begin(12),
            o_progress(s.toks[0], 1, 1, false, "a\"b", "c\nd"),
            o_done(s, s.toks),
        };
        run_case({"quotes and newlines are escaped",
                  {"{\"cmd\":\"generate\",\"id\":12,\"prompt\":\"p\"}"}, {s}},
                 exp, {}, {}, {});
    }

    printf("FG577_S2 cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
