// Full common_chat_params oracle: calls common_chat_templates_apply() with
// use_jinja=false (legacy) and true (jinja) on the same template source.
// common_chat_templates_init(nullptr, src) only parses the template text, so no
// model or weights are required.
#include "chat.h"
#include "llama.h"

#include <cstdio>
#include <string>
#include <vector>

static std::string esc(const std::string & s, size_t max = 140) {
    std::string out;
    size_t n = 0;
    for (unsigned char c : s) {
        if (n++ > max) { out += "..."; break; }
        switch (c) {
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            case '\\': out += "\\\\"; break;
            default:
                if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\x%02x", c); out += b; }
                else out += (char) c;
        }
    }
    return out;
}

static void dump(const char * tag, const common_chat_params & p) {
    const std::string prompt = p.prompt.str();
    printf("  [%s] format=%s prompt_len=%zu grammar_len=%zu lazy=%d genp_len=%zu thinking=%d "
           "think_start=%d think_end=%d triggers=%zu preserved=%zu stops=%zu parser_len=%zu delims=%zu\n",
           tag, common_chat_format_name(p.format), prompt.size(), p.grammar.size(), (int) p.grammar_lazy,
           p.generation_prompt.size(), (int) p.supports_thinking,
           (int) !p.thinking_start_tag.empty(), (int) p.thinking_end_tags.size(),
           p.grammar_triggers.size(), p.preserved_tokens.size(), p.additional_stops.size(),
           p.parser.size(), p.message_delimiters.delimiters.size());
    printf("    prompt=%s\n", esc(prompt).c_str());
    if (!p.generation_prompt.empty()) printf("    genp=%s\n", esc(p.generation_prompt).c_str());
    if (!p.grammar.empty()) printf("    grammar=%s\n", esc(p.grammar, 100).c_str());
}

static std::string hexs(const std::string & s) {
    std::string o;
    for (unsigned char c : s) { char b[3]; snprintf(b, sizeof b, "%02x", c); o += b; }
    return o;
}

static int parts_tsv() {
    auto tmpls = common_chat_templates_init(nullptr, "chatml");
    for (int c = 0; c < 2; c++) {
        common_chat_msg m;
        m.role = "user";
        m.content = c == 0 ? "C" : "";
        m.content_parts.push_back({"text", "P1"});
        m.content_parts.push_back({"text", "P2"});
        m.content_parts.push_back({"media_marker", "<image>"});
        common_chat_templates_inputs in;
        in.messages = { m };
        in.add_generation_prompt = false;
        in.use_jinja = false;
        auto p = common_chat_templates_apply(tmpls.get(), in);
        const std::string prompt = p.prompt.str();
        printf("%s\t%d\t%zu\t%s\n", c == 0 ? "parts" : "parts-empty",
               (int) p.format, prompt.size(), hexs(prompt).c_str());
    }
    return 0;
}

int main(int argc, char ** argv) {
    if (argc > 1 && std::string(argv[1]) == "--parts-tsv") return parts_tsv();
    auto sm = [](const char * role, const char * content) {
        common_chat_msg m;
        m.role = role;
        m.content = content;
        return m;
    };
    const std::vector<common_chat_msg> msgs = {
        sm("system",    "You are a helpful assistant."),
        sm("user",      "Hello"),
        sm("assistant", "Hi there"),
        sm("user",      "Who are you?"),
    };

    // 1. Builtin name route: legacy accepts "chatml" etc. Jinja treats the same
    // string as a template source and renders it literally.
    for (const char * name : { "chatml", "llama3", "gemma" }) {
        printf("=== name %s ===\n", name);
        auto tmpls = common_chat_templates_init(nullptr, name);
        common_chat_templates_inputs in;
        in.messages = msgs;
        in.add_generation_prompt = true;
        in.use_jinja = false;
        dump("legacy", common_chat_templates_apply(tmpls.get(), in));
        try {
            in.use_jinja = true;
            dump("jinja", common_chat_templates_apply(tmpls.get(), in));
        } catch (const std::exception & e) {
            printf("  [jinja] exception: %s\n", e.what());
        }
        printf("\n");
    }

    // 2. Real template source: legacy detect+format vs jinja render.
    const std::vector<std::pair<const char *, const char *>> real = {
        { "llama3",   "<|start_header_id|>system<|end_header_id|>\n\nYou are a helpful assistant.<|eot_id|><|start_header_id|>user<|end_header_id|>\n\nHello<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\nHi there<|eot_id|><|start_header_id|>user<|end_header_id|>\n\nWho are you?<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n" },
        { "gemma",    "<start_of_turn>user\nHello<end_of_turn>\n<start_of_turn>model\nHi there<end_of_turn>\n<start_of_turn>user\nWho are you?<end_of_turn>\n<start_of_turn>model\n" },
        { "granite",  "<|start_of_role|>system<|end_of_role|>You are a helpful assistant.<|end_of_text|>\n<|start_of_role|>user<|end_of_role|>Hello<|end_of_text|>\n<|start_of_role|>assistant<|end_of_role|>Hi there<|end_of_text|>\n<|start_of_role|>user<|end_of_role|>Who are you?<|end_of_text|>\n<|start_of_role|>assistant<|end_of_role|>" },
    };
    // Template sources that the legacy detector recognizes, used as `tmpl`.
    const std::vector<std::pair<const char *, const char *>> srcs = {
        { "llama3",  "{% for message in messages %}<|start_header_id|>{{ message['role'] }}<|end_header_id|>\n\n{{ message['content'] }}<|eot_id|>{% endfor %}{% if add_generation_prompt %}<|start_header_id|>assistant<|end_header_id|>\n\n{% endif %}" },
        { "gemma",   "{% for message in messages %}<start_of_turn>{{ message['role'] }}\n{{ message['content'] }}<end_of_turn>\n{% endfor %}{% if add_generation_prompt %}<start_of_turn>model\n{% endif %}" },
        { "granite", "{% for message in messages %}<|start_of_role|>{{ message['role'] }}<|end_of_role|>{{ message['content'] }}<|end_of_text|>\n{% endfor %}{% if add_generation_prompt %}<|start_of_role|>assistant<|end_of_role|>{% endif %}" },
    };

    printf("=== real source, legacy vs jinja ===\n");
    for (const auto & kv : srcs) {
        printf("--- %s ---\n", kv.first);
        auto tmpls = common_chat_templates_init(nullptr, kv.second);
        common_chat_templates_inputs in;
        in.messages = msgs;
        in.add_generation_prompt = true;
        in.use_jinja = false;
        dump("legacy", common_chat_templates_apply(tmpls.get(), in));
        try {
            in.use_jinja = true;
            dump("jinja", common_chat_templates_apply(tmpls.get(), in));
        } catch (const std::exception & e) {
            printf("  [jinja] exception: %s\n", e.what());
        }
        printf("\n");
    }

    // 3. json_schema: both routes should produce the same grammar, but the
    // legacy path never fills anything else.
    {
        printf("=== json_schema ===\n");
        auto tmpls = common_chat_templates_init(nullptr,
            "{% for message in messages %}<|im_start|>{{ message['role'] }}\n{{ message['content'] }}<|im_end|>\n{% endfor %}{% if add_generation_prompt %}<|im_start|>assistant\n{% endif %}");
        common_chat_templates_inputs in;
        in.messages = msgs;
        in.add_generation_prompt = true;
        in.json_schema = R"({"type":"object","properties":{"x":{"type":"string"}},"required":["x"]})";
        in.use_jinja = false;
        dump("legacy", common_chat_templates_apply(tmpls.get(), in));
        try {
            in.use_jinja = true;
            dump("jinja", common_chat_templates_apply(tmpls.get(), in));
        } catch (const std::exception & e) {
            printf("  [jinja] exception: %s\n", e.what());
        }
        printf("\n");
    }

    // 4. Unsupported source: legacy throws, jinja renders.
    {
        printf("=== unsupported source ===\n");
        auto tmpls = common_chat_templates_init(nullptr, "{{ messages }}");
        common_chat_templates_inputs in;
        in.messages = msgs;
        in.add_generation_prompt = true;
        in.use_jinja = false;
        try {
            dump("legacy", common_chat_templates_apply(tmpls.get(), in));
        } catch (const std::exception & e) {
            printf("  [legacy] exception: %s\n", e.what());
        }
        try {
            in.use_jinja = true;
            dump("jinja", common_chat_templates_apply(tmpls.get(), in));
        } catch (const std::exception & e) {
            printf("  [jinja] exception: %s\n", e.what());
        }
    }
    (void) real;
    return 0;
}
