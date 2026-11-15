// Carrier of the model-management decisions (staged S3.1 trial): the native
// transcriptions and the dispatch. See fg-model-decide.h for provenance.
//
// The transcriptions copy the branch conditions, the predicates, the loop
// budgets and the strings of the native code; they do not link it. That is the
// same shape the accepted P8-06 gates used, and it is what keeps this gate free
// of models, children, threads and files.

#include "fg-model-decide.h"

#include <algorithm>
#include <map>

#ifdef FG_GOOSE_MODELS
#include "fg-goose-model-bridge.h"
#endif

namespace fg_model {

namespace {

// server-models.h 92-105
bool nat_is_ready(int s) { return s == kLoaded; }
bool nat_is_running(int s) { return s == kLoaded || s == kLoading || s == kSleeping; }
bool nat_is_ready_or_sleep(int s) { return s == kLoaded || s == kSleeping; }
bool nat_is_failed(int s, long long code) { return s == kUnloaded && code != 0; }

// preset_options_for_compare 633-638 plus the option-map comparison: entries
// are KEY=VALUE, keys unique, so the map is already the normalised form
std::string nat_norm(const std::string & opts) {
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
    for (const auto & kv : m) {
        if (!out.empty()) {
            out += '\n';
        }
        out += kv.first + "=" + kv.second;
    }
    return out;
}

}  // namespace

// --- native transcriptions --------------------------------------------------

// server-models.h 89-105
pred_t pred_native(int status, long long exit_code) {
    pred_t p;
    p.ready = nat_is_ready(status) ? 1 : 0;
    p.running = nat_is_running(status) ? 1 : 0;
    p.ready_or_sleep = nat_is_ready_or_sleep(status) ? 1 : 0;
    p.failed = nat_is_failed(status, exit_code) ? 1 : 0;
    return p;
}

// unload() 1172-1195
unload_t unload_action_native(int mapped, int status) {
    unload_t u;
    if (!mapped) {
        return u;
    }
    if (status == kDownloading) {
        u.action = 1;
        return u;
    }
    if (nat_is_running(status)) {
        u.action = 2;
        if (status == kLoading) {
            // special case: unloading a model that is still loading force-kills it
            u.terminate = 1;
        } else {
            u.send_exit = 1;
        }
        return u;
    }
    u.action = 3;
    return u;
}

// server_lru_sched::pick_victim 274-296 over the std::map name order.
// `victim` is the row index, -1 when every remaining model is busy, still
// coming up, already stopping or wanted by a queued request.
long long victim_native(const std::vector<row_t> & rows) {
    std::map<std::string, size_t> order;
    for (size_t i = 0; i < rows.size(); ++i) {
        order.emplace(rows[i].name, i);
    }

    long long victim = -1;
    long long victim_last_used = 0;

    for (const auto & kv : order) {
        const row_t & r = rows[kv.second];
        if (r.req_count != 0 || !nat_is_ready_or_sleep(r.status)) {
            continue;
        }
        if (r.stopping != 0 || r.queued != 0) {
            continue;
        }
        if (victim < 0 || r.last_used < victim_last_used) {
            victim = (long long) kv.second;
            victim_last_used = r.last_used;
        }
    }
    return victim;
}

std::string preset_norm_native(const std::string & options) {
    return nat_norm(options);
}

// load_models() reload branch 706-709: a running model goes down when the
// source dropped the name or its preset changed. Rows are emitted in name
// order, as the native map walk does.
unload_set_t unload_set_native(const std::vector<model_t> & cur,
                               const std::vector<model_t> & next) {
    std::map<std::string, std::string> fresh;
    for (const auto & m : next) {
        fresh.emplace(m.name, m.options);
    }

    std::map<std::string, size_t> order;
    for (size_t i = 0; i < cur.size(); ++i) {
        order.emplace(cur[i].name, i);
    }

    unload_set_t out;
    for (const auto & kv : order) {
        const model_t & r = cur[kv.second];
        if (!nat_is_running(r.status)) {
            continue;
        }
        auto it = fresh.find(r.name);
        if (it == fresh.end()) {
            out.rows.push_back((long long) kv.second);
            out.reasons.push_back(1);
        } else if (nat_norm(r.options) != nat_norm(it->second)) {
            out.rows.push_back((long long) kv.second);
            out.reasons.push_back(2);
        }
    }
    return out;
}

// upstream_host 128-150 and find_binary 136-165. The caller resolves "the
// sibling exists" and "this PATH entry has the binary" to flags, because the
// carrier must not touch the filesystem.
connector_t connector_target_native(int sibling, const std::vector<int> & dir_exists,
                                    const std::vector<std::string> & hosts) {
    connector_t c;

    if (sibling) {
        c.binary = 0;
    } else {
        c.binary = -1;
        for (size_t i = 0; i < dir_exists.size(); ++i) {
            if (dir_exists[i] != 0) {
                c.binary = (long long) i + 1;
                break;
            }
        }
    }

    for (const auto & host : hosts) {
        if (host.size() >= 5 && host.compare(host.size() - 5, 5, ".sock") == 0) {
            continue;
        }
        if (host == "0.0.0.0") {
            c.host = "127.0.0.1";
            return c;
        }
        if (host == "::") {
            c.host = "[::1]";
            return c;
        }
        if (host.find(':') != std::string::npos && host[0] != '[') {
            c.host = "[" + host + "]";
            return c;
        }
        c.host = host;
        return c;
    }
    return c;
}

// --- dispatch ---------------------------------------------------------------

pred_t pred(int status, long long exit_code) {
#ifdef FG_GOOSE_MODELS
    return fg_goose_model::pred(status, exit_code);
#else
    return pred_native(status, exit_code);
#endif
}

unload_t unload_action(int mapped, int status) {
#ifdef FG_GOOSE_MODELS
    return fg_goose_model::unload_action(mapped, status);
#else
    return unload_action_native(mapped, status);
#endif
}

long long victim(const std::vector<row_t> & rows) {
#ifdef FG_GOOSE_MODELS
    return fg_goose_model::victim(rows);
#else
    return victim_native(rows);
#endif
}

std::string preset_norm(const std::string & options) {
#ifdef FG_GOOSE_MODELS
    return fg_goose_model::preset_norm(options);
#else
    return preset_norm_native(options);
#endif
}

unload_set_t unload_set(const std::vector<model_t> & cur,
                        const std::vector<model_t> & next) {
#ifdef FG_GOOSE_MODELS
    return fg_goose_model::unload_set(cur, next);
#else
    return unload_set_native(cur, next);
#endif
}

connector_t connector_target(int sibling, const std::vector<int> & dir_exists,
                             const std::vector<std::string> & hosts) {
#ifdef FG_GOOSE_MODELS
    return fg_goose_model::connector_target(sibling, dir_exists, hosts);
#else
    return connector_target_native(sibling, dir_exists, hosts);
#endif
}

long long goose_calls() {
#ifdef FG_GOOSE_MODELS
    return fg_goose_model::calls();
#else
    return 0;
#endif
}

}  // namespace fg_model
