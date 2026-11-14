// Gate for the MCP stdio integration (P8-07 S5b).
//
// Port side: one composed Goose unit (source/*.goose) that owns the protocol
// decisions; this file owns the child process, the pipes and the clock.
// Native side: a transcription of tools/server/server-mcp.cpp (lines cited at
// every function) driven over a second real process with the same pipes, so a
// scenario is compared end to end: frames sent, frames received, tool table,
// call response, last_error and diagnostics.
//
// The fake server is tests/fake_server.cpp; its flag list is the fault injector.
#include "g.h"

#include <nlohmann/json.hpp>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <map>
#include <poll.h>
#include <signal.h>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using json = nlohmann::json;

extern char ** environ;

// Goose runtime shims the string build does not ship (same as the decision
// core gate, EXP610): JSON number conversion and printf-style exponent format.
extern "C" double gs_json_strtod(sl_u8 s) {
    std::string t((const char *) s.data, (size_t) s.len);
    return std::strtod(t.c_str(), nullptr);
}

extern "C" int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    return std::snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
}

static int cases = 0;
static int fails = 0;
static int steps = 0;

// ── small helpers ──────────────────────────────────────────────────────────

static int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static std::string S(sl_u8 v) {
    return std::string((const char *) v.data, (size_t) v.len);
}

static sl_u8 U8(std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

static sl_u8 U8c(const char * p, size_t n) {
    return sl_u8{(uint8_t *) (void *) p, (int64_t) n};
}

static uint8_t g_out[1 << 17];

static sl_u8 OB() {
    return sl_u8{g_out, (int64_t) sizeof g_out};
}

static std::string OUT(int64_t n) {
    return std::string((const char *) g_out, (size_t) n);
}

static std::string cut(const std::string & s, size_t n = 160) {
    if (s.size() <= n) return s;
    return s.substr(0, n) + "...";
}

static void esc(std::string & out, const std::string & s) {
    for (unsigned char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c < 32 || c > 126) {
            char b[8];
            snprintf(b, sizeof b, "\\x%02x", c);
            out += b;
        } else out += (char) c;
    }
}

static void eq(const std::string & name, const std::string & want, const std::string & got) {
    cases++;
    if (want == got) {
        printf("OK %s\n", name.c_str());
        return;
    }
    fails++;
    std::string a, b;
    esc(a, want);
    esc(b, got);
    printf("FAIL %s\n  want: %s\n  got : %s\n", name.c_str(), cut(a).c_str(), cut(b).c_str());
}

static void eqi(const std::string & name, long long want, long long got) {
    eq(name, std::to_string(want), std::to_string(got));
}

// ── process and pipes (subproc.h create + mcp_write_all 537) ────────────────

struct Proc {
    bool ok = false;
    pid_t pid = -1;
    int in = -1;
    int out = -1;
    int err = -1;
};

// The spawn reports the child's exec failure through a close-on-exec pipe, the
// same contract as subprocess_option_search_user_path + sp.create() returning
// false.
static Proc spawn_proc(const std::vector<std::string> & argv,
                       const std::vector<std::string> & env, bool use_env) {
    Proc p;
    int pi[2], po[2], pe[2], st[2];
    if (pipe(pi) != 0 || pipe(po) != 0 || pipe(pe) != 0) return p;
    if (pipe2(st, O_CLOEXEC) != 0) return p;

    pid_t pid = fork();
    if (pid < 0) return p;
    if (pid == 0) {
        dup2(pi[0], 0);
        dup2(po[1], 1);
        dup2(pe[1], 2);
        close(pi[0]); close(pi[1]);
        close(po[0]); close(po[1]);
        close(pe[0]); close(pe[1]);
        close(st[0]);
        std::vector<char *> av;
        for (const auto & a : argv) av.push_back((char *) a.c_str());
        av.push_back(nullptr);
        if (use_env) {
            std::vector<char *> ev;
            for (const auto & e : env) ev.push_back((char *) e.c_str());
            ev.push_back(nullptr);
            execvpe(av[0], av.data(), ev.data());
        } else {
            execvp(av[0], av.data());
        }
        int err = errno;
        ssize_t w = write(st[1], &err, sizeof err);
        (void) w;
        _exit(127);
    }
    close(pi[0]);
    close(po[1]);
    close(pe[1]);
    close(st[1]);

    int err = 0;
    ssize_t r = read(st[0], &err, sizeof err);
    close(st[0]);
    if (r == (ssize_t) sizeof err) {
        close(pi[1]); close(po[0]); close(pe[0]);
        int status = 0;
        waitpid(pid, &status, 0);
        return p; // spawn failed, same as sp.create() == false
    }
    p.ok = true;
    p.pid = pid;
    p.in = pi[1];
    p.out = po[0];
    p.err = pe[0];
    return p;
}

// mcp_write_all 537-602: non-blocking, EINTR retries, EAGAIN polls, a hung-up
// pipe fails, a partial write is a failure.
static bool write_all_fd(int fd, const std::string & data) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    size_t total = 0;
    while (total < data.size()) {
        ssize_t n = write(fd, data.data() + total, data.size() - total);
        if (n > 0) {
            total += (size_t) n;
            continue;
        }
        if (n == 0) return false;
        if (errno == EINTR) continue;
        if (errno != EAGAIN && errno != EWOULDBLOCK) return false;
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        int pr = poll(&pfd, 1, 50);
        if (pr < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (pfd.revents & (POLLERR | POLLNVAL | POLLHUP)) return false;
    }
    return total == data.size();
}

static std::string drain_fd(int fd) {
    std::string acc;
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    for (;;) {
        char b[4096];
        ssize_t n = read(fd, b, sizeof b);
        if (n > 0) {
            acc.append(b, (size_t) n);
            continue;
        }
        if (n == 0) break;
        if (errno == EINTR) continue;
        break;
    }
    return acc;
}

// ── scenario ───────────────────────────────────────────────────────────────

struct Scn {
    const char * name = "";
    const char * flags = "";
    const char * args2 = "";
    const char * command = "fake_server_619";
    const char * env_tag = nullptr;
    int timeout_ms = 4000;
    int64_t max_line = 0;
    const char * call = "";
    const char * call_args = "{}";
    int kill_after = 0; // 0 none, 1 after tools/list, 2 after initialize
    const char * g_sent = nullptr;
    int g_warns = -1;
    const char * g_final = nullptr;
    const char * g_error = nullptr;
    const char * g_diag = nullptr;
    const char * g_tools = nullptr;
};

struct Run {
    bool spawned = false;
    std::vector<std::string> sent;
    std::vector<std::string> got;
    std::string tools;
    std::string final;
    std::string last_error;
    std::string diag;
    long long warns = 0;
};

static const char * TOOLS6 =
    "echo|echo text|y\n"
    "add|two text parts|y\n"
    "fail|isError with text|y\n"
    "fail_no_text|isError without text|y\n"
    "env|echo MCP_FAKE_TAG|y\n"
    "argv|echo the extra argument|y\n";

// sent frames already carry their newline; received lines do not
static std::string cat(const std::vector<std::string> & v) {
    std::string s;
    for (const auto & x : v) s += x;
    return s;
}

static std::string join(const std::vector<std::string> & v) {
    std::string s;
    for (const auto & x : v) {
        s += x;
        s += "\n";
    }
    return s;
}

// ── native side: transcription of server-mcp.cpp ────────────────────────────

namespace native {

// 182-195
static std::string rpc_error_message(const json & resp) {
    if (resp.contains("error")) {
        const json & e = resp.at("error");
        if (e.is_object()) return e.value("message", "unknown error");
        if (e.is_string()) return e.get<std::string>();
    }
    return "unknown error";
}

// 197-213
static json mcp_result_to_response(const json & result) {
    std::string text;
    if (result.contains("content") && result.at("content").is_array()) {
        for (const auto & part : result.at("content")) {
            if (part.is_object() && part.value("type", "") == "text") {
                if (!text.empty()) text += "\n";
                text += part.value("text", "");
            }
        }
    }
    if (result.is_object() && result.value("isError", false)) {
        return {{"error", text.empty() ? "MCP tool returned an error" : text}};
    }
    return {{"plain_text_response", text}};
}

// 426-460
static std::vector<std::string> build_env(const std::map<std::string, std::string> & overrides) {
    std::vector<std::string> env;
    if (environ) {
        for (char ** e = environ; *e; ++e) {
            std::string s = *e;
            size_t eq = s.find('=');
            std::string key = eq == std::string::npos ? s : s.substr(0, eq);
            if (overrides.find(key) == overrides.end()) env.push_back(s);
        }
    }
    for (const auto & kv : overrides) env.push_back(kv.first + "=" + kv.second);
    return env;
}

// 618-632 (bounded tail)
static std::string err_tail(const std::string & text, size_t cap) {
    std::string out;
    size_t t = 0;
    while (t < text.size()) {
        size_t e = text.find('\n', t);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(t, e - t);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) {
            out += line;
            out.push_back('\n');
            if (out.size() > cap) out.erase(0, out.size() - cap);
        }
        t = e + 1;
    }
    return out;
}

struct Transport {
    const Scn & s;
    Run & r;
    Proc proc;
    uint64_t next_id = 1; // server-mcp.h: reset to 1 per respawn
    bool initialized = false;
    std::string last_error;
    std::string err_text;
    std::string buf;
    bool skipping = false;
    bool eof = false;
    std::deque<std::string> lines;
    int64_t start = 0;
    int64_t cap = 0;

    int64_t deadline() const { return start + (int64_t) s.timeout_ms; }

    // mcp_pump_ndjson 24-129
    int fill() {
        for (;;) {
            if (!lines.empty()) return 0;
            if (eof) return 1;
            if (now_ms() >= deadline()) return 2;
            struct pollfd pfd;
            pfd.fd = proc.out;
            pfd.events = POLLIN;
            pfd.revents = 0;
            int pr = poll(&pfd, 1, 50);
            if (pr < 0) {
                if (errno == EINTR) continue;
                return 1;
            }
            if (pr == 0) continue;
            if (pfd.revents & (POLLERR | POLLNVAL)) return 1;
            char chunk[4096];
            ssize_t n = read(proc.out, chunk, sizeof chunk);
            if (n < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                return 1;
            }
            if (n == 0) {
                eof = true;
                continue;
            }
            buf.append(chunk, (size_t) n);
            if (skipping) {
                size_t nl = buf.find('\n');
                if (nl == std::string::npos) {
                    if ((int64_t) buf.size() > cap) buf.clear();
                    continue;
                }
                buf.erase(0, nl + 1);
                skipping = false;
            }
            size_t pos;
            while ((pos = buf.find('\n')) != std::string::npos) {
                std::string line = buf.substr(0, pos);
                buf.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;
                r.got.push_back(line);
                lines.push_back(line);
            }
            if ((int64_t) buf.size() > cap) {
                r.warns++;
                buf.clear();
                skipping = true;
            }
        }
    }

    // 215-253
    json send_rpc(const json & request) {
        const std::string frame = request.dump();
        if (!write_all_fd(proc.in, frame + "\n")) {
            return {{"error", {{"code", -32603}, {"message", "transport closed"}}}};
        }
        const bool has_id = request.contains("id");
        start = now_ms();
        for (;;) {
            std::string line;
            int st = fill();
            if (st != 0) {
                if (st == 2) return {{"error", {{"code", -32603}, {"message", "request timed out"}}}};
                return {{"error", {{"code", -32603}, {"message", "transport closed"}}}};
            }
            line = lines.front();
            lines.pop_front();
            json reply;
            try {
                reply = json::parse(line);
            } catch (...) {
                if (now_ms() >= deadline()) return {{"error", {{"code", -32603}, {"message", "request timed out"}}}};
                continue;
            }
            if (!has_id || (reply.contains("id") && reply.at("id") == request.at("id"))) {
                return reply;
            }
            if (now_ms() >= deadline()) {
                return {{"error", {{"code", -32603}, {"message", "request timed out"}}}};
            }
        }
    }

    bool send_frame(const std::string & text) { return write_all_fd(proc.in, text + "\n"); }

    void sent_frame(const json & j) { r.sent.push_back(j.dump() + "\n"); }

    // 255-282
    bool ensure_init() {
        if (initialized) return true;
        json init_req = {
            {"jsonrpc", "2.0"},
            {"id", next_id++},
            {"method", "initialize"},
            {"params", {{"protocolVersion", "2024-11-05"},
                        {"capabilities", json::object()},
                        {"clientInfo", {{"name", "llama.cpp"}, {"version", "1.0"}}}}},
        };
        sent_frame(init_req);
        json resp = send_rpc(init_req);
        if (!resp.contains("result")) {
            last_error = "initialize failed: " + rpc_error_message(resp);
            return false;
        }
        json notif = {{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}};
        r.sent.push_back(notif.dump() + "\n");
        send_frame(notif.dump());
        initialized = true;
        return true;
    }

    // 284-314
    void list_tools() {
        if (!ensure_init()) return;
        if (!r.tools.empty()) return;
        json req = {{"jsonrpc", "2.0"}, {"id", next_id++}, {"method", "tools/list"}};
        sent_frame(req);
        json resp = send_rpc(req);
        if (!resp.contains("result")) {
            last_error = "tools/list failed: " + rpc_error_message(resp);
            return;
        }
        const json & result = resp.at("result");
        if (result.contains("tools") && result.at("tools").is_array()) {
            for (const auto & t : result.at("tools")) {
                r.tools += t.value("name", "");
                r.tools += "|";
                r.tools += t.value("description", "");
                r.tools += "|";
                r.tools += t.contains("inputSchema") ? "y" : "n";
                r.tools += "\n";
            }
        }
    }

    // 316-341
    json call_tool(const std::string & name, const json & args) {
        if (!ensure_init()) return {{"error", last_error}};
        json req = {{"jsonrpc", "2.0"},
                    {"id", next_id++},
                    {"method", "tools/call"},
                    {"params", {{"name", name}, {"arguments", args}}}};
        sent_frame(req);
        json resp = send_rpc(req);
        if (resp.contains("error")) return {{"error", rpc_error_message(resp)}};
        if (resp.contains("result")) return mcp_result_to_response(resp.at("result"));
        return {{"error", "invalid response from MCP server"}};
    }

    // 521-535
    std::string diagnostics() {
        std::string out = last_error;
        std::string tail = err_tail(err_text, 4096);
        if (!tail.empty()) {
            if (!out.empty()) out += "; ";
            out += "last stderr: " + tail;
        }
        return out;
    }
};

} // namespace native

// ── port side: the composed Goose unit owns every decision ──────────────────

struct Port {
    const Scn & s;
    Run & r;
    Proc proc;
    int64_t next_id = 1;
    bool initialized = false;
    std::string last_error;
    std::string err_text;
    uint8_t pump[65536];
    int64_t st[4] = {0, 0, 0, 0};
    std::deque<std::string> pending;
    bool eof = false;
    int64_t start = 0;
    int64_t cap = 0;

    // ── Goose calls ────────────────────────────────────────────────────────

    std::string method_of(int64_t stage) {
        std::string o = OUT(mi_next_method(stage, OB()));
        return o;
    }

    std::string frame_of(const std::string & msg) {
        std::string m = msg;
        int64_t n = mi_frame(U8(m), OB());
        if (n <= 0) return std::string();
        return OUT(n);
    }

    int64_t match(int64_t has_id, int64_t id, const std::string & line) {
        std::string l = line;
        return mi_match(has_id, id, U8(l));
    }

    std::string stop_text(int64_t cancelled, int64_t timed_out) {
        return OUT(mi_stop_text(cancelled, timed_out, OB()));
    }

    int64_t deadline_hit(int64_t elapsed) {
        return mi_deadline_hit(elapsed, (int64_t) s.timeout_ms);
    }

    int64_t call_kind(const std::string & reply) {
        std::string x = reply;
        return mi_call_kind(U8(x));
    }

    std::string diag_text(const std::string & last, const std::string & tail) {
        std::string a = last;
        std::string b = tail;
        return OUT(mi_diag(U8(a), U8(b), OB()));
    }

    // errlog_loop 618-632: every line is appended with a newline and the tail
    // keeps the last 4096 bytes
    std::string err_tail_of(const std::string & text) {
        std::string tail;
        size_t t = 0;
        while (t < text.size()) {
            size_t e = text.find('\n', t);
            if (e == std::string::npos) e = text.size();
            std::string line = text.substr(t, e - t);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) {
                std::string cur = tail;
                std::string l = line;
                tail = OUT(mi_err_tail(U8(cur), U8(l), 4096, OB()));
            }
            t = e + 1;
        }
        return tail;
    }

    std::string rpc_error(const std::string & resp) {
        std::string x = resp;
        return OUT(mcp_rpc_error(U8(x), OB()));
    }

    std::string tool_table(const std::string & result) {
        std::string x = result;
        return OUT(mcp_parse_tools(U8(x), OB()));
    }

    std::string call_response(const std::string & result) {
        std::string x = result;
        return OUT(mcp_result_response(U8(x), OB()));
    }

    std::string init_request(int64_t id) {
        return OUT(mcp_init_request(id, OB()));
    }

    std::string list_request(int64_t id) {
        return OUT(mcp_list_tools_request(id, OB()));
    }

    std::string call_request(int64_t id, const std::string & name, const std::string & args) {
        std::string n = name;
        std::string a = args;
        return OUT(mcp_call_tool_request(id, U8(n), U8(a), OB()));
    }

    std::string notif_text() {
        return OUT(mcp_initialized_notification(OB()));
    }

    int64_t next_id_of(int64_t cur) {
        return mi_next_id(cur);
    }

    // ── pump (mi_lines over live poll chunks) ──────────────────────────────

    void decode(const std::string & text) {
        size_t t = 0;
        while (t < text.size()) {
            size_t nl = text.find('\n', t);
            if (nl == std::string::npos) break;
            long long len = strtoll(text.c_str() + t, nullptr, 10);
            size_t at = nl + 1;
            std::string line = text.substr(at, (size_t) len);
            r.got.push_back(line);
            pending.push_back(line);
            t = at + (size_t) len;
        }
    }

    int fill() {
        for (;;) {
            if (!pending.empty()) return 0;
            if (eof) return 1;
            if (deadline_hit(now_ms() - start)) return 2;
            struct pollfd pfd;
            pfd.fd = proc.out;
            pfd.events = POLLIN;
            pfd.revents = 0;
            int pr = poll(&pfd, 1, 50);
            if (pr < 0) {
                if (errno == EINTR) continue;
                return 1;
            }
            if (pr == 0) continue;
            if (pfd.revents & (POLLERR | POLLNVAL)) return 1;
            char chunk[4096];
            ssize_t n = read(proc.out, chunk, sizeof chunk);
            if (n < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                return 1;
            }
            if (n == 0) {
                eof = true;
                continue;
            }
            sl_u8 buf{pump, (int64_t) sizeof pump};
            sl_i64 stt{st, 4};
            mi_lines(buf, stt, U8c(chunk, (size_t) n), cap, OB());
            r.warns = st[2];
            decode(OUT(st[3]));
        }
    }

    // ── requests ───────────────────────────────────────────────────────────

    std::string build(int64_t stage, const std::string & name, const std::string & args) {
        std::string m = method_of(stage);
        if (m == "initialize") {
            std::string t = init_request(next_id);
            next_id = next_id_of(next_id);
            return t;
        }
        if (m == "tools/list") {
            std::string t = list_request(next_id);
            next_id = next_id_of(next_id);
            return t;
        }
        if (m == "tools/call") {
            std::string t = call_request(next_id, name, args);
            next_id = next_id_of(next_id);
            return t;
        }
        if (m == "notifications/initialized") {
            return notif_text();
        }
        std::string t = "{\"id\":" + std::to_string(next_id) + ",\"jsonrpc\":\"2.0\",\"method\":\"" + m + "\"}";
        next_id = next_id_of(next_id);
        return t;
    }

    std::string send_text(const std::string & text, std::string & err) {
        err.clear();
        const std::string frame = frame_of(text);
        r.sent.push_back(frame.empty() ? text : frame);
        if (frame.empty() || !write_all_fd(proc.in, frame)) {
            err = stop_text(0, 0);
            return std::string();
        }
        json req;
        bool has_id = false;
        int64_t id = 0;
        try {
            req = json::parse(text);
            has_id = req.contains("id");
            id = has_id ? req.at("id").get<int64_t>() : 0;
        } catch (...) {
        }
        start = now_ms();
        for (;;) {
            int stt = fill();
            if (stt == 2) {
                err = stop_text(0, 1);
                return std::string();
            }
            if (stt == 1) {
                err = stop_text(0, 0);
                return std::string();
            }
            std::string line = pending.front();
            pending.pop_front();
            if (match(has_id ? 1 : 0, id, line) != 0) return line;
            if (deadline_hit(now_ms() - start)) {
                err = stop_text(0, 1);
                return std::string();
            }
        }
    }

    // 255-282
    bool ensure_init() {
        if (initialized) return true;
        std::string err;
        std::string reply = send_text(build(0, "", "{}"), err);
        if (reply.empty()) {
            last_error = "initialize failed: " + err;
            return false;
        }
        bool has_result = false;
        try {
            has_result = json::parse(reply).contains("result");
        } catch (...) {
        }
        if (!has_result) {
            last_error = "initialize failed: " + (err.empty() ? rpc_error(reply) : err);
            return false;
        }
        const std::string nm = method_of(1);
        std::string text = nm == "notifications/initialized"
                               ? notif_text()
                               : "{\"jsonrpc\":\"2.0\",\"method\":\"" + nm + "\"}";
        const std::string frame = frame_of(text); // written, not read
        r.sent.push_back(frame.empty() ? text : frame);
        if (!frame.empty()) write_all_fd(proc.in, frame);
        initialized = true;
        return true;
    }

    // 284-314
    void list_tools() {
        if (!ensure_init()) return;
        if (!r.tools.empty()) return;
        std::string err;
        std::string reply = send_text(build(2, "", "{}"), err);
        if (reply.empty()) {
            last_error = "tools/list failed: " + err;
            return;
        }
        json j;
        try {
            j = json::parse(reply);
        } catch (...) {
            last_error = "tools/list failed: unknown error";
            return;
        }
        if (!j.contains("result")) {
            last_error = "tools/list failed: " + rpc_error(reply);
            return;
        }
        // mcp_parse_tools takes the whole response and reads result.tools
        r.tools = tool_table(reply);
    }

    // 316-341
    json call_tool(const std::string & name, const std::string & args) {
        if (!ensure_init()) return {{"error", last_error}};
        std::string err;
        std::string reply = send_text(build(3, name, args), err);
        if (reply.empty()) return {{"error", err}};
        json j;
        try {
            j = json::parse(reply);
        } catch (...) {
            return {{"error", "invalid response from MCP server"}};
        }
        const int64_t kind = call_kind(reply);
        if (kind == 0) return {{"error", rpc_error(reply)}};
        if (kind == 1 && j.contains("result")) {
            return json::parse(call_response(j.at("result").dump()));
        }
        return {{"error", "invalid response from MCP server"}};
    }

    // 521-535
    std::string diagnostics() {
        return diag_text(last_error, err_tail_of(err_text));
    }
};

// ── resolution and environment ─────────────────────────────────────────────

static std::vector<std::string> path_dirs(bool port_side) {
    std::vector<std::string> dirs;
    const char * p = getenv("PATH");
    if (!p) return dirs;
    std::string s = p;
    size_t t = 0;
    while (t <= s.size()) {
        size_t e = s.find(':', t);
        if (e == std::string::npos) e = s.size();
        if (e > t) {
            std::string d = s.substr(t, e - t);
            // the port keeps the native Windows join rule and already accepts
            // both separators; a trailing '/' makes the candidate a POSIX path
            if (port_side && (d.back() != '/' && d.back() != '\\')) d += '/';
            dirs.push_back(d);
        }
        if (e == s.size()) break;
        t = e + 1;
    }
    return dirs;
}

static std::string resolve_port(const std::string & command) {
    // ms_resolve_candidates takes the native ';'-separated directory list
    auto dirs = path_dirs(true);
    std::string d;
    for (const auto & x : dirs) {
        d += x;
        d += ';';
    }
    std::string c = command;
    std::string pe;
    std::string o = OUT(ms_resolve_candidates(U8(c), U8(d), U8(pe), OB()));
    size_t t = 0;
    while (t < o.size()) {
        size_t e = o.find('\n', t);
        std::string cand = o.substr(t, e - t);
        if (!cand.empty() && access(cand.c_str(), X_OK) == 0) return cand;
        if (e == std::string::npos) break;
        t = e + 1;
    }
    return command; // nothing exists: let the spawn report the error
}

static std::string parent_env_text() {
    std::string s;
    for (char ** e = environ; e && *e; ++e) {
        std::string x = *e;
        if (x.find('\n') != std::string::npos) continue;
        s += x;
        s += '\n';
    }
    return s;
}

static std::vector<std::string> env_port(const std::map<std::string, std::string> & ov) {
    std::string parent = parent_env_text();
    std::string over;
    for (const auto & kv : ov) {
        over += kv.first + "=" + kv.second + "\n";
    }
    std::string o = OUT(ms_env_build(U8(parent), U8(over), OB()));
    std::vector<std::string> out;
    size_t t = 0;
    while (t < o.size()) {
        size_t e = o.find('\n', t);
        if (e == std::string::npos) e = o.size();
        if (e > t) out.push_back(o.substr(t, e - t));
        t = e + 1;
    }
    return out;
}

// ── one run, either side ───────────────────────────────────────────────────

static Run run_scenario(const Scn & s, bool port_side) {
    Run r;
    // fake server argv: <command> <flags> [extra]; the flags and the extra
    // argument are the config's args, exactly what parse_cursor_format stores
    std::vector<std::string> argv;
    argv.push_back(port_side ? resolve_port(s.command) : std::string(s.command));
    argv.push_back(s.flags);
    if (s.args2 && *s.args2) argv.push_back(s.args2);

    std::map<std::string, std::string> ov;
    if (s.env_tag) ov["MCP_FAKE_TAG"] = s.env_tag;

    Proc proc;
    if (port_side) {
        auto env = env_port(ov);
        proc = spawn_proc(argv, env, true);
    } else {
        if (ov.empty()) {
            proc = spawn_proc(argv, {}, false);
        } else {
            proc = spawn_proc(argv, native::build_env(ov), true);
        }
    }
    if (!proc.ok) return r;
    r.spawned = true;

    const int64_t cap = s.max_line > 0 ? s.max_line : 8388608;

    auto finish = [&](const std::string & last_error, const std::string & err_text, const std::string & diag) {
        r.last_error = last_error;
        r.diag = diag;
    };

    if (port_side) {
        Port t{s, r, proc};
        t.cap = cap;
        if (s.kill_after == 2) {
            t.ensure_init();
            kill(proc.pid, SIGKILL);
            int status = 0;
            waitpid(proc.pid, &status, 0);
            proc.pid = -1;
        } else {
            t.ensure_init();
            t.list_tools();
        }
        if (s.kill_after == 1) {
            kill(proc.pid, SIGKILL);
            int status = 0;
            waitpid(proc.pid, &status, 0);
            proc.pid = -1;
        }
        if (s.call && *s.call) {
            r.final = t.call_tool(s.call, s.call_args).dump();
        }
        t.err_text = drain_fd(proc.err);
        finish(t.last_error, t.err_text, t.diagnostics());
        r.warns = t.st[2];
    } else {
        native::Transport t{s, r};
        t.proc = proc;
        t.cap = cap;
        if (s.kill_after == 2) {
            t.ensure_init();
            kill(proc.pid, SIGKILL);
            int status = 0;
            waitpid(proc.pid, &status, 0);
            proc.pid = -1;
        } else {
            t.ensure_init();
            t.list_tools();
        }
        if (s.kill_after == 1) {
            kill(proc.pid, SIGKILL);
            int status = 0;
            waitpid(proc.pid, &status, 0);
            proc.pid = -1;
        }
        if (s.call && *s.call) {
            r.final = t.call_tool(s.call, json::parse(s.call_args)).dump();
        }
        t.err_text = drain_fd(proc.err);
        finish(t.last_error, t.err_text, t.diagnostics());
    }

    // teardown: close the request pipe, then reap (join_pumps + sp.join)
    if (proc.in >= 0) close(proc.in);
    if (proc.out >= 0) close(proc.out);
    if (proc.err >= 0) close(proc.err);
    if (proc.pid > 0) {
        int status = 0;
        if (waitpid(proc.pid, &status, WNOHANG) == 0) {
            kill(proc.pid, SIGKILL);
            waitpid(proc.pid, &status, 0);
        }
    }
    return r;
}

static void step(const Scn & s) {
    steps++;
    printf("STEP %s\n", s.name);
    Run a = run_scenario(s, false);
    Run b = run_scenario(s, true);
    const std::string n = s.name;
    eqi(n + "/spawned", a.spawned ? 1 : 0, b.spawned ? 1 : 0);
    eq(n + "/sent", cat(a.sent), cat(b.sent));
    eq(n + "/got", join(a.got), join(b.got));
    eq(n + "/tools", a.tools, b.tools);
    eq(n + "/final", a.final, b.final);
    eq(n + "/error", a.last_error, b.last_error);
    eq(n + "/diag", a.diag, b.diag);
    eqi(n + "/warns", a.warns, b.warns);
    if (s.g_sent) eq(n + "/golden_sent", s.g_sent, cat(b.sent));
    if (s.g_warns >= 0) eqi(n + "/golden_warns", s.g_warns, b.warns);
    if (s.g_final) eq(n + "/golden_final", s.g_final, b.final);
    if (s.g_error) eq(n + "/golden_error", s.g_error, b.last_error);
    if (s.g_diag) eq(n + "/golden_diag", s.g_diag, b.diag);
    if (s.g_tools) eq(n + "/golden_tools", s.g_tools, b.tools);
}

// ── manager rows: get_or_create 763-819 ────────────────────────────────────

struct MgrRow {
    long long stopping;
    long long now;
    long long dead;
    long long ddl;
    long long have;
    long long alive;
    long long have_cfg;
    long long sok;
    long long salive;
};

struct MgrOut {
    int action = 5;
    int close_old = 0;
    int close_new = 0;
    long long cooldown = 0;
};

// transcription of server_mcp::get_or_create (763-819); action codes:
// 0 stopping, 1 cooldown blocked, 2 reuse, 3 started, 4 failed start, 5 no config
static MgrOut native_manager(const MgrRow & r) {
    MgrOut o;
    bool done = false;
    if (r.stopping) {
        o.action = 0;
        return o;
    }
    if (r.dead && r.now < r.ddl) {
        o.action = 1;
        return o;
    }
    if (r.have) {
        if (r.alive) {
            o.action = 2;
            return o;
        }
        o.close_old = 1;
    }
    if (!r.have_cfg) {
        o.action = 5;
        return o;
    }
    if (r.sok && r.salive) {
        o.action = 3;
        return o;
    }
    o.action = 4;
    o.close_new = 1;
    o.cooldown = r.now + 5000;
    (void) done;
    return o;
}

static std::string mgr_text(const MgrOut & o) {
    char b[128];
    snprintf(b, sizeof b, "action=%d close_old=%d close_new=%d cooldown=%lld",
             o.action, o.close_old, o.close_new, o.cooldown);
    return b;
}

static void manager_step() {
    steps++;
    printf("STEP manager_rows\n");
    const MgrRow rows[] = {
        {1, 1000, 0, 0, 1, 1, 1, 1, 1},        // stopping wins
        {0, 1000, 1, 6000, 0, 0, 1, 1, 1},     // active cooldown blocks
        {0, 6000, 1, 6000, 0, 0, 1, 1, 1},     // cooldown expires at the instant
        {0, 5999, 1, 6000, 0, 0, 1, 1, 1},     // just below the deadline
        {0, 1000, 0, 0, 1, 1, 1, 1, 1},        // live transport reused
        {0, 1000, 0, 0, 1, 0, 1, 1, 1},        // dead transport replaced
        {0, 7000, 1, 6000, 0, 0, 1, 0, 0},     // expired, start fails
        {0, 1000, 0, 0, 0, 0, 0, 1, 1},        // no config: nothing to start
        {0, 7000, 1, 6000, 0, 0, 1, 1, 1},     // expired, start ok
    };
    int i = 0;
    for (const auto & row : rows) {
        std::string name = "mgr_row" + std::to_string(i);
        char args[160];
        snprintf(args, sizeof args, "%lld %lld %lld %lld %lld %lld %lld %lld %lld",
                 row.stopping, row.now, row.dead, row.ddl, row.have, row.alive,
                 row.have_cfg, row.sok, row.salive);
        std::string a = args;
        std::string got = OUT(ms_manager_resolve(U8(a), OB()));
        eq(name, mgr_text(native_manager(row)), got);
        i++;
    }
}

// A real failed spawn: the manager must record the cooldown and then block.
static void spawn_failure_step() {
    steps++;
    printf("STEP spawn_failure\n");
    Scn s;
    s.name = "missing_command";
    s.command = "absent-server-619";
    s.call = "echo";
    Run a = run_scenario(s, false);
    Run b = run_scenario(s, true);
    eqi("missing/spawned_native", 0, a.spawned ? 1 : 0);
    eqi("missing/spawned_port", 0, b.spawned ? 1 : 0);
    eqi("missing/no_frames_native", 0, (long long) a.sent.size());
    eqi("missing/no_frames_port", 0, (long long) b.sent.size());

    // the failed start recorded now+5000; a retry before it is blocked
    const long long now = 100000;
    char args[160];
    snprintf(args, sizeof args, "0 %lld 1 %lld 0 0 1 0 0", now, now + 5000);
    std::string a2 = args;
    std::string blocked = OUT(ms_manager_resolve(U8(a2), OB()));
    eq("missing/cooldown_blocks", "action=1 close_old=0 close_new=0 cooldown=0", blocked);

    snprintf(args, sizeof args, "0 %lld 1 %lld 0 0 1 1 1", now + 5001, now + 5000);
    std::string a3 = args;
    std::string fresh = OUT(ms_manager_resolve(U8(a3), OB()));
    eq("missing/cooldown_expired", "action=3 close_old=0 close_new=0 cooldown=0", fresh);
}

// The composed unit carries every accepted server module, not just the MCP one.
static void unit_step() {
    steps++;
    printf("STEP unit_symbols\n");
    std::string o = OUT(tool_name(0, OB()));
    eq("unit/tool_name0", "read_file", o);
    eqi("unit/schema_rows", 63, schema_row_count());
    std::string f = OUT(dc_py_float_of(1.5, OB()));
    eq("unit/py_float", "1.5", f);
    std::string l = OUT(ms_limits(OB()));
    eq("unit/limits",
       "cooldown=5\nwarmup=10\npoll_ms=50\nmax_line=8388608\nerr_tail=4096\ntransport=stdio\n", l);
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    goose_init(0, nullptr);

    Scn s;

    s = Scn();
    s.name = "success";
    s.flags = "none";
    s.call = "echo";
    s.call_args = "{\"text\":\"pong\"}";
    s.g_final = "{\"plain_text_response\":\"pong\"}";
    s.g_tools = TOOLS6;
    s.g_error = "";
    // the canonical exchange of the native ensure_init/list_tools/call_tool
    s.g_sent =
        "{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":"
        "{\"capabilities\":{},\"clientInfo\":{\"name\":\"llama.cpp\",\"version\":\"1.0\"},"
        "\"protocolVersion\":\"2024-11-05\"}}\n"
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n"
        "{\"id\":2,\"jsonrpc\":\"2.0\",\"method\":\"tools/list\"}\n"
        "{\"id\":3,\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":"
        "{\"arguments\":{\"text\":\"pong\"},\"name\":\"echo\"}}\n";
    step(s);

    s = Scn();
    s.name = "multi_text";
    s.flags = "none";
    s.call = "add";
    s.g_final = "{\"plain_text_response\":\"1\\n2\"}";
    step(s);

    s = Scn();
    s.name = "iserror";
    s.flags = "none";
    s.call = "fail";
    s.g_final = "{\"error\":\"boom\"}";
    step(s);

    s = Scn();
    s.name = "iserror_empty";
    s.flags = "none";
    s.call = "fail_no_text";
    s.g_final = "{\"error\":\"MCP tool returned an error\"}";
    step(s);

    s = Scn();
    s.name = "unknown_tool";
    s.flags = "none";
    s.call = "nosuch";
    s.g_final = "{\"error\":\"unknown tool: nosuch\"}";
    step(s);

    s = Scn();
    s.name = "init_error";
    s.flags = "init_error";
    s.call = "echo";
    s.g_error = "initialize failed: init boom";
    s.g_final = "{\"error\":\"initialize failed: init boom\"}";
    s.g_tools = "";
    step(s);

    s = Scn();
    s.name = "empty_list";
    s.flags = "empty_list";
    s.call = "echo";
    s.g_tools = "";
    s.g_final = "{\"plain_text_response\":\"pong\"}";
    step(s);

    s = Scn();
    s.name = "timeout_init";
    s.flags = "slow";
    s.timeout_ms = 150;
    s.call = "echo";
    s.g_error = "initialize failed: request timed out";
    step(s);

    s = Scn();
    s.name = "timeout_list";
    s.flags = "late";
    s.timeout_ms = 150;
    s.g_error = "tools/list failed: request timed out";
    s.g_tools = "";
    step(s);

    s = Scn();
    s.name = "killed_before_call";
    s.flags = "stderr";
    s.kill_after = 1;
    s.call = "echo";
    s.g_final = "{\"error\":\"transport closed\"}";
    s.g_diag = "last stderr: fake: boot\n";
    s.g_tools = TOOLS6;
    step(s);

    s = Scn();
    s.name = "die_after_notify";
    s.flags = "die_after_init";
    s.g_error = "tools/list failed: transport closed";
    s.g_tools = "";
    step(s);

    s = Scn();
    s.name = "malformed_then_ok";
    s.flags = "junk";
    s.call = "echo";
    s.g_tools = TOOLS6;
    s.g_final = "{\"plain_text_response\":\"pong\"}";
    step(s);

    s = Scn();
    s.name = "notification_first";
    s.flags = "notify";
    s.call = "echo";
    s.g_tools = TOOLS6;
    s.g_final = "{\"plain_text_response\":\"pong\"}";
    step(s);

    s = Scn();
    s.name = "stale_id";
    s.flags = "stale";
    s.call = "echo";
    s.g_tools = TOOLS6;
    s.g_final = "{\"plain_text_response\":\"pong\"}";
    step(s);

    s = Scn();
    s.name = "crlf";
    s.flags = "crlf";
    s.call = "echo";
    s.g_tools = TOOLS6;
    s.g_final = "{\"plain_text_response\":\"pong\"}";
    step(s);

    s = Scn();
    s.name = "chunked_reply";
    s.flags = "chunked";
    s.timeout_ms = 8000;
    s.call = "add";
    s.g_final = "{\"plain_text_response\":\"1\\n2\"}";
    step(s);

    s = Scn();
    s.name = "oversize_resync";
    s.flags = "bigsplit";
    s.max_line = 64;
    s.g_warns = 1;
    s.call = "echo";
    s.g_tools = TOOLS6;
    s.g_final = "{\"plain_text_response\":\"pong\"}";
    step(s);

    s = Scn();
    s.name = "blank_lines";
    s.flags = "blank";
    s.call = "echo";
    s.g_tools = TOOLS6;
    s.g_final = "{\"plain_text_response\":\"pong\"}";
    step(s);

    s = Scn();
    s.name = "eof_partial";
    s.flags = "eof_partial";
    s.g_error = "tools/list failed: transport closed";
    s.g_tools = "";
    step(s);

    s = Scn();
    s.name = "env_override";
    s.flags = "none";
    s.env_tag = "zz-619";
    s.call = "env";
    s.g_final = "{\"plain_text_response\":\"zz-619\"}";
    step(s);

    s = Scn();
    s.name = "argv_passthrough";
    s.flags = "none";
    s.args2 = "ARGV-619";
    s.call = "argv";
    s.g_final = "{\"plain_text_response\":\"ARGV-619\"}";
    step(s);

    s = Scn();
    s.name = "big_description";
    s.flags = "bigdesc";
    s.call = "echo";
    step(s);

    s = Scn();
    s.name = "stderr_tail";
    s.flags = "stderr_long";
    s.kill_after = 1;
    s.call = "echo";
    s.g_final = "{\"error\":\"transport closed\"}";
    step(s);

    spawn_failure_step();
    manager_step();
    unit_step();

    printf("FG_MCPINT steps=%d cases=%d failures=%d\n", steps, cases, fails);
    return fails == 0 ? 0 : 1;
}
