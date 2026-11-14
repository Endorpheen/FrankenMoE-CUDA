// Hypothesis check: normalize_quotes_to_json across a chunk boundary that
// falls between '\' and '\'' in a Python single-quoted container.
#include "chat.h"
#include "chat-peg-parser.h"

#include <cstdio>
#include <string>
#include <vector>

static std::string map_args(const std::string & value_text) {
    common_peg_ast_arena arena;
    std::string open = "<tool_call>", name = "f", value = value_text;
    auto a = arena.add_node("", common_chat_peg_builder::TOOL_OPEN,  0, 11, open,  {});
    auto b = arena.add_node("", common_chat_peg_builder::TOOL_NAME, 11, 12, name,  {});
    auto c = arena.add_node("", common_chat_peg_builder::TOOL_ARG_VALUE, 12, 12 + (size_t) value.size(), value, {});
    std::vector<common_peg_ast_id> roots = { a, b, c };

    common_chat_msg msg;
    msg.role = "assistant";
    common_chat_peg_mapper mapper(msg);
    common_peg_parse_result r(COMMON_PEG_PARSE_RESULT_NEED_MORE_INPUT, 0, 12 + value.size(), roots);
    mapper.from_ast(arena, r);

    return msg.tool_calls.empty() ? std::string("<none>") : msg.tool_calls[0].arguments;
}

static void show(const char * label, const std::string & s) {
    printf("%s = \"", label);
    for (unsigned char c : s) {
        if (c == '"') printf("\\\"");
        else if (c == '\\') printf("\\\\");
        else putchar(c);
    }
    printf("\"\n");
}

int main() {
    std::string partial = "{\'a\':\'x\\";     // {'a':'x\   (ends right after backslash)
    std::string full    = "{\'a\':\'x\\\'}";  // {'a':'x\'}
    auto p = map_args(partial);
    auto f = map_args(full);
    show("partial", p);
    show("full   ", f);
    printf("prefix=%d\n", (int) (f.rfind(p, 0) == 0));
    return 0;
}
