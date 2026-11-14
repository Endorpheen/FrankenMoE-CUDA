// Fake MCP server for the P8-07 S5b integration gate.
//
// One NDJSON request per line on stdin, one NDJSON response per line on stdout,
// diagnostics on stderr. argv[1] is a ':'-separated flag list that injects the
// faults the gate needs; argv[2], when present, is echoed by the `argv` tool.
//
// Flags: slow (delay the initialize reply), late (delay the tools/list reply),
// silent (never answer tools/list), init_error, empty_list, junk, big, blank,
// notify, stale, crlf, chunked, die_after_init, eof_partial, stderr,
// stderr_long, bigdesc.
//
// Protocol order is enforced: tools/list and tools/call are refused until
// notifications/initialized has been received, like a real MCP server.
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

using json = nlohmann::json;

static std::string g_flags;

static bool flag(const std::string & f) {
    const std::string s = ":" + g_flags + ":";
    return s.find(":" + f + ":") != std::string::npos;
}

static void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

static void raw(const std::string & text) {
    fwrite(text.data(), 1, text.size(), stdout);
    fflush(stdout);
}

// one NDJSON frame, honouring the crlf / chunked fault flags
static void emit(const std::string & text) {
    if (flag("chunked")) {
        const std::string frame = text + "\n";
        for (char c : frame) {
            fwrite(&c, 1, 1, stdout);
            fflush(stdout);
            sleep_ms(1);
        }
        return;
    }
    if (flag("crlf")) {
        raw(text + "\r\n");
        return;
    }
    raw(text + "\n");
}

static json text_part(const std::string & text) {
    return json{{"type", "text"}, {"text", text}};
}

static json text_result(const std::string & text) {
    return json{{"content", json::array({text_part(text)})}};
}

static void reply_result(long long id, const json & result) {
    emit(json{{"id", id}, {"jsonrpc", "2.0"}, {"result", result}}.dump());
}

static void reply_error(long long id, int code, const std::string & message) {
    emit(json{{"error", {{"code", code}, {"message", message}}}, {"id", id}, {"jsonrpc", "2.0"}}.dump());
}

static json tools_doc() {
    json list = json::array();
    auto add = [&](const char * name, const std::string & text) {
        std::string desc = text;
        if (flag("bigdesc") && std::string(name) == "echo") {
            desc.assign(2000, 'd');
        }
        list.push_back(json{{"description", desc}, {"inputSchema", json{{"type", "object"}}}, {"name", name}});
    };
    add("echo", "echo text");
    add("add", "two text parts");
    add("fail", "isError with text");
    add("fail_no_text", "isError without text");
    add("env", "echo MCP_FAKE_TAG");
    add("argv", "echo the extra argument");
    return json{{"tools", list}};
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        return 2;
    }
    g_flags = argv[1];
    const std::string extra = argc > 2 ? argv[2] : "";
    bool initialized = false;
    std::string line;

    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        json req;
        try {
            req = json::parse(line);
        } catch (...) {
            continue;
        }
        const std::string method = req.value("method", "");
        const bool has_id = req.contains("id") && req.at("id").is_number_integer();
        const long long id = has_id ? req.at("id").get<long long>() : 0;

        if (method == "notifications/initialized") {
            initialized = true;
            continue;
        }
        if (method == "initialize") {
            if (flag("slow")) {
                sleep_ms(400);
            }
            if (flag("init_error")) {
                reply_error(id, -32603, "init boom");
                continue;
            }
            reply_result(id, json{{"capabilities", json{{"tools", json::object()}}},
                                  {"protocolVersion", "2024-11-05"},
                                  {"serverInfo", json{{"name", "fake-mcp"}, {"version", "1.0"}}}});
            continue;
        }
        if (method == "tools/list") {
            if (flag("late")) {
                sleep_ms(400);
            }
            if (flag("silent")) {
                continue;
            }
            if (!initialized) {
                reply_error(id, -32601, "server not initialized");
                continue;
            }
            if (flag("die_after_init")) {
                fflush(stdout);
                _exit(0);
            }
            if (flag("stderr")) {
                fputs("fake: boot\n", stderr);
                fflush(stderr);
            }
            if (flag("stderr_long")) {
                for (int i = 0; i < 200; i++) {
                    fprintf(stderr, "fake stderr line %03d with padding\n", i);
                }
                fflush(stderr);
            }
            if (flag("junk")) {
                raw("this is not json\n");
            }
            if (flag("big")) {
                raw(std::string(400, 'x') + "\n");
            }
            if (flag("blank")) {
                raw("\n\r\n");
            }
            if (flag("notify")) {
                emit(json{{"jsonrpc", "2.0"}, {"method", "notifications/message"},
                          {"params", json::object()}}.dump());
            }
            if (flag("stale")) {
                emit(json{{"id", 999}, {"jsonrpc", "2.0"}, {"result", json::object()}}.dump());
            }
            if (flag("eof_partial")) {
                raw("{\"jsonrpc\":\"2.0\",");
                fflush(stdout);
                _exit(0);
            }
            if (flag("bigsplit")) {
                // an unterminated line larger than the pump cap: the client
                // must drop it and resynchronise at the newline that arrives
                // together with the real reply
                raw(std::string(400, 'x'));
                sleep_ms(150);
                raw("\n" + json{{"id", id}, {"jsonrpc", "2.0"}, {"result", tools_doc()}}.dump() + "\n");
                continue;
            }
            if (flag("empty_list")) {
                reply_result(id, json{{"tools", json::array()}});
                continue;
            }
            reply_result(id, tools_doc());
            continue;
        }
        if (method == "tools/call") {
            if (!initialized) {
                reply_error(id, -32601, "server not initialized");
                continue;
            }
            const json & params = req.at("params");
            const std::string name = params.value("name", "");
            const json args = params.value("arguments", json::object());
            if (name == "echo") {
                reply_result(id, text_result(args.value("text", "pong")));
            } else if (name == "add") {
                reply_result(id, json{{"content", json::array({text_part("1"), text_part("2")})}});
            } else if (name == "fail") {
                reply_result(id, json{{"content", json::array({text_part("boom")})}, {"isError", true}});
            } else if (name == "fail_no_text") {
                reply_result(id, json{{"isError", true}});
            } else if (name == "env") {
                const char * v = getenv("MCP_FAKE_TAG");
                reply_result(id, text_result(v ? v : "unset"));
            } else if (name == "argv") {
                reply_result(id, text_result(extra.empty() ? "none" : extra));
            } else {
                reply_error(id, -32602, "unknown tool: " + name);
            }
            continue;
        }
        if (has_id) {
            reply_error(id, -32601, "method not found: " + method);
        }
    }
    return 0;
}
