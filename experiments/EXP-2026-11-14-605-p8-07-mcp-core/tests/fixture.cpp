// Gate for the MCP session core (P8-07 S3).
//
// Native side: a transcription of tools/server/server-mcp.cpp (lines cited in
// each function) on nlohmann::json. The port side calls the exported functions.
// No process, thread or socket is involved.
#include "g.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using json = nlohmann::json;

extern "C" double gs_json_strtod(sl_u8 s) {
    std::string tmp((const char *) s.data, (size_t) s.len);
    return std::strtod(tmp.c_str(), nullptr);
}

static int cases = 0;
static int fails = 0;

static std::string cfg_lines_native(const std::string & text) {
    json j;
    try {
        j = json::parse(text);
    } catch (...) {
        return "";
    }
    std::string out;
    if (!j.is_object() || !j.contains("mcpServers") || !j.at("mcpServers").is_object()) {
        return out;
    }
    for (const auto & [name, cfg] : j.at("mcpServers").items()) {
        std::string command = cfg.value("command", std::string());
        if (command.empty()) {
            continue;
        }
        out += name;
        out += "|";
        out += command;
        out += "|";
        out += cfg.value("cwd", std::string());
        out += "|";
        out += std::to_string(cfg.value("timeout_ms", 30000));
        out += "|";
        bool first = true;
        if (cfg.contains("args") && cfg.at("args").is_array()) {
            for (const auto & a : cfg.at("args")) {
                if (!first) out += ",";
                out += a.get<std::string>();
                first = false;
            }
        }
        out += "|";
        first = true;
        if (cfg.contains("env") && cfg.at("env").is_object()) {
            for (const auto & [k, v] : cfg.at("env").items()) {
                if (!first) out += ";";
                out += k + "=" + v.get<std::string>();
                first = false;
            }
        }
        out += "\n";
    }
    return out;
}

// server-mcp.cpp:182
static std::string rpc_error_native(const std::string & text) {
    json resp;
    try {
        resp = json::parse(text);
    } catch (...) {
        return "unknown error";
    }
    if (resp.contains("error")) {
        const json & e = resp.at("error");
        if (e.is_object()) {
            return e.value("message", "unknown error");
        }
        if (e.is_string()) {
            return e.get<std::string>();
        }
    }
    return "unknown error";
}

// server-mcp.cpp:197
static std::string result_native(const std::string & text) {
    json result;
    try {
        result = json::parse(text);
    } catch (...) {
        result = json::object();
    }
    std::string out;
    if (result.contains("content") && result.at("content").is_array()) {
        for (const auto & part : result.at("content")) {
            if (part.is_object() && part.value("type", "") == "text") {
                if (!out.empty()) out += "\n";
                out += part.value("text", "");
            }
        }
    }
    if (result.is_object() && result.value("isError", false)) {
        return json{{"error", out.empty() ? "MCP tool returned an error" : out}}.dump();
    }
    return json{{"plain_text_response", out}}.dump();
}

// server-mcp.cpp:255
static std::string init_req_native(int64_t id) {
    return json{{"jsonrpc", "2.0"},
                {"id", id},
                {"method", "initialize"},
                {"params", {{"protocolVersion", "2024-11-05"},
                            {"capabilities", json::object()},
                            {"clientInfo", {{"name", "llama.cpp"}, {"version", "1.0"}}}}}}.dump();
}

static std::string notif_native() {
    return json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}.dump();
}

static std::string list_req_native(int64_t id) {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"method", "tools/list"}}.dump();
}

// server-mcp.cpp:316
static std::string call_req_native(int64_t id, const std::string & name, const std::string & args) {
    json a = json::parse(args);
    return json{{"jsonrpc", "2.0"},
                {"id", id},
                {"method", "tools/call"},
                {"params", {{"name", name}, {"arguments", a}}}}.dump();
}

// server-mcp.cpp:284
static std::string tools_native(const std::string & text) {
    json resp;
    try {
        resp = json::parse(text);
    } catch (...) {
        return "";
    }
    std::string out;
    if (!resp.contains("result")) return out;
    const json & result = resp.at("result");
    if (!result.contains("tools") || !result.at("tools").is_array()) return out;
    for (const auto & t : result.at("tools")) {
        out += t.value("name", "");
        out += "|";
        out += t.value("description", "");
        out += "|";
        out += t.contains("inputSchema") ? "y" : "n";
        out += "\n";
    }
    return out;
}

// server-mcp.cpp:215
static std::string verdict_native(int written, int has_id, int matches, int cancelled, int timed_out, int * rc) {
    if (!written) {
        *rc = 1;
        return json{{"error", {{"code", -32603}, {"message", "transport closed"}}}}.dump();
    }
    if (!has_id) {
        *rc = 0;
        return "";
    }
    if (matches) {
        *rc = 0;
        return "";
    }
    if (cancelled) {
        *rc = 1;
        return json{{"error", {{"code", -32603}, {"message", "cancelled"}}}}.dump();
    }
    if (timed_out) {
        *rc = 1;
        return json{{"error", {{"code", -32603}, {"message", "request timed out"}}}}.dump();
    }
    *rc = 1;
    return json{{"error", {{"code", -32603}, {"message", "transport closed"}}}}.dump();
}

static std::string call(int64_t (*fn)(sl_u8, sl_u8), const std::string & in) {
    static char buf[65536];
    sl_u8 s{(uint8_t *) (void *) in.data(), (int64_t) in.size()};
    sl_u8 o{(uint8_t *) buf, (int64_t) sizeof buf};
    int64_t n = fn(s, o);
    return std::string(buf, (size_t) n);
}

static std::string call_id(int64_t (*fn)(int64_t, sl_u8), int64_t id) {
    static char buf[65536];
    sl_u8 o{(uint8_t *) buf, (int64_t) sizeof buf};
    int64_t n = fn(id, o);
    return std::string(buf, (size_t) n);
}

static std::string call_id_name(int64_t (*fn)(int64_t, sl_u8, sl_u8, sl_u8), int64_t id,
                                const std::string & name, const std::string & args) {
    static char buf[65536];
    sl_u8 nn{(uint8_t *) (void *) name.data(), (int64_t) name.size()};
    sl_u8 aa{(uint8_t *) (void *) args.data(), (int64_t) args.size()};
    sl_u8 o{(uint8_t *) buf, (int64_t) sizeof buf};
    int64_t n = fn(id, nn, aa, o);
    return std::string(buf, (size_t) n);
}

static void eq(const char * name, const std::string & want, const std::string & got) {
    cases++;
    if (want == got) {
        printf("OK   %s\n", name);
        return;
    }
    fails++;
    printf("FAIL %s\n  native: %s\n  port:   %s\n", name, want.c_str(), got.c_str());
}

int main() {
    goose_init(0, nullptr);

    // config
    eq("cfg_empty", cfg_lines_native("{}"), call(mcp_config_lines, "{}"));
    eq("cfg_missing", cfg_lines_native("{\"x\":1}"), call(mcp_config_lines, "{\"x\":1}"));
    eq("cfg_not_object", cfg_lines_native("[]"), call(mcp_config_lines, "[]"));
    eq("cfg_two",
       cfg_lines_native("{\"mcpServers\":{\"b\":{\"command\":\"bb\",\"args\":[\"x\",\"y\"],\"env\":{\"K\":\"V\",\"L\":\"W\"}},"
                        "\"a\":{\"command\":\"aa\",\"cwd\":\"/tmp\",\"timeout_ms\":5}}}"),
       call(mcp_config_lines,
            "{\"mcpServers\":{\"b\":{\"command\":\"bb\",\"args\":[\"x\",\"y\"],\"env\":{\"K\":\"V\",\"L\":\"W\"}},"
            "\"a\":{\"command\":\"aa\",\"cwd\":\"/tmp\",\"timeout_ms\":5}}}"));
    eq("cfg_skip_nocmd",
       cfg_lines_native("{\"mcpServers\":{\"a\":{},\"b\":{\"command\":\"c\"}}}"),
       call(mcp_config_lines, "{\"mcpServers\":{\"a\":{},\"b\":{\"command\":\"c\"}}}"));

    // error mapping
    eq("err_obj", rpc_error_native("{\"error\":{\"message\":\"boom\"}}"),
       call(mcp_rpc_error, "{\"error\":{\"message\":\"boom\"}}"));
    eq("err_str", rpc_error_native("{\"error\":\"bad\"}"), call(mcp_rpc_error, "{\"error\":\"bad\"}"));
    eq("err_none", rpc_error_native("{}"), call(mcp_rpc_error, "{}"));
    eq("err_no_msg", rpc_error_native("{\"error\":{}}"), call(mcp_rpc_error, "{\"error\":{}}"));

    // result mapping
    eq("res_parts",
       result_native("{\"content\":[{\"type\":\"text\",\"text\":\"a\"},{\"type\":\"text\",\"text\":\"b\"}]}"),
       call(mcp_result_response, "{\"content\":[{\"type\":\"text\",\"text\":\"a\"},{\"type\":\"text\",\"text\":\"b\"}]}"));
    eq("res_error", result_native("{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"no\"}]}"),
       call(mcp_result_response, "{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"no\"}]}"));
    eq("res_error_empty", result_native("{\"isError\":true}"), call(mcp_result_response, "{\"isError\":true}"));
    eq("res_empty", result_native("{}"), call(mcp_result_response, "{}"));

    // builders
    eq("init_1", init_req_native(1), call_id(mcp_init_request, 1));
    eq("init_42", init_req_native(42), call_id(mcp_init_request, 42));
    eq("notif", notif_native(), std::string([] {
            static char b[4096];
            sl_u8 o{(uint8_t *) b, 4096};
            int64_t n = mcp_initialized_notification(o);
            return std::string(b, (size_t) n);
        }()));
    eq("list_7", list_req_native(7), call_id(mcp_list_tools_request, 7));
    eq("call_3", call_req_native(3, "add", "{\"a\":1,\"b\":2}"),
       call_id_name(mcp_call_tool_request, 3, "add", "{\"a\":1,\"b\":2}"));

    // tools
    eq("tools_two",
       tools_native("{\"result\":{\"tools\":[{\"name\":\"a\",\"description\":\"A\",\"inputSchema\":{}},"
                    "{\"name\":\"b\"}]}}"),
       call(mcp_parse_tools, "{\"result\":{\"tools\":[{\"name\":\"a\",\"description\":\"A\",\"inputSchema\":{}},"
                             "{\"name\":\"b\"}]}}"));
    eq("tools_none", tools_native("{\"result\":{}}"), call(mcp_parse_tools, "{\"result\":{}}"));

    // verdict
    const int v[6][5] = {{0, 1, 0, 0, 0}, {1, 0, 0, 0, 0}, {1, 1, 1, 0, 0},
                         {1, 1, 0, 1, 0}, {1, 1, 0, 0, 1}, {1, 1, 0, 0, 0}};
    for (int i = 0; i < 6; i++) {
        int rc_n = 0, rc_p = 0;
        std::string want = verdict_native(v[i][0], v[i][1], v[i][2], v[i][3], v[i][4], &rc_n);
        char b[4096];
        memset(b, 0, sizeof b);
        sl_u8 o{(uint8_t *) b, 4096};
        rc_p = (int) mcp_rpc_verdict(v[i][0], v[i][1], v[i][2], v[i][3], v[i][4], o);
        std::string got(b, strnlen(b, sizeof b));
        char name[32];
        snprintf(name, sizeof name, "verdict_%d", i);
        cases++;
        if (want == got && rc_n == rc_p) {
            printf("OK   %s\n", name);
        } else {
            fails++;
            printf("FAIL %s rc native=%d port=%d\n  native: %s\n  port:   %s\n",
                   name, rc_n, rc_p, want.c_str(), got.c_str());
        }
    }

    printf("FG_MCP cases=%d failures=%d\n", cases, fails);
    return fails == 0 ? 0 : 1;
}
