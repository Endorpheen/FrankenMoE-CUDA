#include "chat.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    std::ifstream fs(argv[1]);
    std::string source{std::istreambuf_iterator<char>(fs), std::istreambuf_iterator<char>()};
    auto t = common_chat_templates_init(nullptr, source, "", "<|maxwell_end|>");
    common_chat_templates_inputs in;
    in.reasoning_format=COMMON_REASONING_FORMAT_AUTO;
    in.use_jinja=true;
    common_chat_msg user;
    user.role="user";
    user.content="Ты тут?";
    in.messages.push_back(user);
    common_chat_session first(t.get(),nullptr,in);
    std::cout << "no_tools=" << common_chat_format_name(first.format()) << '\n';
    common_chat_tool tool;
    tool.name="test_capability";
    tool.description="A model-free tool schema fixture";
    tool.parameters=R"({"type":"object","properties":{}})";
    in.tools.push_back(tool);
    common_chat_session second(t.get(),nullptr,in);
    const auto & msg=second.finish(common_chat_input("<think>Plan.</think>\nОтвет."));
    std::cout << "tools_present=" << common_chat_format_name(second.format()) << " split=" << !msg.reasoning_content.empty() << '\n';
}
