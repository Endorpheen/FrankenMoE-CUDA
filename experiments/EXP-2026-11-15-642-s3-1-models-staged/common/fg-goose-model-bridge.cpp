// Goose bridge for the model-management decisions (staged S3.1 trial).
// See fg-goose-model-bridge.h for the contract; the modules are
// goose/server/{model-manager,hot-reload,connectors}.goose, composed to
// goose-gen/models.goose and generated to goose-gen/models.c.

#include "fg-goose-model-bridge.h"

#include "models.h"

#include <algorithm>
#include <cstring>
#include <mutex>

namespace fg_goose_model {

namespace {

long long g_calls = 0;

void init() {
    static std::once_flag once;
    std::call_once(once, []() { goose_init(0, nullptr); });
}

// Every name the module takes is one buffer addressed by off/ln. `owner` keeps the
// bytes alive for the duration of the call.
struct NameTable {
    std::vector<uint8_t> buf;
    std::vector<int64_t> off;
    std::vector<int64_t> len;

    void add(const std::string & s) {
        off.push_back((int64_t) buf.size());
        len.push_back((int64_t) s.size());
        buf.insert(buf.end(), s.begin(), s.end());
    }

    sl_u8 names() { return sl_u8{buf.empty() ? nullptr : buf.data(), (int64_t) buf.size()}; }
    sl_i64 offs() { return sl_i64{off.empty() ? nullptr : off.data(), (int64_t) off.size()}; }
    sl_i64 lens() { return sl_i64{len.empty() ? nullptr : len.data(), (int64_t) len.size()}; }
    int64_t n() const { return (int64_t) off.size(); }
};

sl_u8 sl(const std::string & s) {
    return sl_u8{(uint8_t *) (s.empty() ? nullptr : s.data()), (int64_t) s.size()};
}

sl_i64 slv(std::vector<int64_t> & v) {
    return sl_i64{v.empty() ? nullptr : v.data(), (int64_t) v.size()};
}

// The native preset is a std::map<option>, so its option text is always in key order;
// the module walks the entries in the order it is given. Canonicalising the order here
// keeps the module's comparison order-insensitive exactly as the native map is, and it
// is a flattening, not a policy: keys and values pass through untouched, duplicates are
// outside the contract on both sides (unique keys).
std::string canon_options(const std::string & options) {
    std::vector<std::string> entries;
    size_t pos = 0;
    while (pos <= options.size()) {
        size_t e = options.find('\n', pos);
        if (e == std::string::npos) {
            e = options.size();
        }
        const std::string entry = options.substr(pos, e - pos);
        if (!entry.empty()) {
            entries.push_back(entry);
        }
        if (e == options.size()) {
            break;
        }
        pos = e + 1;
    }
    std::sort(entries.begin(), entries.end());

    std::string out;
    for (const auto & en : entries) {
        if (!out.empty()) {
            out += '\n';
        }
        out += en;
    }
    return out;
}

}  // namespace

long long calls() { return g_calls; }

fg_model::pred_t pred(int status, long long exit_code) {
    init();
    ++g_calls;
    int64_t out[4] = {-1, -1, -1, -1};
    (void) mg_pred((int64_t) status, (int64_t) exit_code,
                   sl_i64{out, (int64_t) (sizeof out / sizeof out[0])});
    fg_model::pred_t p;
    p.ready = (int) out[0];
    p.running = (int) out[1];
    p.ready_or_sleep = (int) out[2];
    p.failed = (int) out[3];
    return p;
}

fg_model::unload_t unload_action(int mapped, int status) {
    init();
    ++g_calls;
    int64_t out[3] = {-1, -1, -1};
    (void) mg_unload_action((int64_t) (mapped ? 1 : 0), (int64_t) status,
                            sl_i64{out, (int64_t) (sizeof out / sizeof out[0])});
    fg_model::unload_t u;
    u.action = (int) out[0];
    u.send_exit = (int) out[1];
    u.terminate = (int) out[2];
    return u;
}

long long victim(const std::vector<fg_model::row_t> & rows) {
    init();
    ++g_calls;

    NameTable t;
    std::vector<int64_t> status, req, last, stopping, queued;
    for (const auto & r : rows) {
        t.add(r.name);
        status.push_back((int64_t) r.status);
        req.push_back(r.req_count);
        last.push_back(r.last_used);
        stopping.push_back((int64_t) r.stopping);
        queued.push_back((int64_t) r.queued);
    }
    return (long long) mg_victim(t.n(), t.names(), t.offs(), t.lens(), slv(status), slv(req),
                                 slv(last), slv(stopping), slv(queued));
}

std::string preset_norm(const std::string & options) {
    init();
    ++g_calls;
    const std::string src = canon_options(options);
    std::vector<uint8_t> out(8192, 0);
    const int64_t n = hr_preset_norm(sl(src), 0, (int64_t) src.size(),
                                     sl_u8{out.data(), (int64_t) out.size()});
    if (n <= 0) {
        return std::string();
    }
    return std::string((const char *) out.data(), (size_t) n);
}fg_model::unload_set_t unload_set(const std::vector<fg_model::model_t> & cur,
                                  const std::vector<fg_model::model_t> & next) {
    init();
    ++g_calls;

    // the current mapping: names, status, and one option text per row
    NameTable ct;
    std::vector<int64_t> status, coff, clen;
    std::string copt;
    for (const auto & m : cur) {
        const std::string o = canon_options(m.options);
        ct.add(m.name);
        status.push_back((int64_t) m.status);
        coff.push_back((int64_t) copt.size());
        clen.push_back((int64_t) o.size());
        copt += o;
    }

    // the new preset list: names and one option text per entry
    NameTable nt;
    std::vector<int64_t> poff, plen;
    std::string popt;
    for (const auto & m : next) {
        const std::string o = canon_options(m.options);
        nt.add(m.name);
        poff.push_back((int64_t) popt.size());
        plen.push_back((int64_t) o.size());
        popt += o;
    }

    std::vector<int64_t> out(1024, -1);
    std::vector<int64_t> reasons(1024, 0);
    const int64_t n = hr_unload_set(
        ct.n(), ct.names(), ct.offs(), ct.lens(), slv(status), sl(copt), slv(coff), slv(clen),
        nt.n(), nt.names(), nt.offs(), nt.lens(), sl(popt), slv(poff), slv(plen), slv(out),
        slv(reasons));

    fg_model::unload_set_t res;
    for (int64_t i = 0; i < n && i < (int64_t) out.size(); ++i) {
        res.rows.push_back(out[(size_t) i]);
        res.reasons.push_back((int) reasons[(size_t) i]);
    }
    return res;
}

fg_model::connector_t connector_target(int sibling, const std::vector<int> & dir_exists,
                                       const std::vector<std::string> & hosts) {
    init();
    ++g_calls;

    std::vector<int64_t> exists;
    exists.reserve(dir_exists.size());
    for (int e : dir_exists) {
        exists.push_back((int64_t) e);
    }
    const int64_t binary = cn_find_binary((int64_t) (sibling ? 1 : 0), (int64_t) exists.size(),
                                          slv(exists));

    NameTable ht;
    for (const auto & h : hosts) {
        ht.add(h);
    }
    std::vector<uint8_t> out(256, 0);
    const int64_t nh = cn_upstream_host(ht.n(), ht.names(), ht.offs(), ht.lens(),
                                        sl_u8{out.data(), (int64_t) out.size()});

    fg_model::connector_t c;
    c.binary = (long long) binary;
    c.host = nh > 0 ? std::string((const char *) out.data(), (size_t) nh) : std::string();
    return c;
}

}  // namespace fg_goose_model
