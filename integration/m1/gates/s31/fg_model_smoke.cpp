// Differential gate for the staged S3.1 slice (model-management decisions).
//
// One binary per build, one corpus, no model, no server, no child process, no GPU:
//   OFF: fg_model::* dispatch to the native transcriptions, the Goose object is
//        not linked.
//   ON : fg_model::* dispatch into goose/server/{model-manager,hot-reload,
//        connectors}.goose (mg_/hr_/cn_) through common/fg-goose-model-bridge.cpp.
// In both builds the oracle is the *_native transcription in the same process.
//
//   ./fg-model-smoke          the gate, per-section counts, non-zero exit on failure
//   ./fg-model-smoke --dump   every dispatched result, for the OFF vs ON text diff
//
// The corpus is synthetic manager state: statuses, exit codes, names, LRU stamps,
// preset option maps, PATH hits and bind hosts. Everything is integer and string
// policy, so equality is exact.
#include "fg-model-decide.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int cases = 0;
static int values = 0;
static int fails = 0;
static bool dump_mode = false;

static void eq(const char * name, const char * field, long long want, long long got) {
    ++values;
    if (want != got) {
        ++fails;
        std::printf("[FAIL] %s/%s: want %lld got %lld\n", name, field, want, got);
    }
    if (dump_mode) {
        std::printf("%-24s %-10s %lld\n", name, field, got);
    }
}

static void eqs(const char * name, const char * field, const std::string & want,
                const std::string & got) {
    ++values;
    std::string w, g;
    for (unsigned char c : want) w += (c == '\n') ? std::string("|") : std::string(1, (char) c);
    for (unsigned char c : got) g += (c == '\n') ? std::string("|") : std::string(1, (char) c);
    if (w != g) {
        ++fails;
        std::printf("[FAIL] %s/%s: want \"%s\" got \"%s\"\n", name, field, w.c_str(), g.c_str());
    }
    if (dump_mode) {
        std::printf("%-24s %-10s \"%s\"\n", name, field, g.c_str());
    }
}

static void begin(const char * name) {
    ++cases;
    if (dump_mode) {
        std::printf("-- %s\n", name);
    }
}

// ── decision 1: the status predicates ---------------------------------------

static void gate_pred() {
    struct C {
        const char * name;
        int status;
        long long code;
    };
    const C cs[] = {
        {"pred_downloading", 0, 0},  {"pred_incomplete", 1, 0},
        {"pred_downloaded", 2, 0},   {"pred_unloaded_ok", 3, 0},
        {"pred_unloaded_fail", 3, 1}, {"pred_unloaded_neg", 3, -1},
        {"pred_loading", 4, 0},      {"pred_loaded", 5, 0},
        {"pred_sleeping", 6, 0},     {"pred_loaded_code", 5, 7},
        {"pred_unknown", 9, 0},      {"pred_unknown_code", 9, 5},
        {"pred_sleeping_code", 6, 3}, {"pred_loading_fail", 4, 2},
    };
    for (const C & c : cs) {
        begin(c.name);
        const fg_model::pred_t nat = fg_model::pred_native(c.status, c.code);
        const fg_model::pred_t got = fg_model::pred(c.status, c.code);
        eq(c.name, "ready", nat.ready, got.ready);
        eq(c.name, "running", nat.running, got.running);
        eq(c.name, "ready_or_sleep", nat.ready_or_sleep, got.ready_or_sleep);
        eq(c.name, "failed", nat.failed, got.failed);
    }
}

// ── decision 2: the unload table --------------------------------------------

static void gate_unload() {
    struct C {
        const char * name;
        int mapped;
        int status;
    };
    const C cs[] = {
        {"unload_unmapped", 0, 0},        {"unload_unmapped_loaded", 0, 5},
        {"unload_downloading", 1, 0},     {"unload_incomplete", 1, 1},
        {"unload_downloaded", 1, 2},      {"unload_unloaded", 1, 3},
        {"unload_loading", 1, 4},         {"unload_loaded", 1, 5},
        {"unload_sleeping", 1, 6},        {"unload_unknown", 1, 8},
        {"unload_unmapped_loading", 0, 4}, {"unload_unmapped_unknown", 0, 9},
        {"unload_loaded_again", 1, 5},    {"unload_downloading_again", 1, 0},
    };
    for (const C & c : cs) {
        begin(c.name);
        const fg_model::unload_t nat = fg_model::unload_action_native(c.mapped, c.status);
        const fg_model::unload_t got = fg_model::unload_action(c.mapped, c.status);
        eq(c.name, "action", nat.action, got.action);
        eq(c.name, "send_exit", nat.send_exit, got.send_exit);
        eq(c.name, "terminate", nat.terminate, got.terminate);
    }
}

// ── decision 3: the LRU victim ----------------------------------------------

static void victim_case(const char * name, const std::vector<fg_model::row_t> & rows) {
    begin(name);
    const long long nat = fg_model::victim_native(rows);
    const long long got = fg_model::victim(rows);
    eq(name, "victim", nat, got);
    const std::string nat_name = nat >= 0 ? rows[(size_t) nat].name : std::string("<none>");
    const std::string got_name = got >= 0 ? rows[(size_t) got].name : std::string("<none>");
    eqs(name, "victim_name", nat_name, got_name);
}

static void gate_victim() {
    using fg_model::row_t;
    const int L = fg_model::kLoaded, S = fg_model::kSleeping, U = fg_model::kUnloaded,
              G = fg_model::kLoading;

    victim_case("victim_empty", {});

    victim_case("victim_one", {{"a", L, 0, 10, 0, 0}});

    victim_case("victim_two_order", {{"b", L, 0, 20, 0, 0}, {"a", L, 0, 10, 0, 0}});

    // tie on last_used: the first row in name order wins
    victim_case("victim_tie", {{"beta", L, 0, 5, 0, 0}, {"alpha", L, 0, 5, 0, 0},
                               {"gamma", L, 0, 5, 0, 0}});

    // busy, stopping, queued and not-ready rows are all skipped
    victim_case("victim_busy", {{"a", L, 1, 1, 0, 0}, {"b", L, 0, 2, 1, 0},
                                {"c", L, 0, 3, 0, 1}, {"d", U, 0, 4, 0, 0},
                                {"e", G, 0, 5, 0, 0}, {"f", S, 0, 6, 0, 0}});

    victim_case("victim_all_skipped", {{"a", L, 3, 1, 0, 0}, {"b", U, 0, 2, 0, 0}});

    victim_case("victim_sleeping", {{"a", L, 0, 30, 0, 0}, {"b", S, 0, 20, 0, 0},
                                    {"c", S, 0, 40, 0, 0}});

    victim_case("victim_map_order", {{"zeta", L, 0, 100, 0, 0}, {"alpha", L, 0, 90, 0, 0},
                                     {"mu", L, 0, 95, 0, 0}});

    victim_case("victim_negative_stamp", {{"a", L, 0, -5, 0, 0}, {"b", L, 0, 5, 0, 0}});

    victim_case("victim_zero_stamp", {{"a", L, 0, 0, 0, 0}, {"b", L, 0, 1, 0, 0}});

    victim_case("victim_mixed_status", {{"a", L, 0, 7, 0, 0}, {"b", S, 0, 3, 0, 0},
                                        {"c", G, 0, 1, 0, 0}, {"d", U, 0, 0, 0, 0}});

    victim_case("victim_busy_newest", {{"a", L, 2, 10, 0, 0}, {"b", L, 0, 50, 0, 0},
                                       {"c", L, 0, 60, 0, 0}});

    // unique names, stamps given newest-first: the walk still picks the oldest
    victim_case("victim_reverse_stamps", {{"a", L, 0, 20, 0, 0}, {"b", L, 0, 10, 0, 0},
                                          {"c", L, 0, 30, 0, 0}});    victim_case("victim_wide", {{"n09", L, 0, 9, 0, 0}, {"n08", L, 0, 8, 0, 0},
                                {"n07", L, 0, 9, 0, 0}, {"n06", S, 0, 6, 0, 0},
                                {"n05", L, 0, 5, 1, 0}, {"n04", L, 0, 4, 0, 0},
                                {"n03", G, 0, 3, 0, 0}, {"n02", L, 0, 2, 0, 0}});
}

// ── decision 4: the preset normalisation ------------------------------------

static void gate_preset() {
    const char * cs[] = {
        "preset_empty",
        "preset_model_only",
        "preset_host_only",
        "preset_port_only",
        "preset_alias_only",
        "preset_colors_only",
        "preset_all_four",
        "preset_keep_two",
        "preset_unsorted_keep",
        "preset_alias_value",
        "preset_value_with_eq",
        "preset_long_value",
    };
    const std::vector<std::string> inp = {
        "",
        "LLAMA_ARG_MODEL=/m/a.gguf",
        "LLAMA_ARG_HOST=0.0.0.0",
        "LLAMA_ARG_PORT=8080",
        "LLAMA_ARG_ALIAS=a,b",
        "LLAMA_ARG_LOG_COLORS=on",
        "LLAMA_ARG_HOST=127.0.0.1\nLLAMA_ARG_PORT=1\nLLAMA_ARG_ALIAS=x,y\nLLAMA_ARG_LOG_COLORS=off",
        "LLAMA_ARG_MODEL=/m/a.gguf\nLLAMA_ARG_NGL=99",
        "LLAMA_ARG_NGL=99\nLLAMA_ARG_CTX=4096\nLLAMA_ARG_HOST=::\nLLAMA_ARG_TAGS=fast",
        "LLAMA_ARG_ALIAS=a,b\nLLAMA_ARG_CTX=4096\nLLAMA_ARG_LOG_COLORS=on",
        "LLAMA_ARG_CHAT_TEMPLATE=hello=world\nLLAMA_ARG_CTX=2048",
        "LLAMA_ARG_MODEL=/models/a-very-long-model-name-2026-11-15.gguf\nLLAMA_ARG_NGL=999",
    };
    for (size_t i = 0; i < inp.size(); ++i) {
        begin(cs[i]);
        eqs(cs[i], "text", fg_model::preset_norm_native(inp[i]), fg_model::preset_norm(inp[i]));
    }
}

// ── decision 5: the reload's unload set -------------------------------------

static void unload_set_case(const char * name, const std::vector<fg_model::model_t> & cur,
                            const std::vector<fg_model::model_t> & next) {
    begin(name);
    const fg_model::unload_set_t nat = fg_model::unload_set_native(cur, next);
    const fg_model::unload_set_t got = fg_model::unload_set(cur, next);

    auto flat = [](const fg_model::unload_set_t & s) {
        std::string o = std::to_string(s.rows.size()) + ":";
        for (size_t i = 0; i < s.rows.size(); ++i) {
            if (i) o += ",";
            o += std::to_string(s.rows[i]) + "r" + std::to_string(s.reasons[i]);
        }
        return o;
    };
    eqs(name, "set", flat(nat), flat(got));
    eq(name, "count", (long long) nat.rows.size(), (long long) got.rows.size());
}

static void gate_unload_set() {
    using fg_model::model_t;
    const int L = fg_model::kLoaded, S = fg_model::kSleeping, U = fg_model::kUnloaded,
              G = fg_model::kLoading, D = fg_model::kDownloading, W = fg_model::kDownloaded;

    // a new preset entry: the reload only reads its options, status is irrelevant
    auto N = [](const char * n, const char * o) {
        model_t m;
        m.name = n;
        m.status = fg_model::kUnloaded;
        m.options = o;
        return m;
    };

    // rows are given in insertion order, deliberately not sorted
    const std::vector<model_t> base = {
        {"zeta", L, "LLAMA_ARG_MODEL=/m/z.gguf\nLLAMA_ARG_HOST=0.0.0.0\nLLAMA_ARG_NGL=99"},
        {"alpha", L, "LLAMA_ARG_MODEL=/m/a.gguf\nLLAMA_ARG_PORT=8080\nLLAMA_ARG_NGL=99"},
        {"beta", S, "LLAMA_ARG_MODEL=/m/b.gguf"},
        {"gamma", U, "LLAMA_ARG_MODEL=/m/g.gguf"},
        {"delta", G, "LLAMA_ARG_MODEL=/m/d.gguf\nLLAMA_ARG_NGL=42"},
        {"omega", D, "LLAMA_ARG_MODEL=/m/o.gguf"},
        {"sigma", W, "LLAMA_ARG_MODEL=/m/s.gguf"},
    };

    unload_set_case("set_empty", {}, {});

    unload_set_case("set_no_change", base, base);

    unload_set_case("set_all_removed", base, {});

    // only the injected HOST/PORT differ: nothing goes down
    unload_set_case("set_host_port_only",
                    {{"a", L, "LLAMA_ARG_MODEL=/m/a.gguf\nLLAMA_ARG_HOST=0.0.0.0\nLLAMA_ARG_PORT=8080"}},
                    {N("a", "LLAMA_ARG_MODEL=/m/a.gguf\nLLAMA_ARG_HOST=127.0.0.1\nLLAMA_ARG_PORT=1")});

    // alias and log colours are injected too
    unload_set_case(
        "set_alias_colors_only",
        {{"a", L, "LLAMA_ARG_MODEL=/m/a.gguf\nLLAMA_ARG_ALIAS=x\nLLAMA_ARG_LOG_COLORS=on"}},
        {N("a", "LLAMA_ARG_MODEL=/m/a.gguf\nLLAMA_ARG_ALIAS=y\nLLAMA_ARG_LOG_COLORS=off")});

    // a real option change
    unload_set_case("set_ngl_changed", {{"a", L, "LLAMA_ARG_MODEL=/m/a.gguf\nLLAMA_ARG_NGL=99"}},
                    {N("a", "LLAMA_ARG_MODEL=/m/a.gguf\nLLAMA_ARG_NGL=42")});

    // a new model appears, the existing one stays
    unload_set_case("set_added_only", {{"a", L, "LLAMA_ARG_MODEL=/m/a.gguf"}},
                    {N("a", "LLAMA_ARG_MODEL=/m/a.gguf"), N("b", "LLAMA_ARG_MODEL=/m/b.gguf")});

    // not-running rows are never in the set, whatever the preset says
    unload_set_case("set_not_running",
                    {{"a", U, "LLAMA_ARG_NGL=1"},
                     {"b", D, "LLAMA_ARG_NGL=2"},
                     {"c", W, "LLAMA_ARG_NGL=3"},
                     {"d", L, "LLAMA_ARG_NGL=4"}},
                    {N("d", "LLAMA_ARG_NGL=5")});

    // name order decides the emission order, not insertion order
    unload_set_case("set_name_order",
                    {{"zeta", L, "X=1"}, {"alpha", L, "X=1"}, {"mu", L, "X=1"}},
                    {N("alpha", "X=2"), N("mu", "X=2"), N("zeta", "X=2")});

    unload_set_case("set_mixed", base,
                    {N("zeta", "LLAMA_ARG_MODEL=/m/z.gguf\nLLAMA_ARG_NGL=99"),
                     {"beta", S, "LLAMA_ARG_MODEL=/m/b.gguf"},
                     {"gamma", U, "LLAMA_ARG_MODEL=/m/g.gguf"}});

    unload_set_case("set_loading_changed", {{"d", G, "LLAMA_ARG_CTX=1"}},
                    {N("d", "LLAMA_ARG_CTX=2")});

    unload_set_case("set_sleeping_removed", {{"s", S, "LLAMA_ARG_CTX=1"}}, {});

    unload_set_case("set_one_of_many", {{"a", L, "K=1"}, {"b", L, "K=1"}, {"c", L, "K=1"}},
                    {N("a", "K=1"), N("b", "K=1"), N("c", "K=2")});
}

// ── decision 6: the connector target ----------------------------------------

static void conn_case(const char * name, int sibling, const std::vector<int> & dirs,
                      const std::vector<std::string> & hosts) {
    begin(name);
    const fg_model::connector_t nat = fg_model::connector_target_native(sibling, dirs, hosts);
    const fg_model::connector_t got = fg_model::connector_target(sibling, dirs, hosts);
    eq(name, "binary", nat.binary, got.binary);
    eqs(name, "host", nat.host, got.host);
}

static void gate_connector() {
    conn_case("conn_sibling", 1, {}, {"0.0.0.0"});
    conn_case("conn_path_first", 0, {1, 0, 0}, {"0.0.0.0"});
    conn_case("conn_path_second", 0, {0, 1, 0}, {"127.0.0.1"});
    conn_case("conn_path_third", 0, {0, 0, 1}, {"::"});
    conn_case("conn_none", 0, {0, 0, 0}, {"0.0.0.0"});
    conn_case("conn_no_dirs", 0, {}, {"0.0.0.0"});
    conn_case("conn_sock_only", 1, {}, {"/tmp/server.sock"});
    conn_case("conn_sock_then_v4", 1, {}, {"/tmp/a.sock", "192.168.1.5"});
    conn_case("conn_sock_then_wildcard", 1, {}, {"/tmp/a.sock", "0.0.0.0"});
    conn_case("conn_sock_then_any", 1, {}, {"/tmp/a.sock", "::", "0.0.0.0"});
    conn_case("conn_ipv6_literal", 1, {}, {"fe80::1"});
    conn_case("conn_ipv6_bracketed", 1, {}, {"[::1]"});
    conn_case("conn_empty_hosts", 1, {}, {});
    conn_case("conn_v4_plain", 0, {1}, {"10.0.0.7"});
    conn_case("conn_short_sock", 1, {}, {".sock", "localhost"});
    conn_case("conn_name_host", 1, {}, {"localhost"});

    // the two fields are independent: a PATH miss does not change the address
    conn_case("conn_none_but_sock", 0, {0}, {"/tmp/x.sock", "0.0.0.0"});
}

// ---------------------------------------------------------------------------

static void report(const char * name, int c0, int v0, int f0) {
    std::printf("FG643_MODEL %-12s cases=%-3d values=%-4d failures=%d\n", name, cases - c0,
                values - v0, fails - f0);
}

int main(int argc, char ** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--dump") == 0) {
        dump_mode = true;
    }
#ifdef FG_GOOSE_MODELS
    const char * mode = "goose";
#else
    const char * mode = "native";
#endif

    int c0 = cases, v0 = values, f0 = fails;
    gate_pred();
    report("pred", c0, v0, f0);

    c0 = cases; v0 = values; f0 = fails;
    gate_unload();
    report("unload", c0, v0, f0);

    c0 = cases; v0 = values; f0 = fails;
    gate_victim();
    report("victim", c0, v0, f0);

    c0 = cases; v0 = values; f0 = fails;
    gate_preset();
    report("preset", c0, v0, f0);

    c0 = cases; v0 = values; f0 = fails;
    gate_unload_set();
    report("unload_set", c0, v0, f0);

    c0 = cases; v0 = values; f0 = fails;
    gate_connector();
    report("connector", c0, v0, f0);

    std::printf("FG643_MODEL mode=%s cases=%d values=%d failures=%d goose_calls=%lld\n", mode,
                cases, values, fails, fg_model::goose_calls());
    return fails == 0 ? 0 : 1;
}
