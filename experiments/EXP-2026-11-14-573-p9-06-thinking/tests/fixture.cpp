// Differential gate for P9-06 S3: thinking control.
//
// Native oracle: bmoe::detail::probe_think_control and
// bmoe::detail::add_no_think_prefill from
// runtime/core/src/engine/thinking_control.cpp, compiled into this fixture.
// Port: think.goose (tc_probe, tc_add_prefill, tc_prefill_reasoning,
// tc_span_closed, tc_name).
//
// The native probe renders three prompts through common_chat_templates_apply.
// That renderer is NOT ported; the fixture uses the native one to produce the
// inputs for the pure Goose decision. This is the recorded dependency.
//
// 12 cases: six vendored templates, a plain template, null, prefill
// construction, prefill bytes, the LFM2.5 span-closed property, and the names.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "runtime-gen.h"
#include "chat.h"
#include "thinking_control.h"

static int g_cases = 0, g_values = 0, g_fails = 0;

static void bad(const char * what, const std::string & detail) {
    ++g_fails;
    if (g_fails <= 100) {
        printf("FAIL %s: %s\n", what, detail.c_str());
    }
}

static sl_u8 sl(std::string & s) {
    return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() };
}
static sl_i64 sli(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

static std::string read_file(const std::string & path) {
    FILE * f = fopen(path.c_str(), "rb");
    if (!f) {
        throw std::runtime_error("cannot open " + path);
    }
    std::string out;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        out.append(buf, n);
    }
    fclose(f);
    return out;
}

struct rendered {
    std::string off;
    std::string on;
    std::string prefilled;
    std::string source;
    std::string start_tag;
    std::vector<std::string> end_tags;
};

static common_chat_templates_ptr load_template(const std::string & src) {
    return common_chat_templates_init(nullptr, src);
}

static std::string render(const common_chat_templates * t, bool think, bool prefill) {
    common_chat_msg user;
    user.role    = "user";
    user.content = "hi";

    common_chat_templates_inputs inputs;
    inputs.messages             = { user };
    inputs.add_generation_prompt = true;
    inputs.use_jinja            = true;
    inputs.enable_thinking      = think;
    inputs.reasoning_format     = COMMON_REASONING_FORMAT_AUTO;
    if (prefill) {
        bmoe::detail::add_no_think_prefill(inputs);
    }
    return common_chat_templates_apply(const_cast<common_chat_templates *>(t), inputs).prompt.str();
}

static rendered render_all(const common_chat_templates * t) {
    rendered r;
    r.off       = render(t, false, false);
    r.on        = render(t, true,  false);
    r.prefilled = render(t, false, true);
    r.source    = common_chat_templates_source(t);

    // Tags come from the thinking-off render, exactly as the native probe reads
    // them.
    auto p = common_chat_templates_apply(const_cast<common_chat_templates *>(t), [&] {
        common_chat_msg user;
        user.role    = "user";
        user.content = "hi";
        common_chat_templates_inputs in;
        in.messages              = { user };
        in.add_generation_prompt = true;
        in.use_jinja             = true;
        in.enable_thinking       = false;
        in.reasoning_format      = COMMON_REASONING_FORMAT_AUTO;
        return in;
    }());
    r.start_tag = p.thinking_start_tag;
    r.end_tags  = p.thinking_end_tags;
    return r;
}

static void run_probe_case(const char * name, const std::string & src) {
    ++g_cases;
    try {
        auto t = load_template(src);
        if (!t) {
            bad(name, "template init failed");
            return;
        }
        const bmoe::ThinkControl native = bmoe::detail::probe_think_control(t.get());
        const rendered r = render_all(t.get());

        std::string ends_blob;
        std::vector<int64_t> eoff, elen;
        for (const auto & e : r.end_tags) {
            eoff.push_back((int64_t) ends_blob.size());
            elen.push_back((int64_t) e.size());
            ends_blob += e;
        }
        std::string nm;
        std::string out_name(16, '\0');
        std::string off = r.off, on = r.on, pre = r.prefilled, source = r.source, start = r.start_tag;
        std::string ends = ends_blob;
        sl_u8 out_sl{ (uint8_t *) out_name.data(), (int64_t) out_name.size() };

        const int64_t port = tc_probe(0, sl(off), sl(on), sl(pre), sl(source), sl(start),
                                      sl(ends), sli(eoff), sli(elen), (int64_t) eoff.size(), 0, out_sl);
        ++g_values;
        if (port != (int64_t) native) {
            bad(name, "native=" + std::to_string((int) native) + " port=" + std::to_string((int) port));
        }
        // The name written by the port must match think_control_name.
        const std::string want = bmoe::think_control_name(native);
        out_name.resize((size_t) tc_name((int64_t) native, out_sl));
        ++g_values;
        if (out_name != want) {
            bad(name, "name native=" + want + " port=" + out_name);
        }
    } catch (const std::exception & e) {
        bad(name, std::string("threw: ") + e.what());
    }
}

static void run_null_case() {
    ++g_cases;
    const bmoe::ThinkControl native = bmoe::detail::probe_think_control(nullptr);
    std::string empty;
    std::vector<int64_t> none;
    std::string out_name(16, '\0');
    sl_u8 out_sl{ (uint8_t *) out_name.data(), (int64_t) out_name.size() };
    const int64_t port = tc_probe(1, sl(empty), sl(empty), sl(empty), sl(empty), sl(empty),
                                  sl(empty), sli(none), sli(none), 0, 0, out_sl);
    ++g_values;
    if (port != (int64_t) native) {
        bad("null", "native=" + std::to_string((int) native) + " port=" + std::to_string((int) port));
    }
}

static void run_prefill_construction() {
    ++g_cases;
    // Native shape
    common_chat_msg user;
    user.role    = "user";
    user.content = "hi";
    common_chat_templates_inputs inputs;
    inputs.messages = { user };
    bmoe::detail::add_no_think_prefill(inputs);

    // Port shape
    std::vector<int64_t> roles(4, 0), has(4, 0), clen(4, 0), flags(4, -1);
    roles[0] = 0;
    has[0]   = 0;
    clen[0]  = 2;
    const int64_t n = tc_add_prefill(sli(roles), sli(has), sli(clen), 1, sli(flags));

    ++g_values;
    if ((int64_t) inputs.messages.size() != n) {
        bad("prefill", "message count native=" + std::to_string(inputs.messages.size()) + " port=" + std::to_string(n));
    }
    ++g_values;
    if (n != 2 || roles[1] != 1) {
        bad("prefill", "appended role is not assistant");
    }
    ++g_values;
    if (inputs.messages.size() < 2 || inputs.messages[1].content != "" || clen[1] != 0) {
        bad("prefill", "content is not empty");
    }
    ++g_values;
    if (inputs.messages.size() < 2 || inputs.messages[1].reasoning_content != "\n\n" || has[1] != 1) {
        bad("prefill", "reasoning is not the empty whitespace span");
    }
    ++g_values;
    if (inputs.continue_final_message != COMMON_CHAT_CONTINUATION_CONTENT || flags[0] != 3) {
        bad("prefill", "continuation is not CONTENT");
    }
}

static void run_prefill_bytes() {
    ++g_cases;
    uint8_t buf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    sl_u8 out{ buf, (int64_t) sizeof buf };
    const int64_t n = tc_prefill_reasoning(out);
    ++g_values;
    if (n != 2) {
        bad("prefill-bytes", "length=" + std::to_string((int) n));
    }
    ++g_values;
    if (buf[0] != '\n' || buf[1] != '\n') {
        bad("prefill-bytes", "not \\n\\n");
    }
}

static void run_span_closed(const common_chat_templates * t) {
    ++g_cases;
    try {
        const std::string pre = render(t, false, true);
        const std::string close_marker = "</think>";
        const size_t at = pre.rfind(close_marker);
        const bool native_ok = at != std::string::npos &&
            pre.substr(at + close_marker.size()).find("<think>") == std::string::npos;
        std::string pre_mut = pre;
        std::string marker  = close_marker;
        const int64_t port = tc_span_closed(sl(pre_mut), sl(marker));

        ++g_values;
        if (!native_ok || port != 1) {
            bad("span-closed", std::string("native=") + (native_ok ? "1" : "0") +
                               " port=" + std::to_string((int) port));
        }
    } catch (const std::exception & e) {
        bad("span-closed", std::string("threw: ") + e.what());
    }
}

static void run_names() {
    ++g_cases;
    const char * want[3] = { "template", "prefill", "none" };
    for (int c = 0; c < 3; ++c) {
        std::string buf(16, '\0');
        sl_u8 out{ (uint8_t *) buf.data(), (int64_t) buf.size() };
        const int64_t n = tc_name(c, out);
        buf.resize((size_t) n);
        ++g_values;
        if (buf != want[c]) {
            bad("names", "code " + std::to_string(c) + " got " + buf);
        }
    }
}

int main(int argc, char ** argv) {
    goose_init(0, nullptr);
    const std::string root = argc > 1 ? argv[1] : "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda";
    struct tmpl_case { const char * name; const char * file; };
    const tmpl_case tmpls[] = {
        { "qwen3",            "Qwen3.5-4B.jinja" },
        { "lfm2.5",           "LFM2.5-8B-A1B.jinja" },
        { "gpt-oss",          "openai-gpt-oss-120b.jinja" },
        { "lfm2",             "LFM2-8B-A1B.jinja" },
        { "lfm2.5-instruct",  "LFM2.5-Instruct.jinja" },
        { "gemma4",           "google-gemma-4-31B-it.jinja" },
    };
    for (const auto & c : tmpls) {
        run_probe_case(c.name, read_file(root + "/models/templates/" + c.file));
    }

    run_probe_case("plain", "{% for m in messages %}{{ m.role }}: {{ m.content }}\n{% endfor %}assistant:");
    run_null_case();
    run_prefill_construction();
    run_prefill_bytes();

    auto lfm25 = load_template(read_file(root + "/models/templates/LFM2.5-8B-A1B.jinja"));
    run_span_closed(lfm25.get());

    run_names();

    printf("FG577_THINK cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
