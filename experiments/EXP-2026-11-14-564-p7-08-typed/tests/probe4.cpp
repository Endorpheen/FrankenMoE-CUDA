// Probe 4: case corpus for the typed/string content direction.
// Prints one labeled native output per case; cases.md transcribes them verbatim.

#include "chat.h"
#include "json.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using json = common_json;

static std::string read_file(const std::string & p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static jinja::caps caps_of(const std::string & src) {
    jinja::lexer lexer;
    auto res  = lexer.tokenize(src);
    auto prog = jinja::parse_from_tokens(res);
    return jinja::caps_get(prog);
}

static common_chat_msg parts_msg(std::vector<common_chat_msg_content_part> p) {
    common_chat_msg m;
    m.role          = "user";
    m.content_parts = std::move(p);
    return m;
}

static void show_json(const char * label, const common_chat_msg & m, bool concat) {
    printf("CASE %s\n", label);
    printf("  in : parts={");
    for (const auto & p : m.content_parts) {
        printf("{%s,%s}", p.type.c_str(), p.text.c_str());
    }
    printf("} content='%s' concat=%d\n", m.content.c_str(), (int) concat);
    printf("  out: %s\n", common_chat_msgs_to_json_oaicompat({ m }, concat).dump().c_str());
}

int main() {
    // ---- A. only_string direction: common_chat_msgs_to_json_oaicompat(msgs, true)
    show_json("A1 text+text", parts_msg({ { "text", "first" }, { "text", "second" } }), true);
    show_json("A2 text+media", parts_msg({ { "text", "look:" }, { "media_marker", "<__media__>" } }), true);
    show_json("A3 media+text", parts_msg({ { "media_marker", "<__media__>" }, { "text", "describe" } }), true);
    show_json("A4 media only", parts_msg({ { "media_marker", "<__media__>" } }), true);
    show_json("A5 media+media", parts_msg({ { "media_marker", "<m1>" }, { "media_marker", "<m2>" } }), true);
    show_json("A6 unknown between texts", parts_msg({ { "text", "a" }, { "image_url", "u" }, { "text", "b" } }), true);
    show_json("A7 empty first text", parts_msg({ { "text", "" }, { "text", "b" } }), true);
    show_json("A8 text with own newline", parts_msg({ { "text", "a\n" }, { "text", "b" } }), true);

    // ---- B. typed direction via the caps-driven serializer (concat=false)
    show_json("B1 text+text", parts_msg({ { "text", "first" }, { "text", "second" } }), false);
    show_json("B2 media present flattens", parts_msg({ { "text", "look:" }, { "media_marker", "<__media__>" } }), false);
    show_json("B3 single text part", parts_msg({ { "text", "only" } }), false);

    // ---- C. only_typed direction: public apply() with a typed-only template
    printf("CASE C1 synthetic typed-only, flat string\n");
    {
        const std::string src = "{% for m in messages %}{{ m.content[0] | tojson }}{% endfor %}";
        auto c = caps_of(src);
        printf("  caps: string=%d typed=%d\n", (int) c.supports_string_content, (int) c.supports_typed_content);
        auto tmpls = common_chat_templates_init(nullptr, src);
        common_chat_msg flat;
        flat.role    = "user";
        flat.content = "hello";
        common_chat_templates_inputs in;
        in.messages             = { flat };
        in.add_generation_prompt = false;
        printf("  in : content='hello' (string)\n");
        printf("  out: %s\n", common_chat_templates_apply(tmpls.get(), in).prompt.str().c_str());
    }
    printf("CASE C2 synthetic typed-only, typed parts\n");
    {
        const std::string src = "{% for m in messages %}{{ m.content[0] | tojson }}{% endfor %}";
        auto tmpls = common_chat_templates_init(nullptr, src);
        common_chat_templates_inputs in;
        in.messages             = { parts_msg({ { "text", "hello" }, { "text", "world" } }) };
        in.add_generation_prompt = false;
        printf("  in : parts={text:hello},{text:world}\n");
        printf("  out: %s\n", common_chat_templates_apply(tmpls.get(), in).prompt.str().c_str());
    }

    // ---- D. caps table
    printf("CASE D1 caps\n");
    {
        auto q = caps_of(read_file("/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/models/templates/Qwen3.5-4B.jinja"));
        auto m = caps_of("{% for m in messages %}{{ m.content[0] | tojson }}{% endfor %}");
        auto k = caps_of(read_file("/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/models/templates/Reka-Edge.jinja"));
        printf("  Qwen3.5-4B: string=%d typed=%d\n", (int) q.supports_string_content, (int) q.supports_typed_content);
        printf("  Reka-Edge : string=%d typed=%d\n", (int) k.supports_string_content, (int) k.supports_typed_content);
        printf("  synthetic : string=%d typed=%d\n", (int) m.supports_string_content, (int) m.supports_typed_content);
    }

    // ---- E. render difference on a real template
    printf("CASE E1 Qwen3.5-4B, typed text parts\n");
    {
        auto tmpls = common_chat_templates_init(nullptr,
            read_file("/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/models/templates/Qwen3.5-4B.jinja"), "<s>", "</s>");
        common_chat_templates_inputs in;
        in.messages             = { parts_msg({ { "text", "first line" }, { "text", "second line" } }) };
        in.add_generation_prompt = false;
        printf("  out: %s\n", common_chat_templates_apply(tmpls.get(), in).prompt.str().c_str());
    }
    printf("CASE E2 Qwen3.5-4B, flat string with an explicit newline\n");
    {
        auto tmpls = common_chat_templates_init(nullptr,
            read_file("/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/models/templates/Qwen3.5-4B.jinja"), "<s>", "</s>");
        common_chat_msg flat;
        flat.role    = "user";
        flat.content = "first line\nsecond line";
        common_chat_templates_inputs in;
        in.messages             = { flat };
        in.add_generation_prompt = false;
        printf("  out: %s\n", common_chat_templates_apply(tmpls.get(), in).prompt.str().c_str());
    }

    // ---- F. parser round trip and rejection
    printf("CASE F1 parse text-only array\n");
    {
        auto p = common_chat_msgs_parse_oaicompat(json::parse(
            R"([{"role":"user","content":[{"type":"text","text":"a"},{"type":"text","text":"b"}]}])"));
        printf("  out: content='%s' parts=%zu json=%s\n", p[0].content.c_str(), p[0].content_parts.size(),
               common_chat_msgs_to_json_oaicompat(p, false).dump().c_str());
    }
    printf("CASE F2 parse image_url array\n");
    {
        try {
            common_chat_msgs_parse_oaicompat(
                json::parse(R"([{"role":"user","content":[{"type":"image_url","image_url":{"url":"data:x"}}]}])"));
            printf("  out: ACCEPTED\n");
        } catch (const std::exception & e) {
            printf("  out: REJECTED: %s\n", e.what());
        }
    }
    return 0;
}
