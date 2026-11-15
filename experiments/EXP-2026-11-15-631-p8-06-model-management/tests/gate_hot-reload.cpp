// Differential gate for the hot-reload slice (hot-reload.goose).
//
// Every case runs twice: through the Goose export and through a transcription of
// the native reload code with its lines cited. The mapping is a std::map here,
// because that is what the native walk iterates; the port sorts the rows itself.
#include "hot-reload-gen.h"

#include "common.h"

static sl_u8 su8(UBuf & u) {
    return sl_u8{ u.data(), u.cap() };
}

static sl_i64 si64(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

static sl_i64 si64(IBuf & i) {
    return sl_i64{ i.data(), i.cap() };
}

static sl_u8 su8(NameTable & t) {
    return sl_u8{ t.buf.data(), (int64_t) t.buf.size() };
}

// ── native side ────────────────────────────────────────────────────────────

enum {
    N_DOWNLOADING = 0,
    N_DOWNLOADED  = 2,
    N_UNLOADED    = 3,
    N_LOADING     = 4,
    N_LOADED      = 5,
    N_SLEEPING    = 6,
};

static bool n_is_running(int s) {
    return s == N_LOADED || s == N_LOADING || s == N_SLEEPING;
}

// common/common.cpp 461-471
static std::string n_strip(const std::string & str) {
    size_t a = 0;
    size_t e = str.size();
    while (a < e && std::isspace((unsigned char) str[a])) {
        a++;
    }
    while (e > a && std::isspace((unsigned char) str[e - 1])) {
        e--;
    }
    return str.substr(a, e - a);
}

// common/common.h 836-849
static std::vector<std::string> n_split(const std::string & str, char delim) {
    std::vector<std::string> parts;
    size_t begin = 0;
    size_t pos = str.find(delim);
    while (pos != std::string::npos) {
        parts.push_back(str.substr(begin, pos - begin));
        begin = pos + 1;
        pos = str.find(delim, begin);
    }
    parts.push_back(str.substr(begin));
    return parts;
}

// preset_options_for_compare 1029-1036 plus the map comparison
static std::string n_norm(const std::string & opts) {
    std::map<std::string, std::string> m;
    size_t pos = 0;
    while (pos <= opts.size()) {
        size_t e = opts.find('\n', pos);
        if (e == std::string::npos) {
            e = opts.size();
        }
        const std::string entry = opts.substr(pos, e - pos);
        const size_t eq = entry.find('=');
        if (eq != std::string::npos) {
            m[entry.substr(0, eq)] = entry.substr(eq + 1);
        }
        if (e == opts.size()) {
            break;
        }
        pos = e + 1;
    }
    m.erase("LLAMA_ARG_HOST");
    m.erase("LLAMA_ARG_PORT");
    m.erase("LLAMA_ARG_ALIAS");
    m.erase("LLAMA_ARG_LOG_COLORS");

    std::string out;
    for (const auto & [k, v] : m) {
        if (!out.empty()) {
            out += '\n';
        }
        out += k + "=" + v;
    }
    return out;
}

struct Row {
    int status = N_UNLOADED;
    std::string opts;
};

// load_models() reload branch 1104-1116
static std::vector<std::string> n_unload(const std::map<std::string, Row> & cur,
                                         const std::map<std::string, std::string> & fresh) {
    std::vector<std::string> out;
    for (const auto & [name, r] : cur) {
        if (!n_is_running(r.status)) {
            continue;
        }
        auto it = fresh.find(name);
        if (it == fresh.end()) {
            out.push_back(name); // removed from source
        } else if (n_norm(r.opts) != n_norm(it->second)) {
            out.push_back(name); // preset changed
        }
    }
    return out;
}

// load_models() reload branch 1128-1148
static std::vector<std::string> n_erase(const std::map<std::string, Row> & cur,
                                        const std::map<std::string, std::string> & fresh) {
    std::vector<std::string> out;
    for (const auto & [name, r] : cur) {
        if (r.status == N_DOWNLOADING) {
            continue;
        }
        if (r.status == N_DOWNLOADED) {
            out.push_back(name);
        } else if (fresh.find(name) == fresh.end()) {
            out.push_back(name);
        }
    }
    return out;
}

// the alias pass 1139-1169 over the live sets
static void n_alias_pass(const std::map<std::string, std::string> & alias_opt,
                         const std::map<std::string, bool> & running,
                         std::map<std::string, std::set<std::string>> & sets,
                         std::vector<std::string> & conf) {
    for (const auto & [name, opt] : alias_opt) {
        if (running.at(name)) {
            continue;
        }
        std::set<std::string> new_aliases;
        if (!opt.empty()) {
            for (const auto & raw : n_split(opt, ',')) {
                const std::string a = n_strip(raw);
                if (!a.empty()) {
                    new_aliases.insert(a);
                }
            }
        }
        sets[name].clear();
        for (const auto & a : new_aliases) {
            std::string other;
            for (const auto & [on, os] : sets) {
                if (on == name) {
                    continue;
                }
                if (on == a || os.count(a) != 0) {
                    other = on;
                    break;
                }
            }
            if (other.empty()) {
                sets[name].insert(a);
            } else {
                conf.push_back(a + "|" + name + "|" + other);
            }
        }
    }
}

// 1171-1180
static std::set<std::string> n_tags(const std::string & opt, bool present) {
    std::set<std::string> out;
    if (!present) {
        return out;
    }
    for (const auto & raw : n_split(opt, ',')) {
        const std::string t = n_strip(raw);
        if (!t.empty()) {
            out.insert(t);
        }
    }
    return out;
}

// common/arg.cpp 1337-1339
static bool n_truthy(const std::string & v) {
    return v == "on" || v == "enabled" || v == "true" || v == "1";
}

// 1075-1095
static std::vector<std::string> n_startup_pick(const std::map<std::string, std::string> & vals,
                                               const std::map<std::string, bool> & present) {
    std::vector<std::string> out;
    for (const auto & [name, v] : vals) {
        if (present.at(name) && n_truthy(v)) {
            out.push_back(name);
        }
    }
    return out;
}

// 960-995
static std::vector<int> n_hidden(const std::vector<std::string> & names, const std::vector<std::string> & paths,
                                 const std::vector<bool> & pure, const std::set<std::string> & preset_paths) {
    std::vector<int> out;
    for (size_t i = 0; i < names.size(); i++) {
        const bool hide = pure[i] && !paths[i].empty() && preset_paths.count(paths[i]) != 0;
        out.push_back(hide ? 1 : 0);
    }
    return out;
}

// ── gate ───────────────────────────────────────────────────────────────────

static void gate_norm(Gate & g) {
    UBuf o(4096);
    g.begin("preset_norm");
    const std::vector<std::string> cases = {
        "LLAMA_ARG_MODEL=/m/a.gguf\nLLAMA_ARG_HOST=0.0.0.0\nLLAMA_ARG_PORT=8080\nLLAMA_ARG_NGL=99",
        "LLAMA_ARG_ALIAS=a,b\nLLAMA_ARG_LOG_COLORS=on\nLLAMA_ARG_CTX=4096",
        "LLAMA_ARG_HOST=127.0.0.1\nLLAMA_ARG_PORT=1",
        "LLAMA_ARG_MODEL=/m/b.gguf",
        "",
        "LLAMA_ARG_CTX=4096\nLLAMA_ARG_NGL=99\nLLAMA_ARG_TAGS=fast",
    };
    for (const auto & c : cases) {
        o.zero();
        const int64_t n = hr_preset_norm(sl_u8{ (uint8_t *) c.data(), (int64_t) c.size() }, 0, (int64_t) c.size(), su8(o));
        g.value("text", o.str(n), n_norm(c));
    }
}

struct CurRow {
    std::string name;
    Row row;
};

static void gate_unload(Gate & g) {
    UBuf o(4096);
    g.begin("unload_set");
    // rows in insertion order, deliberately not sorted
    const std::vector<CurRow> rows = {
        { "zeta",   { N_LOADED, "LLAMA_ARG_MODEL=/m/z.gguf\nLLAMA_ARG_HOST=0.0.0.0\nLLAMA_ARG_NGL=99" } },
        { "alpha",  { N_LOADED, "LLAMA_ARG_MODEL=/m/a.gguf\nLLAMA_ARG_PORT=8080\nLLAMA_ARG_NGL=99" } },
        { "beta",   { N_SLEEPING, "LLAMA_ARG_MODEL=/m/b.gguf" } },
        { "gamma",  { N_UNLOADED, "LLAMA_ARG_MODEL=/m/g.gguf" } },
        { "delta",  { N_LOADING, "LLAMA_ARG_MODEL=/m/d.gguf\nLLAMA_ARG_NGL=42" } },
    };
    // zeta keeps its file but only the injected options change; alpha changes
    // NGL; beta keeps its preset; gamma is gone from the source but not running;
    // delta changes NGL while loading
    const std::vector<std::pair<std::string, std::string>> fresh = {
        { "zeta", "LLAMA_ARG_MODEL=/m/z.gguf\nLLAMA_ARG_PORT=9999\nLLAMA_ARG_ALIAS=zz\nLLAMA_ARG_NGL=99" },
        { "alpha", "LLAMA_ARG_MODEL=/m/a.gguf\nLLAMA_ARG_PORT=8080\nLLAMA_ARG_NGL=88" },
        { "beta", "LLAMA_ARG_MODEL=/m/b.gguf" },
        { "delta", "LLAMA_ARG_MODEL=/m/d.gguf\nLLAMA_ARG_NGL=43" },
        { "epsilon", "LLAMA_ARG_MODEL=/m/e.gguf" },
    };

    NameTable names, nnames;
    std::vector<int64_t> status, coff, clen, noff, nlen;
    std::string copt, popt;
    std::map<std::string, Row> cmap;
    std::map<std::string, std::string> fmap;
    for (const auto & r : rows) {
        names.add(r.name);
        status.push_back(r.row.status);
        coff.push_back((int64_t) copt.size());
        clen.push_back((int64_t) r.row.opts.size());
        copt += r.row.opts;
        cmap[r.name] = r.row;
    }
    for (const auto & [name, opts] : fresh) {
        nnames.add(name);
        noff.push_back((int64_t) popt.size());
        nlen.push_back((int64_t) opts.size());
        popt += opts;
        fmap[name] = opts;
    }
    IBuf out(64), reasons(64);
    const int64_t n = hr_unload_set(names.n(), su8(names), si64(names.off), si64(names.len), si64(status),
                                    sl_u8{ (uint8_t *) copt.data(), (int64_t) copt.size() }, si64(coff), si64(clen),
                                    nnames.n(), su8(nnames), si64(nnames.off), si64(nnames.len),
                                    sl_u8{ (uint8_t *) popt.data(), (int64_t) popt.size() }, si64(noff), si64(nlen),
                                    si64(out), si64(reasons));
    const std::vector<std::string> nv = n_unload(cmap, fmap);
    g.value("count", n, (int64_t) nv.size());
    for (int64_t i = 0; i < n && i < out.cap(); i++) {
        const std::string & pname = rows[(size_t) out.v[i]].name;
        g.value("row", pname, i < (int64_t) nv.size() ? nv[(size_t) i] : std::string("<extra>"));
        // 1 removed from the source, 2 the preset changed
        int64_t want = 0;
        if (i < (int64_t) nv.size()) {
            want = fmap.count(nv[(size_t) i]) == 0 ? 1 : 2;
        }
        g.value("reason", reasons.v[i], want);
    }
    o.zero();
}

static void gate_erase(Gate & g) {
    g.begin("erase_set");
    const std::vector<CurRow> rows = {
        { "a_down",  { N_DOWNLOADING, "" } },
        { "b_dled",  { N_DOWNLOADED, "" } },
        { "c_gone",  { N_UNLOADED, "" } },
        { "d_keep",  { N_UNLOADED, "" } },
        { "e_run",   { N_LOADED, "" } },
    };
    const std::vector<std::string> fresh = { "d_keep", "e_run" };

    NameTable names, nnames;
    std::vector<int64_t> status;
    std::map<std::string, Row> cmap;
    std::map<std::string, std::string> fmap;
    for (const auto & r : rows) {
        names.add(r.name);
        status.push_back(r.row.status);
        cmap[r.name] = r.row;
    }
    for (const auto & n : fresh) {
        nnames.add(n);
        fmap[n] = "";
    }
    IBuf out(64);
    const int64_t n = hr_erase_set(names.n(), su8(names), si64(names.off), si64(names.len), si64(status),
                                   nnames.n(), su8(nnames), si64(nnames.off), si64(nnames.len), si64(out));
    const std::vector<std::string> nv = n_erase(cmap, fmap);
    g.value("count", n, (int64_t) nv.size());
    for (int64_t i = 0; i < n && i < out.cap(); i++) {
        g.value("row", rows[(size_t) out.v[i]].name, i < (int64_t) nv.size() ? nv[(size_t) i] : std::string("<extra>"));
    }
}

struct AliasCase {
    const char * name;
    std::vector<std::string> names;      // insertion order
    std::vector<std::string> alias_opt;  // "" = option absent
    std::vector<bool> running;
    std::map<std::string, std::set<std::string>> old_sets;
    std::vector<std::string> golden_conf;
};

static void run_alias(Gate & g, const AliasCase & c) {
    g.begin(c.name);
    const size_t n = c.names.size();

    // the universe: every name and every alias any preset asks for, sorted
    std::set<std::string> uni;
    for (size_t i = 0; i < n; i++) {
        uni.insert(c.names[i]);
        if (!c.alias_opt[i].empty()) {
            for (const auto & raw : n_split(c.alias_opt[i], ',')) {
                const std::string a = n_strip(raw);
                if (!a.empty()) {
                    uni.insert(a);
                }
            }
        }
    }
    std::vector<std::string> uv(uni.begin(), uni.end());
    NameTable ubuf, nm, astr;
    for (const auto & u : uv) {
        ubuf.add(u);
    }
    std::vector<int64_t> ao, al;
    std::string abuf;
    std::vector<int64_t> running, masks;
    std::map<std::string, std::string> alias_map;
    std::map<std::string, bool> run_map;
    std::map<std::string, std::set<std::string>> sets = c.old_sets;
    for (size_t i = 0; i < n; i++) {
        nm.add(c.names[i]);
        ao.push_back((int64_t) abuf.size());
        al.push_back((int64_t) c.alias_opt[i].size());
        abuf += c.alias_opt[i];
        running.push_back(c.running[i] ? 1 : 0);
        alias_map[c.names[i]] = c.alias_opt[i];
        run_map[c.names[i]] = c.running[i];

        int64_t mask = 0;
        const std::set<std::string> held_set = c.old_sets.count(c.names[i]) ? c.old_sets.at(c.names[i])
                                                                             : std::set<std::string>();
        for (const auto & held : held_set) {
            for (size_t k = 0; k < uv.size(); k++) {
                if (uv[k] == held) {
                    mask |= (int64_t) 1 << k;
                }
            }
        }
        masks.push_back(mask);
    }

    UBuf conf(4096);
    IBuf meta(8);
    const int64_t nconf = hr_alias_apply((int64_t) n,
                                         su8(ubuf), si64(ubuf.off), si64(ubuf.len), (int64_t) uv.size(),
                                         su8(nm), si64(nm.off), si64(nm.len),
                                         sl_u8{ (uint8_t *) abuf.data(), (int64_t) abuf.size() }, si64(ao), si64(al),
                                         si64(running), si64(masks), su8(conf), si64(meta));

    std::vector<std::string> native_conf;
    n_alias_pass(alias_map, run_map, sets, native_conf);

    // the log text is line oriented: every warning carries its own newline
    std::string want;
    for (const auto & line : native_conf) {
        want += line + "\n";
    }
    std::string want_golden;
    for (const auto & line : c.golden_conf) {
        want_golden += line + "\n";
    }
    g.value("conflicts", meta.v[0], (int64_t) native_conf.size());
    g.value("conf_text", conf.str(nconf), want);
    g.value("conf_golden", conf.str(nconf), want_golden);
    g.value("conf_len", nconf, (int64_t) want.size());

    for (size_t i = 0; i < n; i++) {
        std::vector<std::string> port_set;
        for (size_t k = 0; k < uv.size(); k++) {
            if ((masks[i] >> k) & 1) {
                port_set.push_back(uv[k]);
            }
        }
        std::vector<std::string> nat_set(sets[c.names[i]].begin(), sets[c.names[i]].end());
        g.value(("set_" + c.names[i]).c_str(), join(port_set, ","), join(nat_set, ","));
    }
}

static void gate_alias(Gate & g) {
    {
        // a preset alias that is another model's name
        AliasCase c;
        c.name = "alias_name_clash";
        c.names = { "zeta", "beta" };
        c.alias_opt = { "beta,zz", "x" };
        c.running = { false, false };
        c.golden_conf = { "beta|zeta|beta" };
        run_alias(g, c);
    }
    {
        // a later row asks for an alias an earlier row just took: the earlier
        // row is already updated, so the clash is seen
        AliasCase c;
        c.name = "alias_forward_clash";
        c.names = { "m_one", "m_two" };
        c.alias_opt = { "shared", "shared,y" };
        c.running = { false, false };
        c.golden_conf = { "shared|m_two|m_one" };
        run_alias(g, c);
    }
    {
        // an earlier row asks for an alias a later row still holds from before
        // the reload: the later row has not been cleared yet, so the clash is seen
        AliasCase c;
        c.name = "alias_backward_clash";
        c.names = { "m_one", "m_two" };
        c.alias_opt = { "keepme", "" };
        c.running = { false, false };
        c.old_sets = { { "m_two", { "keepme" } } };
        c.golden_conf = { "keepme|m_one|m_two" };
        run_alias(g, c);
    }
    {
        // a running row is not rewritten, but it still owns its aliases
        AliasCase c;
        c.name = "alias_running_holds";
        c.names = { "m_run", "m_idle" };
        c.alias_opt = { "other", "runalias" };
        c.running = { true, false };
        c.old_sets = { { "m_run", { "runalias" } } };
        c.golden_conf = { "runalias|m_idle|m_run" };
        run_alias(g, c);
    }
    {
        // an alias equal to the model's own name is not a clash
        AliasCase c;
        c.name = "alias_self";
        c.names = { "solo" };
        c.alias_opt = { "solo" };
        c.running = { false };
        c.golden_conf = {};
        run_alias(g, c);
    }
    {
        // whitespace, empty entries and a dropped alias set
        AliasCase c;
        c.name = "alias_ws";
        c.names = { "w_one", "w_two" };
        c.alias_opt = { "  aa , ,bb\t", "" };
        c.running = { false, false };
        c.old_sets = { { "w_two", { "bb" } } };
        c.golden_conf = { "bb|w_one|w_two" };
        run_alias(g, c);
    }
}

static void gate_tags(Gate & g) {
    UBuf o(1024);
    g.begin("tags");
    const char * cases[] = { "fast, small", "  ", "b,a,c", "a,a,a", "", " x ,y, z " };
    for (const char * c : cases) {
        o.zero();
        const std::string s(c);
        const int64_t n = hr_tags(sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() }, 1, su8(o));
        const std::set<std::string> want = n_tags(s, true);
        g.value("text", o.str(n), join(std::vector<std::string>(want.begin(), want.end()), ","));
    }
    o.zero();
    g.value("absent", hr_tags(sl_u8{ (uint8_t *) "", 0 }, 0, su8(o)), (int64_t) 0);
}

static void gate_truthy(Gate & g) {
    g.begin("truthy");
    const char * cases[] = { "on", "enabled", "true", "1", "off", "0", "", "TRUE", "yes" };
    for (const char * c : cases) {
        const std::string s(c);
        g.value("v", hr_truthy(sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() }), (int64_t) n_truthy(s));
    }
}

static void gate_startup(Gate & g) {
    UBuf o(1024);
    g.begin("startup_pick");
    const std::vector<std::string> names = { "zeta", "alpha", "mid", "beta" };
    const std::vector<std::string> vals = { "0", "on", "true", "enabled" };
    const std::vector<bool> present = { true, true, false, true };

    NameTable nt;
    std::string vbuf;
    std::vector<int64_t> vo, vl, pres;
    std::map<std::string, std::string> vmap;
    std::map<std::string, bool> pmap;
    for (size_t i = 0; i < names.size(); i++) {
        nt.add(names[i]);
        vo.push_back((int64_t) vbuf.size());
        vl.push_back((int64_t) vals[i].size());
        vbuf += vals[i];
        pres.push_back(present[i] ? 1 : 0);
        vmap[names[i]] = vals[i];
        pmap[names[i]] = present[i];
    }
    IBuf out(32);
    const int64_t n = hr_startup_pick(nt.n(), su8(nt), si64(nt.off), si64(nt.len),
                                      sl_u8{ (uint8_t *) vbuf.data(), (int64_t) vbuf.size() }, si64(vo), si64(vl), si64(pres),
                                      si64(out));
    const std::vector<std::string> nv = n_startup_pick(vmap, pmap);
    g.value("count", n, (int64_t) nv.size());
    for (int64_t i = 0; i < n && i < out.cap(); i++) {
        g.value("row", names[(size_t) out.v[i]], i < (int64_t) nv.size() ? nv[(size_t) i] : std::string("<extra>"));
    }

    g.begin("startup_guard");
    for (int cnt : { 0, 1, 3, 4 }) {
        for (int mm : { 0, 1, 4 }) {
            o.zero();
            const int64_t bad = hr_startup(cnt, mm, su8(o));
            const bool want = cnt > mm;
            g.value("refuse", bad, (int64_t) want);
            std::string text;
            if (want) {
                text = "number of models to load on startup (" + std::to_string(cnt) + ") exceeds models_max (" +
                       std::to_string(mm) + ")";
            }
            g.value("text", o.str(bad == 0 ? 0 : (int64_t) text.size()), text);
        }
    }
}

static void gate_hidden(Gate & g) {
    IBuf out(32);
    UBuf log(2048);
    g.begin("hidden");
    const std::vector<std::string> names = { "c_hit", "c_pure_miss", "c_shared", "c_empty" };
    const std::vector<std::string> paths = { "/cache/a.gguf", "/cache/b.gguf", "/cache/c.gguf", "" };
    const std::vector<bool> pure = { true, true, false, true };
    const std::vector<std::string> preset_paths = { "/cache/a.gguf", "/cache/c.gguf" };

    NameTable cn, pn;
    std::string pbuf;
    std::vector<int64_t> po, pl, pu;
    for (size_t i = 0; i < names.size(); i++) {
        cn.add(names[i]);
        po.push_back((int64_t) pbuf.size());
        pl.push_back((int64_t) paths[i].size());
        pbuf += paths[i];
        pu.push_back(pure[i] ? 1 : 0);
    }
    std::set<std::string> pset;
    for (const auto & p : preset_paths) {
        pn.add(p);
        pset.insert(p);
    }
    const int64_t n = hr_hidden(cn.n(), sl_u8{ (uint8_t *) pbuf.data(), (int64_t) pbuf.size() },
                                si64(po), si64(pl), si64(pu),
                                pn.n(), su8(pn), si64(pn.off), si64(pn.len), si64(out));
    const std::vector<int> nv = n_hidden(names, paths, pure, pset);
    int64_t want_count = 0;
    for (size_t i = 0; i < names.size(); i++) {
        g.value(("flag_" + names[i]).c_str(), out.v[(int64_t) i], (int64_t) nv[i]);
        want_count += nv[i];
    }
    g.value("count", n, want_count);

    log.zero();
    const int64_t ln = hr_hidden_log(cn.n(), su8(cn), si64(cn.off), si64(cn.len), si64(out), su8(log));
    std::string want_log;
    for (size_t i = 0; i < names.size(); i++) {
        if (nv[i] != 0) {
            want_log += "hiding cache model name=" + names[i] + " (deduplicated by a preset)\n";
        }
    }
    g.value("log", log.str(ln), want_log);
}

int main() {
    goose_init(0, nullptr);

    Gate g("HR");
    gate_norm(g);
    gate_unload(g);
    gate_erase(g);
    gate_alias(g);
    gate_tags(g);
    gate_truthy(g);
    gate_startup(g);
    gate_hidden(g);
    return g.finish() == 0 ? 0 : 1;
}
