// Probe 3: exact normalized JSON of the only_typed branch through public API.
//
// Template uses content[0], which makes caps_get report typed=true and
// string=false. Native then rewrites a string content into
// [{"type":"text","text":...}] before rendering, so content[0] is that object.
// Without the rewrite, content[0] of a string would be a single character.

#include "chat.h"
#include "json.h"

#include <cstdio>
#include <string>

static jinja::caps caps_of(const std::string & src) {
    jinja::lexer lexer;
    auto res  = lexer.tokenize(src);
    auto prog = jinja::parse_from_tokens(res);
    return jinja::caps_get(prog);
}

int main() {
    const std::string src = "{% for m in messages %}{{ m.content[0] | tojson }}{% endfor %}";
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
    return 0;
}
