// Read-only probe: streaming pairs whose later message is NOT a prefix of the
// earlier one, then the real common_chat_msg_diff::compute_diffs over them.
// No weights, no template: a PEG grammar is built in memory.
#include "chat.h"
#include "chat-peg-parser.h"
#include "peg-parser.h"

#include <cstdio>
#include <string>
#include <vector>

static common_peg_arena make_py_parser() {
    return build_chat_peg_parser([](common_chat_peg_builder & p) {
        auto arg = p.tool_arg(
            p.tool_arg_open(p.literal("<p>")) +
            p.tool_arg_name(p.until("=")) +
            p.literal("=") +
            p.tool_arg_value(p.until(";")) +
            p.tool_arg_close(p.literal(";")));
        auto tool = p.tool(
            p.tool_open(p.literal("<tool>")) +
            p.tool_name(p.until("|")) +
            p.literal("|") +
            p.tool_args(p.zero_or_more(arg)) +
            p.tool_close(p.literal("</tool>")));
        return p.start() + p.content(p.until("<tool>")) + p.zero_or_more(tool) + p.end();
    });
}

static common_peg_arena make_json_parser() {
    return build_chat_peg_parser([](common_chat_peg_builder & p) {
        auto tool = p.tool(
            p.tool_open(p.literal("<tool_call>")) +
            p.tool_name(p.chars("[a-zA-Z_]", 1, 64)) +
            p.tool_args(p.until("</tool_call>")) +
            p.tool_close(p.literal("</tool_call>")));
        return p.start() + p.content(p.until("<tool_call>")) + p.zero_or_more(tool) + p.end();
    });
}

static std::string esc(const std::string & s) {
    std::string o;
    for (char c : s) {
        if (c == '\n') o += "\\n";
        else if (c == '\t') o += "\\t";
        else if (c == '\r') o += "\\r";
        else if (c == '"') o += "\\\"";
        else o += c;
    }
    return o;
}

static void print_msg(const common_chat_msg & m) {
    printf("content=[%s] reasoning=[%s] calls=%zu", esc(m.content).c_str(),
           esc(m.reasoning_content).c_str(), m.tool_calls.size());
    for (size_t i = 0; i < m.tool_calls.size(); i++) {
        printf(" [%zu id=[%s] name=[%s] args=[%s]]", i, esc(m.tool_calls[i].id).c_str(),
               esc(m.tool_calls[i].name).c_str(), esc(m.tool_calls[i].arguments).c_str());
    }
}

static void print_diffs(const common_chat_msg & a, const common_chat_msg & b) {
    try {
        auto diffs = common_chat_msg_diff::compute_diffs(a, b);
        printf("    diffs=%zu", diffs.size());
        for (size_t i = 0; i < diffs.size(); i++) {
            const auto & d = diffs[i];
            if (d.tool_call_index == std::string::npos) {
                if (!d.reasoning_content_delta.empty() || (d.content_delta.empty() && d.reasoning_content_delta.empty())) {
                    printf(" | r_delta=[%s]", esc(d.reasoning_content_delta).c_str());
                }
                if (!d.content_delta.empty()) printf(" | c_delta=[%s]", esc(d.content_delta).c_str());
            } else {
                printf(" | call[%zu] id=[%s] name=[%s] args_delta=[%s]",
                       d.tool_call_index, esc(d.tool_call_delta.id).c_str(),
                       esc(d.tool_call_delta.name).c_str(), esc(d.tool_call_delta.arguments).c_str());
            }
        }
        printf("\n");
    } catch (const std::exception & e) {
        printf("    THROW: %s\n", esc(e.what()).c_str());
    }
}

static void run_seq(const char * title, common_peg_arena arena,
                    const std::vector<std::string> & chunks) {
    printf("=== %s\n", title);
    common_chat_parser_params params;
    params.format = COMMON_CHAT_FORMAT_PEG_SIMPLE;
    params.parser = arena;

    std::string text;
    std::vector<common_chat_msg> msgs;
    for (size_t i = 0; i < chunks.size(); i++) {
        text += chunks[i];
        common_peg_parse_context ctx(text, COMMON_PEG_PARSE_FLAG_LENIENT);
        auto res = params.parser.parse(ctx);
        common_chat_msg m;
        try {
            m = common_chat_parse(common_chat_input(text), true, params);
        } catch (const std::exception & e) {
            printf(" step %zu chunk=[%s] THROW: %s\n", i + 1, esc(chunks[i]).c_str(), esc(e.what()).c_str());
            continue;
        }
        printf(" step %zu chunk=[%s] parse=%s end=%zu roots=%zu ", i + 1, esc(chunks[i]).c_str(),
               common_peg_parse_result_type_name(res.type), res.end, res.nodes.size());
        print_msg(m);
        printf("\n");
        msgs.push_back(m);
    }
    for (size_t i = 1; i < msgs.size(); i++) {
        printf("  diff %zu -> %zu\n", i - 1, i);
        print_diffs(msgs[i - 1], msgs[i]);
    }
}

int main() {
    run_seq("python container, split between backslash and quote",
            make_py_parser(),
            {"<tool>search|<p>arg={'a':'x\\", "'};</tool>"});
    run_seq("python container, split inside the quote",
            make_py_parser(),
            {"<tool>search|<p>arg={'a':'x", "y\"};</tool>"});
    run_seq("json tool args, split inside the string",
            make_json_parser(),
            {"Hi<tool_call>get_weather{\"city\":\"Par", "is\"}</tool_call>"});
    return 0;
}
