// Read-only probe for FrankenMoE-CUDA / llama.cpp-flint-cuda.
// No weights, no templates: build a PEG chat grammar in memory and feed
// prefixes through common_chat_parse(), the same call the session makes.
#include "chat.h"
#include "chat-peg-parser.h"
#include "peg-parser.h"

#include <cstdio>
#include <string>
#include <vector>

static common_peg_arena make_stream_parser() {
    return build_chat_peg_parser([](common_chat_peg_builder & p) {
        auto tool = p.tool(
            p.tool_open(p.literal("<tool_call>")) +
            p.tool_name(p.chars("[a-zA-Z_]", 1, 64)) +
            p.tool_args(p.until("</tool_call>")) +
            p.tool_close(p.literal("</tool_call>")));
        auto reason = p.optional(
            p.literal("<think>") + p.reasoning(p.until("</think>")) + p.literal("</think>"));
        return p.start() + reason + p.content(p.until("<tool_call>")) + p.zero_or_more(tool) + p.end();
    });
}

static common_peg_arena make_fail_parser() {
    return build_chat_peg_parser([](common_chat_peg_builder & p) {
        return p.start() + p.content(p.until("</s>")) + p.literal("</s>") + p.end();
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
    printf("content=\"%s\" reasoning=\"%s\" calls=%zu",
           esc(m.content).c_str(), esc(m.reasoning_content).c_str(), m.tool_calls.size());
    for (size_t i = 0; i < m.tool_calls.size(); i++) {
        printf(" [%zu name=\"%s\" args=\"%s\" id=\"%s\"]", i,
               esc(m.tool_calls[i].name).c_str(),
               esc(m.tool_calls[i].arguments).c_str(),
               esc(m.tool_calls[i].id).c_str());
    }
    printf(" empty=%d", (int) m.empty());
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: probe <stream|fail> <is_partial> <text>\n");
        return 2;
    }
    const bool use_fail = std::string(argv[1]) == "fail";
    const bool is_partial = argc > 2 && std::string(argv[2]) == "1";
    const std::string text = argc > 3 ? argv[3] : "";

    common_peg_arena arena = use_fail ? make_fail_parser() : make_stream_parser();

    common_chat_parser_params params;
    params.format = use_fail ? COMMON_CHAT_FORMAT_PEG_SIMPLE : COMMON_CHAT_FORMAT_PEG_SIMPLE;
    params.parser = arena;

    // raw parse for the result type and whether any roots survived
    common_peg_parse_context ctx(text, COMMON_PEG_PARSE_FLAG_LENIENT);
    auto res = params.parser.parse(ctx);

    printf("t=%s end=%zu roots=%zu | ",
           common_peg_parse_result_type_name(res.type), res.end, res.nodes.size());
    common_chat_msg m;
    try {
        m = common_chat_parse(common_chat_input(text), is_partial, params);
        print_msg(m);
        printf("\n");
    } catch (const std::exception & e) {
        printf("THROW %s\n", e.what());
    }
    return 0;
}
