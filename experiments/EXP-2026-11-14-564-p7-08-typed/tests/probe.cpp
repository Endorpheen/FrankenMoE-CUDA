// Probe: can typed content be gate-checked without model weights?
// Uses only prebuilt objects/libraries; no model is loaded.

#include "chat.h"
#include "json.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using json = common_json;

static std::string read_file(const std::string & path) {
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error("cannot read " + path);
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void section(const char * title) {
    printf("\n=== %s ===\n", title);
}

static common_chat_msg make(const std::string & role, std::vector<common_chat_msg_content_part> parts) {
    common_chat_msg m;
    m.role          = role;
    m.content_parts = std::move(parts);
    return m;
}

int main() {
    const std::string tmpl_dir = "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/models/templates/";

    // ---------------------------------------------------------------
    // 1. Serialization oracle: typed parts -> OAI JSON
    // ---------------------------------------------------------------
    section("1. common_chat_msgs_to_json_oaicompat: text-only parts (no media)");

    auto text_parts = make("user", {
        { "text", "first line" },
        { "text", "second line" },
    });
    auto text_media = make("user", {
        { "text",         "look at this:" },
        { "media_marker", "<__media__>" },
        { "text",         "describe it" },
    });
    auto media_only = make("user", { { "media_marker", "<__media__>" } });

    printf("[text-only parts, concat=false]\n%s\n",
           common_chat_msgs_to_json_oaicompat({ text_parts }, false).dump(2).c_str());
    printf("[text-only parts, concat=true]\n%s\n",
           common_chat_msgs_to_json_oaicompat({ text_parts }, true).dump(2).c_str());
    printf("[text+media, concat=false]\n%s\n",
           common_chat_msgs_to_json_oaicompat({ text_media }, false).dump(2).c_str());
    printf("[media only, concat=false]\n%s\n",
           common_chat_msgs_to_json_oaicompat({ media_only }, false).dump(2).c_str());

    common_chat_msg flat;
    flat.role    = "user";
    flat.content = "first line\nsecond line";
    printf("[flat string 'first line\\nsecond line', concat=false]\n%s\n",
           common_chat_msgs_to_json_oaicompat({ flat }, false).dump(2).c_str());

    // ---------------------------------------------------------------
    // 2. Request parser round trip
    // ---------------------------------------------------------------
    section("2. parse -> serialize round trip");
    const std::string req_text =
        R"([{"role":"user","content":[{"type":"text","text":"a"},{"type":"text","text":"b"}]}])";
    const std::string req_media =
        R"([{"role":"user","content":[{"type":"text","text":"a"},{"type":"media_marker","text":"<m>"}]}])";
    const std::string req_image =
        R"([{"role":"user","content":[{"type":"image_url","image_url":{"url":"data:x"}}]}])";

    for (const auto & r : { req_text, req_media }) {
        auto parsed = common_chat_msgs_parse_oaicompat(json::parse(r));
        printf("in : %s\n", r.c_str());
        printf("out: content='%s' parts=%zu -> %s\n\n", parsed[0].content.c_str(), parsed[0].content_parts.size(),
               common_chat_msgs_to_json_oaicompat(parsed, false).dump().c_str());
    }
    try {
        common_chat_msgs_parse_oaicompat(json::parse(req_image));
        printf("image_url part: ACCEPTED\n");
    } catch (const std::exception & e) {
        printf("image_url part: REJECTED -> %s\n", e.what());
    }

    // ---------------------------------------------------------------
    // 3. Caps oracle
    // ---------------------------------------------------------------
    section("3. caption flags from template source (no weights)");
    const char * files[] = { "Qwen3.5-4B.jinja", "Reka-Edge.jinja", "MiniMax-M3.jinja", "StepFun3.5-Flash.jinja" };
    for (const char * f : files) {
        auto src = read_file(tmpl_dir + f);
        try {
            common_chat_template t(src, "<s>", "</s>");
            const auto & c = t.original_caps();
            printf("%-24s string=%-5d typed=%-5d tools=%d\n", f, (int) c.supports_string_content,
                   (int) c.supports_typed_content, (int) c.supports_tools);
        } catch (const std::exception & e) {
            printf("%-24s EXCEPTION: %s\n", f, e.what());
        }
    }
    {
        common_chat_templates_ptr t = common_chat_templates_init(nullptr, "chatml");
        common_chat_template tm(common_chat_templates_source(t.get()), "<s>", "</s>");
        const auto & c = tm.original_caps();
        printf("%-24s string=%-5d typed=%-5d\n", "chatml (builtin)", (int) c.supports_string_content,
               (int) c.supports_typed_content);
    }

    // ---------------------------------------------------------------
    // 4. Render oracle: typed vs flat on a typed-capable template
    // ---------------------------------------------------------------
    section("4. template render, typed text parts vs flat string");
    {
        auto src   = read_file(tmpl_dir + "Qwen3.5-4B.jinja");
        auto tmpls = common_chat_templates_init(nullptr, src, "<s>", "</s>");

        common_chat_templates_inputs in_parts;
        in_parts.messages             = { text_parts };
        in_parts.add_generation_prompt = false;

        common_chat_templates_inputs in_flat;
        in_flat.messages             = { flat };
        in_flat.add_generation_prompt = false;

        printf("[Qwen3.5-4B typed parts]\n%s\n", common_chat_templates_apply(tmpls.get(), in_parts).prompt.str().c_str());
        printf("[Qwen3.5-4B flat string]\n%s\n", common_chat_templates_apply(tmpls.get(), in_flat).prompt.str().c_str());
    }
    {
        auto src   = read_file(tmpl_dir + "Qwen3.5-4B.jinja");
        auto tmpls = common_chat_templates_init(nullptr, src, "<s>", "</s>");

        common_chat_templates_inputs in;
        in.messages             = { text_media };
        in.add_generation_prompt = false;
        printf("[Qwen3.5-4B text+media parts]\n%s\n",
               common_chat_templates_apply(tmpls.get(), in).prompt.str().c_str());
    }

    // flat-only template: does it flatten or reject typed input?
    section("5. flat-only template (chatml) with typed input");
    {
        auto tmpls = common_chat_templates_init(nullptr, "chatml");
        common_chat_templates_inputs in;
        in.messages             = { text_parts };
        in.add_generation_prompt = false;
        try {
            printf("render: %s\n", common_chat_templates_apply(tmpls.get(), in).prompt.str().c_str());
        } catch (const std::exception & e) {
            printf("EXCEPTION: %s\n", e.what());
        }
    }

    return 0;
}
