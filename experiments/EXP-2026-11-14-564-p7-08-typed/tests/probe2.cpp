// Probe 2: reach the only_typed branch of messages_inp_normalizer through a
// public API (no model weights).
//
// Path: common_chat_templates_init(nullptr, custom_template)
//       -> common_chat_templates_apply() -> render_message_to_json()
//       -> messages_inp_normalizer(caps).normalize()
//
// If the template's caps are string=false / typed=true, a *string* content
// message must be rewritten to [{type:"text",text:...}] before rendering.

#include "chat.h"
#include "json.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using json = common_json;

static std::string read_file(const std::string & path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static jinja::caps caps_of(const std::string & src) {
    jinja::lexer lexer;
    auto res = lexer.tokenize(src);
    auto prog = jinja::parse_from_tokens(res);
    return jinja::caps_get(prog);
}

int main() {
    const std::string dir = "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/models/templates/";

    // ---------------------------------------------------------------
    // 1. Scan every shipped template: which caps combinations occur?
    // ---------------------------------------------------------------
    printf("=== 1. caps combinations over models/templates ===\n");
    int n_total = 0, n_typed = 0, n_typed_only = 0, n_string_only = 0, n_both = 0;
    for (const auto & e : std::filesystem::directory_iterator(dir)) {
        if (e.path().extension() != ".jinja") {
            continue;
        }
        n_total++;
        try {
            auto c = caps_of(read_file(e.path().string()));
            if (c.supports_string_content && c.supports_typed_content) {
                n_both++;
            } else if (c.supports_typed_content) {
                n_typed_only++;
                printf("typed-only: %s\n", e.path().filename().string().c_str());
            } else if (c.supports_string_content) {
                n_string_only++;
            }
            if (c.supports_typed_content) {
                n_typed++;
            }
        } catch (const std::exception & ex) {
            printf("parse error: %s (%s)\n", e.path().filename().string().c_str(), ex.what());
        }
    }
    printf("templates=%d typed=%d both=%d typed_only=%d string_only=%d\n", n_total, n_typed, n_both, n_typed_only,
           n_string_only);

    // ---------------------------------------------------------------
    // 2. A synthetic typed-only template: does public apply() rewrite a
    //    string content into parts?
    // ---------------------------------------------------------------
    printf("\n=== 2. synthetic typed-only template ===\n");
    // Prints the part type and text; a bare string has no .type and no .text,
    // so with string content the template cannot produce this output.
    const std::string src =
        "{% for message in messages %}"
        "<{{ message.role }}>"
        "{% for part in message.content %}[{{ part.type }}:{{ part.text }}]{% endfor %}"
        "{% endfor %}";

    auto c = caps_of(src);
    printf("caps: string=%d typed=%d\n", (int) c.supports_string_content, (int) c.supports_typed_content);

    auto tmpls = common_chat_templates_init(nullptr, src);

    common_chat_msg flat;
    flat.role    = "user";
    flat.content = "hello";

    common_chat_msg typed;
    typed.role          = "user";
    typed.content_parts = { { "text", "hello" }, { "text", "world" } };

    for (auto * m : { &flat, &typed }) {
        common_chat_templates_inputs in;
        in.messages             = { *m };
        in.add_generation_prompt = false;
        printf("%s -> ", m == &flat ? "flat string" : "typed parts");
        try {
            printf("%s\n", common_chat_templates_apply(tmpls.get(), in).prompt.str().c_str());
        } catch (const std::exception & e) {
            printf("EXCEPTION: %s\n", e.what());
        }
    }

    // ---------------------------------------------------------------
    // 3. Same check on a real shipped template for contrast
    // ---------------------------------------------------------------
    printf("\n=== 3. real template (Qwen3.5-4B) same probe ===\n");
    auto qwen   = read_file(dir + "Qwen3.5-4B.jinja");
    auto qcaps  = caps_of(qwen);
    auto qtmpls = common_chat_templates_init(nullptr, qwen, "<s>", "</s>");
    printf("caps: string=%d typed=%d\n", (int) qcaps.supports_string_content, (int) qcaps.supports_typed_content);
    for (auto * m : { &flat, &typed }) {
        common_chat_templates_inputs in;
        in.messages             = { *m };
        in.add_generation_prompt = false;
        printf("%s -> ", m == &flat ? "flat string" : "typed parts");
        try {
            printf("%s\n", common_chat_templates_apply(qtmpls.get(), in).prompt.str().c_str());
        } catch (const std::exception & e) {
            printf("EXCEPTION: %s\n", e.what());
        }
    }

    return 0;
}
