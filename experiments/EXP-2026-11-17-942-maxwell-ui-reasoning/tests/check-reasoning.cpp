#include "chat.h"
#include <nlohmann/json.hpp>

#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

using json = nlohmann::ordered_json;

static std::string read(const char * path) {
    std::ifstream in(path);
    assert(in.good());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static common_chat_templates_inputs inputs(common_reasoning_format format) {
    common_chat_templates_inputs in;
    in.use_jinja = true;
    in.reasoning_format = format;
    in.enable_thinking = true;
    common_chat_msg user;
    user.role = "user";
    user.content = "Ты тут?";
    in.messages.push_back(std::move(user));
    return in;
}

static void check(const common_chat_templates * tmpl, common_reasoning_format format,
                  const std::string & original, const std::string & reasoning, const std::string & answer) {
    auto in = inputs(format);
    common_chat_session final(tmpl, nullptr, in);
    assert(std::string(common_chat_format_name(final.format())) == "goose-maxwell");
    auto msg = final.finish(common_chat_input(original));
    assert(msg.reasoning_content == reasoning && msg.content == answer);
    json encoded = json::parse(msg.to_json_oaicompat().dump());
    if (!reasoning.empty()) {
        assert(encoded.at("reasoning_content") == reasoning);
    }

    common_chat_session stream(tmpl, nullptr, in);
    std::string streamed_content;
    std::string streamed_reasoning;
    auto old = stream.msg();
    auto append = [&](const common_chat_msg & now) {
        for (const auto & delta : common_chat_msg_diff::compute_diffs(old, now)) {
            streamed_content += delta.content_delta;
            streamed_reasoning += delta.reasoning_content_delta;
        }
        assert(streamed_content == now.content && streamed_reasoning == now.reasoning_content);
        old = now;
    };

    for (size_t off = 0; off < original.size();) {
        size_t next = off + 1;
        while (next < original.size() && (static_cast<unsigned char>(original[next]) & 0xc0) == 0x80) {
            ++next;
        }
        append(stream.feed(common_chat_input(original.substr(off, next - off))));
        off = next;
    }
    append(stream.finish());
    assert(streamed_content == answer && streamed_reasoning == reasoning);
}

int main(int argc, char ** argv) {
    assert(argc == 4);
    const bool candidate = std::string_view(argv[1]) == "candidate";
    auto mx = common_chat_templates_init(nullptr, read(argv[2]), "", "<|maxwell_end|>");
    const std::string raw = "<think>\nReason\n</think>\r\n\nОтвет";
    const auto in = inputs(COMMON_REASONING_FORMAT_DEEPSEEK);
    common_chat_session session(mx.get(), nullptr, in);
    auto mx_prompt = session.prompt().str();
    assert(mx_prompt == "<|maxwell_start|>user\nТы тут?<|maxwell_end|>\n<|maxwell_start|>assistant\n");
    const auto actual = session.finish(common_chat_input(raw));
    if (candidate) {
        assert(actual.reasoning_content == "\nReason\n" && actual.content == "Ответ");
        check(mx.get(), COMMON_REASONING_FORMAT_AUTO, raw + "<|maxwell_end|>", "\nReason\n", "Ответ");
        check(mx.get(), COMMON_REASONING_FORMAT_DEEPSEEK, " \n<think>partial </th", "partial ", "");
        check(mx.get(), COMMON_REASONING_FORMAT_AUTO, "\n<think>Незакрыто", "Незакрыто", "");
        check(mx.get(), COMMON_REASONING_FORMAT_AUTO, "\n<thi", "", "");
        check(mx.get(), COMMON_REASONING_FORMAT_AUTO, "<think></think>\nHello", "", "Hello");
        check(mx.get(), COMMON_REASONING_FORMAT_AUTO, "Plain <think> literal", "", "Plain <think> literal");
        check(mx.get(), COMMON_REASONING_FORMAT_AUTO, "Plain <tag", "", "Plain <tag");
        assert(common_chat_templates_support_enable_thinking(mx.get()));
        std::cerr << "PASS full response and monotonic streaming, split tags, truncation, UTF-8, EOG\n";
    } else {
        assert(actual.reasoning_content.empty() && actual.content.find("<think>") != std::string::npos);
        std::cerr << "PASS original backend reproduced raw think tags despite deepseek format\n";
    }

    common_chat_session disabled(mx.get(), nullptr, inputs(COMMON_REASONING_FORMAT_NONE));
    auto disabled_output = disabled.finish(common_chat_input(raw));
    assert(disabled_output.reasoning_content.empty() && disabled_output.content.find("<think>") != std::string::npos);

    auto apex = common_chat_templates_init(nullptr, read(argv[3]), "<|im_start|>", "<|im_end|>");
    common_chat_session apex_session(apex.get(), nullptr, in);
    auto apexfmt = common_chat_format_name(apex_session.format());
    const auto prefill = apex_session.generation_prompt();
    const std::string generated = prefill.find("<think>") == std::string::npos ?
            "<think>Work</think>\nOK" : "Work</think>\nOK";
    json apex_result;
    try {
        apex_result = json::parse(apex_session.finish(common_chat_input(generated)).to_json_oaicompat().dump());
    } catch (const std::exception & e) {
        apex_result = {{"parse_error", e.what()}};
    }
    std::cout << json{{"maxwell_prompt", mx_prompt}, {"maxwell_disabled", json::parse(disabled_output.to_json_oaicompat().dump())},
        {"apex_format", apexfmt}, {"apex_prompt", apex_session.prompt().str()}, {"apex_response", apex_result}}.dump() << '\n';
}
