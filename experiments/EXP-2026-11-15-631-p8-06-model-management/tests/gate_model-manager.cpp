// Differential gate for the model-manager slice (model-manager.goose).
//
// Every case runs twice: through the Goose export and through a transcription of
// the native code with its lines cited. The compared fields are the values the
// native functions return or write, never the logging around them.
#include "model-manager-gen.h"

#include "common.h"

static sl_u8 su8(UBuf & u) {
    return sl_u8{ u.data(), u.cap() };
}

static sl_i64 si64(IBuf & i) {
    return sl_i64{ i.data(), i.cap() };
}

static sl_i64 si64(std::vector<int64_t> & i) {
    return sl_i64{ i.data(), (int64_t) i.size() };
}

static sl_u8 su8(NameTable & t) {
    return sl_u8{ t.buf.data(), (int64_t) t.buf.size() };
}

// ── native side ────────────────────────────────────────────────────────────

// server-models.h status enum
enum {
    N_DOWNLOADING = 0,
    N_INCOMPLETE  = 1,
    N_DOWNLOADED  = 2,
    N_UNLOADED    = 3,
    N_LOADING     = 4,
    N_LOADED      = 5,
    N_SLEEPING    = 6,
};

// server-models.h 41-53
static std::string n_status_str(int s) {
    switch (s) {
        case N_DOWNLOADING: return "downloading";
        case N_INCOMPLETE:  return "incomplete";
        case N_DOWNLOADED:  return "downloaded";
        case N_UNLOADED:    return "unloaded";
        case N_LOADING:     return "loading";
        case N_LOADED:      return "loaded";
        case N_SLEEPING:    return "sleeping";
        default:            return "unknown";
    }
}

// server-models.h 55-63
static std::string n_source_str(int s) {
    switch (s) {
        case 0:  return "preset";
        case 1:  return "models_dir";
        case 2:  return "cache";
        default: return "unknown";
    }
}

// server-models.h 105-124
static bool n_is_ready(int s)          { return s == N_LOADED; }
static bool n_is_running(int s)        { return s == N_LOADED || s == N_LOADING || s == N_SLEEPING; }
static bool n_is_ready_or_sleep(int s) { return s == N_LOADED || s == N_SLEEPING; }
static bool n_is_failed(int s, int code) { return s == N_UNLOADED && code != 0; }

// unload() 1518-1543: 0 nothing, 1 cancel the download, 2 stop the child,
// 3 nothing running
struct UnloadAct {
    int action;
    bool send_exit;
    bool terminate;
};

static UnloadAct n_unload(bool mapped, int s) {
    if (!mapped) {
        return { 0, false, false };
    }
    if (s == N_DOWNLOADING) {
        return { 1, false, false };
    }
    if (n_is_running(s)) {
        const bool loading = (s == N_LOADING);
        return { 2, !loading, loading };
    }
    return { 3, false, false };
}

// remove() 1668-1690: 0 accepted, 1 not found, 2 not from the cache
static int n_remove_gate(bool mapped, int source) {
    if (!mapped) {
        return 1;
    }
    if (source != 2) {
        return 2;
    }
    return 0;
}

// request_stop() 1487-1494
static bool n_stop_gate(bool mapped, bool stopping) {
    return mapped && !stopping;
}

// update_status() 1571-1620
struct Upd {
    int64_t status;
    int64_t code;
    int64_t stopping;
    int64_t sse_exit;
    int64_t sse_info;
    int64_t sse_progress;
};

static Upd n_update(bool mapped, int st, int code, bool has_info, bool has_prog, bool stopping) {
    Upd u{};
    if (mapped) {
        u.status = st;
        u.code = code;
        u.stopping = (st == N_UNLOADED) ? 0 : (stopping ? 1 : 0);
    } else {
        u.status = -1;
        u.code = -1;
        u.stopping = stopping ? 1 : 0;
    }
    // the SSE object is built outside the mapping lookup
    u.sse_exit = (st == N_UNLOADED) ? 1 : 0;
    u.sse_info = has_info ? 1 : 0;
    u.sse_progress = has_prog ? 1 : 0;
    return u;
}

// update_status() 1583-1604: the architecture modality merge
static bool n_arch_take(bool has_arch, int kind_state, bool all_strings) {
    if (!has_arch) {
        return false;
    }
    if (kind_state != 1) {
        return false;
    }
    return all_strings;
}

// server_lru_sched::has_capacity 267-273
static bool n_capacity(int models_max, size_t n_running) {
    return models_max <= 0 || n_running < (size_t) models_max;
}

struct Row {
    int status = N_UNLOADED;
    int64_t req = 0;
    int64_t last = 0;
    bool stopping = false;
    bool queued = false;
};

// server_lru_sched::pick_victim 276-296 over the std::map name order
static std::string n_victim(const std::map<std::string, Row> & m) {
    std::string victim;
    int64_t victim_last_used = 0;
    for (const auto & [name, r] : m) {
        if (r.req != 0 || !n_is_ready_or_sleep(r.status)) {
            continue;
        }
        if (r.stopping || r.queued) {
            continue;
        }
        if (victim.empty() || r.last < victim_last_used) {
            victim = name;
            victim_last_used = r.last;
        }
    }
    return victim;
}

struct QEntry {
    std::string model;
    bool loading = false;
};

// server_lru_sched::tick 369-411
static std::vector<std::string> n_tick(std::map<std::string, Row> & m, int models_max, const std::vector<QEntry> & q) {
    std::vector<std::string> evicted;
    if (models_max <= 0 || q.empty()) {
        return evicted;
    }
    int n_running = 0;
    int n_stopping = 0;
    for (const auto & [name, r] : m) {
        if (n_is_running(r.status)) {
            n_running++;
            if (r.stopping) {
                n_stopping++;
            }
        }
    }
    int n_needed = 0;
    int n_claimed = 0;
    for (const auto & e : q) {
        if (!e.loading) {
            n_needed++;
            continue;
        }
        auto it = m.find(e.model);
        if (it != m.end() && !n_is_running(it->second.status)) {
            n_claimed++;
        }
    }
    int n_free = models_max - n_running + n_stopping - n_claimed;
    while (n_free < n_needed) {
        const std::string victim = n_victim(m);
        if (victim.empty()) {
            return evicted;
        }
        m[victim].stopping = true; // request_stop inserts into stopping_models
        evicted.push_back(victim);
        n_free++;
    }
    return evicted;
}

// ── gate ───────────────────────────────────────────────────────────────────

static void gate_names(Gate & g) {
    UBuf o;
    g.begin("status_str");
    for (int s = 0; s <= 7; s++) {
        o.zero();
        const int64_t n = mg_status_str(s, su8(o));
        g.value("text", o.str(n), n_status_str(s));
        if (s <= 6) {
            g.expect("len", n, (int64_t) n_status_str(s).size());
        }
    }
    g.begin("source_str");
    for (int s = 0; s <= 3; s++) {
        o.zero();
        const int64_t n = mg_source_str(s, su8(o));
        g.value("text", o.str(n), n_source_str(s));
    }
}

static void gate_pred(Gate & g) {
    IBuf o(16);
    g.begin("pred");
    for (int s = 0; s <= 6; s++) {
        for (int code : { 0, 7 }) {
            o.zero();
            mg_pred(s, code, si64(o));
            g.value("is_ready", o.v[0], (int64_t) n_is_ready(s));
            g.value("is_running", o.v[1], (int64_t) n_is_running(s));
            g.value("is_ready_or_sleep", o.v[2], (int64_t) n_is_ready_or_sleep(s));
            g.value("is_failed", o.v[3], (int64_t) n_is_failed(s, code));
        }
    }
}

static void gate_unload(Gate & g) {
    IBuf o(8);
    g.begin("unload_action");
    for (int mapped = 0; mapped <= 1; mapped++) {
        for (int s = 0; s <= 6; s++) {
            o.zero();
            mg_unload_action(mapped, s, si64(o));
            const UnloadAct a = n_unload(mapped != 0, s);
            g.value("action", o.v[0], (int64_t) a.action);
            g.value("send_exit", o.v[1], (int64_t) a.send_exit);
            g.value("terminate", o.v[2], (int64_t) a.terminate);
        }
    }
    g.begin("unload_golden");
    o.zero();
    mg_unload_action(1, N_LOADING, si64(o));
    g.expect("loading_action", o.v[0], 2);
    g.expect("loading_send_exit", o.v[1], 0);
    g.expect("loading_terminate", o.v[2], 1);
    o.zero();
    mg_unload_action(1, N_SLEEPING, si64(o));
    g.expect("sleeping_send_exit", o.v[1], 1);
    g.expect("sleeping_terminate", o.v[2], 0);
}

static void gate_remove(Gate & g) {
    IBuf o(8);
    g.begin("remove_gate");
    for (int mapped = 0; mapped <= 1; mapped++) {
        for (int src = 0; src <= 2; src++) {
            o.zero();
            const int64_t n = mg_remove_gate(mapped, src, si64(o));
            g.value("verdict", o.v[0], (int64_t) n_remove_gate(mapped != 0, src));
            g.value("words", n, (int64_t) 1);
        }
    }
}

// the wait predicates: unload() 1534-1538, unload_lru() 1309-1315, remove() 1695-1698
static bool n_wait_pred(int purpose, int s) {
    if (purpose == 0) {
        return s != N_DOWNLOADING;
    }
    if (purpose == 1) {
        return s == N_UNLOADED;
    }
    return s == N_UNLOADED || s == N_DOWNLOADED;
}

static void gate_wait(Gate & g) {
    g.begin("wait_pred");
    for (int purpose = 0; purpose <= 2; purpose++) {
        for (int s = 0; s <= 6; s++) {
            g.value("satisfied", mg_wait_pred(purpose, s), (int64_t) n_wait_pred(purpose, s));
        }
    }
}

static void gate_stopgate(Gate & g) {
    g.begin("stop_gate");
    for (int mapped = 0; mapped <= 1; mapped++) {
        for (int stopping = 0; stopping <= 1; stopping++) {
            g.value("issue", mg_stop_gate(mapped, stopping), (int64_t) n_stop_gate(mapped != 0, stopping != 0));
        }
    }
}

static void gate_update(Gate & g) {
    IBuf o(16);
    g.begin("update_fields");
    struct Case {
        int mapped, status, code, info, prog, stopping;
    };
    const Case cases[] = {
        { 1, N_LOADED,      0, 1, 0, 0 },
        { 1, N_UNLOADED,    0, 0, 0, 1 },
        { 1, N_UNLOADED,    9, 0, 0, 0 },
        { 1, N_LOADING,     0, 0, 1, 0 },
        { 1, N_SLEEPING,    0, 0, 0, 0 },
        { 0, N_LOADED,      0, 1, 1, 1 },
        { 0, N_UNLOADED,    3, 0, 0, 1 },
        { 1, N_DOWNLOADING, 0, 1, 1, 1 },
    };
    for (const auto & c : cases) {
        o.zero();
        const int64_t n = mg_update_fields(c.mapped, c.status, c.code, c.info, c.prog, c.stopping, si64(o));
        const Upd u = n_update(c.mapped != 0, c.status, c.code, c.info != 0, c.prog != 0, c.stopping != 0);
        g.value("status", o.v[0], u.status);
        g.value("code", o.v[1], u.code);
        g.value("stopping", o.v[2], u.stopping);
        g.value("sse_exit", o.v[3], u.sse_exit);
        g.value("sse_info", o.v[4], u.sse_info);
        g.value("sse_progress", o.v[5], u.sse_progress);
        g.value("words", n, (int64_t) 6);
    }
}

static void gate_arch(Gate & g) {
    g.begin("arch_take");
    for (int has = 0; has <= 1; has++) {
        for (int kind = 0; kind <= 2; kind++) {
            for (int all = 0; all <= 1; all++) {
                g.value("take", mg_arch_take(has, kind, all), (int64_t) n_arch_take(has != 0, kind, all != 0));
            }
        }
    }
}

static void gate_capacity(Gate & g) {
    g.begin("capacity");
    for (int mm : { 0, -1, 2, 4 }) {
        for (int nr : { 0, 1, 2, 3, 5 }) {
            g.value("has_capacity", mg_capacity(mm, nr), (int64_t) n_capacity(mm, (size_t) nr));
        }
    }
}

struct Table {
    std::map<std::string, Row> m;
    std::vector<std::string> order; // insertion order, deliberately unsorted

    void add(const std::string & name, Row r) {
        order.push_back(name);
        m[name] = r;
    }
};

static void run_victim(Gate & g, const char * case_name, const Table & t) {
    g.begin(case_name);
    NameTable names;
    std::vector<int64_t> status, req, last, stopping, queued;
    for (const auto & name : t.order) {
        names.add(name);
        const Row & r = t.m.at(name);
        status.push_back(r.status);
        req.push_back(r.req);
        last.push_back(r.last);
        stopping.push_back(r.stopping ? 1 : 0);
        queued.push_back(r.queued ? 1 : 0);
    }
    const int64_t victim = mg_victim(names.n(), su8(names), si64(names.off), si64(names.len),
                                     si64(status), si64(req), si64(last), si64(stopping), si64(queued));
    const std::string nv = n_victim(t.m);
    std::string pv;
    if (victim >= 0) {
        pv = t.order[(size_t) victim];
    }
    g.value("victim", pv, nv);
    if (!nv.empty()) {
        int64_t want = -1;
        for (size_t i = 0; i < t.order.size(); i++) {
            if (t.order[i] == nv) {
                want = (int64_t) i;
            }
        }
        g.value("victim_row", victim, want);
    } else {
        g.value("victim_row", victim, (int64_t) -1);
    }
}

static void gate_victim(Gate & g) {
    {
        Table t;
        t.add("zeta",  { N_LOADED, 0, 500 });
        t.add("alpha", { N_LOADED, 0, 100 });
        t.add("mid",   { N_SLEEPING, 0, 300 });
        t.add("beta",  { N_LOADED, 0, 200 });
        run_victim(g, "victim_lru", t);
    }
    {
        // equal last_used: the first name in map order wins
        Table t;
        t.add("delta", { N_LOADED, 0, 700 });
        t.add("bravo", { N_LOADED, 0, 700 });
        t.add("charlie", { N_LOADED, 0, 700 });
        run_victim(g, "victim_tie", t);
    }
    {
        // busy, still loading, already stopping and wanted: all skipped
        Table t;
        t.add("a_busy",    { N_LOADED, 3, 10 });
        t.add("b_loading", { N_LOADING, 0, 20 });
        t.add("c_stopping", { N_LOADED, 0, 30 });
        t.m["c_stopping"].stopping = true;
        t.add("d_wanted",  { N_SLEEPING, 0, 40 });
        t.m["d_wanted"].queued = true;
        t.add("e_free",    { N_LOADED, 0, 900 });
        run_victim(g, "victim_skip", t);
    }
    {
        // nothing can be given up
        Table t;
        t.add("a_busy",   { N_LOADED, 1, 10 });
        t.add("b_load",   { N_LOADING, 0, 20 });
        t.add("c_unl",    { N_UNLOADED, 0, 30 });
        t.add("d_down",   { N_DOWNLOADING, 0, 40 });
        run_victim(g, "victim_none", t);
    }
    {
        Table t;
        run_victim(g, "victim_empty", t);
    }
}

struct TickCase {
    const char * name;
    int models_max;
    Table table;
    std::vector<QEntry> queue;
    std::vector<std::string> golden; // the eviction order this case must produce
};

static void run_tick(Gate & g, const TickCase & c) {
    g.begin(c.name);
    NameTable names;
    std::vector<int64_t> status, req, last, stopping;
    std::map<std::string, int64_t> row_of;
    for (const auto & name : c.table.order) {
        row_of[name] = names.n();
        names.add(name);
        const Row & r = c.table.m.at(name);
        status.push_back(r.status);
        req.push_back(r.req);
        last.push_back(r.last);
        stopping.push_back(r.stopping ? 1 : 0);
    }
    std::vector<int64_t> qrow, qloading;
    for (const auto & e : c.queue) {
        auto it = row_of.find(e.model);
        qrow.push_back(it == row_of.end() ? -1 : it->second);
        qloading.push_back(e.loading ? 1 : 0);
    }
    IBuf out(64);
    const int64_t n = mg_tick(names.n(), su8(names), si64(names.off), si64(names.len),
                              si64(status), si64(req), si64(last), si64(stopping), c.models_max,
                              (int64_t) c.queue.size(), si64(qrow), si64(qloading), si64(out));
    std::map<std::string, Row> native_map = c.table.m;
    const std::vector<std::string> nv = n_tick(native_map, c.models_max, c.queue);

    g.value("evicted", n, (int64_t) nv.size());
    for (int64_t i = 0; i < n && i < out.cap(); i++) {
        const std::string pname = c.table.order[(size_t) out.v[i]];
        const std::string nname = i < (int64_t) nv.size() ? nv[(size_t) i] : std::string("<extra>");
        g.value("victim", pname, nname);
    }
    // the stop mark the eviction leaves behind
    for (size_t i = 0; i < c.table.order.size(); i++) {
        const std::string & name = c.table.order[i];
        g.value(("stopping_" + name).c_str(), stopping[i], native_map[name].stopping ? 1 : 0);
    }
    g.value("order", join([&] {
        std::vector<std::string> v;
        for (int64_t i = 0; i < n && i < out.cap(); i++) {
            v.push_back(c.table.order[(size_t) out.v[i]]);
        }
        return v;
    }(), ","), join(c.golden, ","));
}

static void gate_tick(Gate & g) {
    {
        TickCase c;
        c.name = "tick_evict_two";
        c.models_max = 2;
        c.table.add("m_a", { N_LOADED, 0, 100 });
        c.table.add("m_b", { N_LOADED, 0, 200 });
        c.table.add("m_c", { N_LOADED, 0, 50 });
        c.queue = { { "m_x", false }, { "m_y", false } };
        c.golden = { "m_c", "m_a", "m_b" };
        run_tick(g, c);
    }
    {
        TickCase c;
        c.name = "tick_busy_wait";
        c.models_max = 1;
        c.table.add("m_a", { N_LOADED, 2, 100 });
        c.queue = { { "m_x", false }, { "m_x", false } };
        c.golden = {};
        run_tick(g, c);
    }
    {
        TickCase c;
        c.name = "tick_no_queue";
        c.models_max = 1;
        c.table.add("m_a", { N_LOADED, 0, 100 });
        c.golden = {};
        run_tick(g, c);
    }
    {
        TickCase c;
        c.name = "tick_no_limit";
        c.models_max = 0;
        c.table.add("m_a", { N_LOADED, 0, 100 });
        c.queue = { { "m_x", false }, { "m_y", false } };
        c.golden = {};
        run_tick(g, c);
    }
    {
        // a claimed slot for a model that is not running yet counts as free-for-one
        TickCase c;
        c.name = "tick_claimed";
        c.models_max = 2;
        c.table.add("m_a", { N_LOADED, 0, 100 });
        c.table.add("m_b", { N_UNLOADED, 0, 10 });
        c.table.add("m_c", { N_LOADED, 0, 200 });
        c.queue = { { "m_b", true }, { "m_x", false } };
        c.golden = { "m_a", "m_c" };
        run_tick(g, c);
    }
}

static void gate_lru_log(Gate & g) {
    UBuf o;
    g.begin("lru_log");
    const char * names[] = { "m_a", "m_b" };
    for (int mm : { 0, 1, 2 }) {
        for (int nr : { 0, 1, 2 }) {
            o.zero();
            const std::string name = names[(mm + nr) % 2];
            const int64_t n = mg_lru_log(mm, nr, sl_u8{ (uint8_t *) name.data(), (int64_t) name.size() }, su8(o));
            std::string want;
            if (mm > 0 && nr >= mm) {
                want = "models_max limit reached, removing LRU name=" + name;
            }
            g.value("text", o.str(n), want);
        }
    }
}

int main() {
    goose_init(0, nullptr);

    Gate g("MG");
    gate_names(g);
    gate_pred(g);
    gate_unload(g);
    gate_remove(g);
    gate_wait(g);
    gate_stopgate(g);
    gate_update(g);
    gate_arch(g);
    gate_capacity(g);
    gate_victim(g);
    gate_tick(g);
    gate_lru_log(g);
    return g.finish() == 0 ? 0 : 1;
}
