// APX028 differential boundary fixture.
//
// Same inputs are fed to:
//   (a) the canonical plan + Goose control entry, and
//   (b) a direct transcription of the relevant chat.cpp control lines
//       (template selection, continuation, workarounds, handler detection,
//       reasoning dispatch, force-pure render).
// The fixture compares selected template, continuation/add-generation, the
// prepared messages JSON, detected handler id, format, thinking tags, error
// code, and the rendered ChatML prompt.  The parser-engine effects are the
// explicit external boundary; local-effects.cpp only satisfies their symbols
// for this isolated control check.

#include "chat-public.h"
#include "chat-schema.h"

#include "json.h"
#include "jinja/caps.h"
#include "jinja/lexer.h"
#include "jinja/parser.h"
#include "jinja/runtime.h"
#include "jinja/value.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_failures = 0;
static int g_checks = 0;

static void fail(const std::string & name, const std::string & what,
                 const std::string & got, const std::string & want) {
    g_failures++;
    std::printf("CASE %s FAIL %s got=[%s] want=[%s]\n", name.c_str(), what.c_str(),
                got.c_str(), want.c_str());
}

static void eq_int(const std::string & name, const char * what, int64_t got, int64_t want) {
    g_checks++;
    if (got != want) {
        fail(name, what, std::to_string(got), std::to_string(want));
    }
}

static void eq_str(const std::string & name, const char * what, const std::string & got,
                   const std::string & want) {
    g_checks++;
    if (got != want) {
        fail(name, what, got, want);
    }
}

static std::string trim_ws(const std::string & in) {
    size_t b = 0;
    size_t e = in.size();
    while (b < e && std::isspace((unsigned char) in[b])) { b++; }
    while (e > b && std::isspace((unsigned char) in[e - 1])) { e--; }
    return in.substr(b, e - b);
}

/* ------------------------------------------------- reference transcription */
static bool has(const std::string & src, const char * needle) {
    return src.find(needle) != std::string::npos;
}

static int64_t ref_detect(const std::string & src) {
    if (has(src, "[SYSTEM_PROMPT]") && has(src, "[TOOL_CALLS]") && has(src, "[ARGS]") &&
        !has(src, "[CALL_ID]")) { return 1; }
    if (has(src, "chat_format=llm-jp-harmony-v1")) { return 2; }
    if (has(src, "<|channel|>") && has(src, "<|constrain|>xml") && has(src, "rfl:parameter")) { return 3; }
    if (has(src, "<|channel|>")) { return 4; }
    if (has(src, "to=commentary") && has(src, "<|eom|>")) { return 5; }
    if (has(src, "<atem:function_calls>") && has(src, "<|eom|>")) { return 6; }
    if (has(src, ">>>all") && has(src, ">>>${recipient}")) { return 7; }
    if (has(src, "<|tool_calls_section_begin|>") && has(src, "<|tool_call_begin|>")) { return 8; }
    if (has(src, "<|open|>") && has(src, "<|close|>") && has(src, "<|end_of_msg|>")) { return 9; }
    if (has(src, "<|ifm|im_start|>") && has(src, "<ifm|tool_calls>")) { return 10; }
    if (has(src, "<role>ASSISTANT</role>") && has(src, "<arg_key>")) { return 11; }
    if (has(src, "<|START_TEXT|>") && has(src, "<|START_ACTION|>")) { return 12; }
    if (has(src, "<|tool_list_start|>") && has(src, "<|tool_list_end|>")) { return 13; }
    if (has(src, "List of tools: [") && !has(src, "<|tool_list_start|>")) { return 14; }
    if (has(src, "<|role_sep|>") && has(src, "<|message_sep|>") && !has(src, "<|function_call|>")) { return 15; }
    if (has(src, "]<]minimax[>[") && has(src, "<tool_call>") && has(src, "<invoke name=")) { return 16; }
    if (has(src, "dsml_token") && has(src, "DSML") &&
        (has(src, "function_calls") || has(src, "tool_calls"))) { return 17; }
    if (has(src, "'<|tool_call>call:'")) {
        if (!has(src, "{#- OpenAI Chat Completions:")) { return 18; }
        return 19;
    }
    if (has(src, "Tool usage guidelines:") && has(src, "<function name=\"") &&
        has(src, "<param name=\"")) { return 20; }
    if (has(src, "[source_lang_code]") && has(src, "[target_lang_code]")) { return 21; }
    if (has(src, "<tool_call>") && has(src, "<function=") && has(src, "<parameter=") &&
        !has(src, "'<tool_call><function=' ~ tool_call.name ~ '>'")) { return 22; }
    return 0;
}

static std::string concat_parts(const common_json & parts) {
    std::string text;
    bool last_media = false;
    for (size_t i = 0; i < parts.size(); i++) {
        const common_json & part = parts.at(i);
        std::string type = part.value("type", "");
        std::string s = part.value("text", "");
        if (type == "text") {
            if (!last_media && !text.empty()) { text += '\n'; }
            last_media = false;
            text += s;
        } else if (type == "media_marker") {
            last_media = true;
            text += s;
        }
    }
    return text;
}

static common_json normalize_messages(const common_json & in, int64_t caps) {
    bool only_string = ((caps & APX_C_STRING) != 0) && ((caps & APX_C_TYPED) == 0);
    bool only_typed  = ((caps & APX_C_STRING) == 0) && ((caps & APX_C_TYPED) != 0);
    if ((!only_string && !only_typed) || !in.is_array()) {
        return in;
    }
    common_json out = common_json::array();
    for (size_t i = 0; i < in.size(); i++) {
        common_json copy = in.at(i);
        if (copy.contains("content")) {
            common_json content = copy.at("content");
            if (only_typed && content.is_string()) {
                common_json arr = common_json::array();
                arr.push_back({{"type", "text"}, {"text", (std::string) content}});
                copy["content"] = arr;
            } else if (only_string && content.is_array()) {
                copy["content"] = concat_parts(content);
            }
        }
        out.push_back(copy);
    }
    return out;
}

static int map_developer(common_json & messages) {
    for (size_t i = 0; i < messages.size(); i++) {
        common_json & m = messages.at(i);
        if (m.contains("role") && m.at("role") == "developer") {
            m["role"] = std::string("system");
        }
    }
    return 0;
}

static int system_unsupported(common_json & messages) {
    if (messages.size() == 0 || !messages.front().contains("role") ||
        !(messages.front().at("role") == "system")) {
        return 0;
    }
    if (messages.size() > 1) {
        std::string first = messages.front().at("content");
        std::string second = messages.at(1).at("content");
        messages.at(1)["content"] = first + "\n" + second;
    }
    messages.erase((size_t) 0);
    return 0;
}

static int requires_content(common_json & messages) {
    for (size_t i = 0; i < messages.size(); i++) {
        common_json & m = messages.at(i);
        if (m.contains("tool_calls") && !m.contains("content")) {
            m["content"] = std::string();
        }
    }
    return 0;
}

static int func_args(common_json & messages) {
    for (size_t i = 0; i < messages.size(); i++) {
        common_json & m = messages.at(i);
        if (!m.contains("tool_calls")) { continue; }
        common_json & calls = m.at("tool_calls");
        for (size_t j = 0; j < calls.size(); j++) {
            common_json & tc = calls.at(j);
            if (!tc.contains("function")) { continue; }
            common_json & fn = tc.at("function");
            if (!fn.contains("arguments")) { continue; }
            common_json & args = fn.at("arguments");
            if (args.is_string()) {
                try {
                    args = common_json::parse((std::string) args);
                } catch (...) {
                    return APX_ERR_ARGS_JSON;
                }
            }
        }
    }
    return 0;
}

static void trim_all(common_json & messages) {
    for (size_t i = 0; i < messages.size(); i++) {
        common_json & m = messages.at(i);
        if (m.contains("content") && m.at("content").is_string()) {
            m["content"] = trim_ws((std::string) m.at("content"));
        }
        if (m.contains("reasoning_content") && m.at("reasoning_content").is_string()) {
            m["reasoning_content"] = trim_ws((std::string) m.at("reasoning_content"));
        }
        if (m.contains("content") && m.at("content").is_array()) {
            common_json & parts = m.at("content");
            for (size_t j = 0; j < parts.size(); j++) {
                common_json & part = parts.at(j);
                if (part.value("type", "") == "text" && part.contains("text")) {
                    part["text"] = trim_ws((std::string) part.at("text"));
                }
            }
        }
    }
}

struct RefRender {
    std::string text;
    std::string parts_json;
};

static RefRender ref_render(const std::string & src, const common_json & messages,
                            const std::string & bos, const std::string & eos, bool add_gen) {
    RefRender out;
    jinja::lexer lexer;
    auto lexer_res = lexer.tokenize(src);
    jinja::program prog = jinja::parse_from_tokens(lexer_res);

    common_json inp = common_json::object();
    inp["messages"] = messages;
    inp["bos_token"] = bos;
    inp["eos_token"] = eos;
    inp["enable_thinking"] = true;
    if (add_gen) {
        inp["add_generation_prompt"] = true;
    }
    jinja::context ctx(src);
    jinja::global_from_json(ctx, inp, true);
    ctx.set_val("bos_token", jinja::mk_val<jinja::value_string>(bos));
    ctx.set_val("eos_token", jinja::mk_val<jinja::value_string>(eos));
    jinja::runtime runtime(ctx);
    jinja::value results = runtime.execute(prog);
    jinja::value_string parts = jinja::runtime::gather_string_parts(results);

    common_json arr = common_json::array();
    for (const auto & p : parts->val_str.parts) {
        arr.push_back({{"text", p.val}, {"input", p.is_input}});
        out.text += p.val;
    }
    out.parts_json = arr.dump();
    return out;
}

struct RefCase {
    std::string name;
    std::string src;
    std::string tool_src;
    std::string bos = "<bos>";
    std::string eos = "<eos>";
    std::vector<std::string> messages;
    std::string tools_json;
    std::string grammar;
    int64_t caps = APX_C_TOOLS | APX_C_TOOL_CALLS | APX_C_SYSTEM | APX_C_PARALLEL | APX_C_STRING;
    int64_t cont = APX_CONT_NONE;
    bool add_gen = true;
    int64_t tool_choice = APX_TOOL_AUTO;
    bool force_pure = false;
    bool detect_only = false;
    int64_t analysis_ready = 1;
    int64_t reason_mode = APX_REASON_NONE;
    std::string reasoning_start;
    std::string reasoning_end;
    int64_t expected_handler = -1; // only for detect cases
};

struct RefResult {
    int64_t selected = 0;
    int64_t cont = 0;
    int64_t add_gen = 1;
    std::string messages_json;
    int64_t handler = 0;
    int64_t format = 0;
    int64_t supports_thinking = 0;
    std::string thinking_start;
    std::string thinking_end;
    int64_t error = 0;
    std::string prompt_text;
    std::string prompt_parts_json;
};

static RefResult ref_apply(const RefCase & c) {
    RefResult r;
    bool tools_present = !c.tools_json.empty() && c.tools_json != "[]";
    bool has_tool_use = !c.tool_src.empty();
    r.selected = (tools_present && has_tool_use) ? 1 : 0;
    const std::string & src = r.selected ? c.tool_src : c.src;

    common_json messages = common_json::array();
    for (const auto & m : c.messages) {
        messages.push_back(common_json::parse(m));
    }
    r.cont = c.cont;
    r.add_gen = c.add_gen ? 1 : 0;
    common_json cont_msg;
    bool has_cont_msg = false;
    if (r.cont != APX_CONT_NONE) {
        r.add_gen = 0;
        if (messages.size() > 0) {
            cont_msg = messages.back();
            messages.erase(messages.size() - 1);
            has_cont_msg = true;
            if (r.cont == APX_CONT_AUTO) {
                r.cont = APX_CONT_CONTENT;
                std::string reasoning = cont_msg.value("reasoning_content", "");
                std::string content = cont_msg.value("content", "");
                bool parts_empty = true;
                if (cont_msg.contains("content") && cont_msg.at("content").is_array()) {
                    parts_empty = cont_msg.at("content").size() == 0;
                }
                if (!reasoning.empty() && content.empty() && parts_empty) {
                    r.cont = APX_CONT_REASONING;
                }
            }
        }
    }
    (void) has_cont_msg;

    messages = normalize_messages(messages, c.caps);

    if (has(src, "You have access to the following functions in JSONSchema format")) {
        trim_all(messages);
    }
    if (!has(src, "<|channel|>")) {
        map_developer(messages);
    }
    if ((c.caps & APX_C_SYSTEM) == 0) {
        system_unsupported(messages);
    }
    if ((c.caps & APX_C_TOOL_CALLS) != 0) {
        requires_content(messages);
    }
    if ((c.caps & APX_C_OBJECT_ARGS) != 0) {
        int e = func_args(messages);
        if (e < 0) { r.error = e; r.messages_json = messages.dump(); return r; }
    }
    r.messages_json = messages.dump();

    if (tools_present && c.tool_choice != APX_TOOL_NONE && !c.grammar.empty()) {
        r.error = APX_ERR_GRAMMAR_TOOLS;
        return r;
    }

    if (c.force_pure) {
        r.format = APX_FMT_PEG_NATIVE;
        RefRender rr = ref_render(src, messages, c.bos, c.eos, c.add_gen);
        r.prompt_text = rr.text;
        r.prompt_parts_json = rr.parts_json;
        return r;
    }

    r.handler = ref_detect(src);
    if (r.handler != 0) {
        return r;
    }
    if (!c.analysis_ready) {
        r.error = APX_ERR_NO_ANALYSIS;
        return r;
    }
    r.format = APX_FMT_PEG_NATIVE;
    if (c.reason_mode == APX_REASON_TAG || c.reason_mode == APX_REASON_TOOLS_ONLY) {
        r.supports_thinking = 1;
        r.thinking_start = trim_ws(c.reasoning_start);
        std::string end = trim_ws(c.reasoning_end);
        if (!end.empty()) { r.thinking_end = end; }
    }
    return r;
}

/* ----------------------------------------------------------- plan runner */
struct PlanResult {
    int64_t rc = 0;
    int64_t selected = 0;
    int64_t cont = 0;
    int64_t add_gen = 0;
    std::string messages_json;
    int64_t handler = 0;
    int64_t format = 0;
    int64_t supports_thinking = 0;
    std::string thinking_start;
    std::string thinking_end;
    int64_t error = 0;
    std::string prompt_text;
    std::string prompt_parts_json;
};

static PlanResult run_plan(const RefCase & c) {
    PlanResult p;
    int64_t plan = apx028_plan_new();
    if (!plan) {
        p.rc = APX_ERR_STATE;
        return p;
    }
    if (apx028_plan_set_template(plan, 0, c.src.c_str(), c.bos.c_str(), c.eos.c_str()) < 0) {
        apx028_plan_free(plan);
        p.rc = APX_ERR_INVAL;
        return p;
    }
    if (!c.tool_src.empty()) {
        if (apx028_plan_set_template(plan, 1, c.tool_src.c_str(), c.bos.c_str(), c.eos.c_str()) < 0) {
            apx028_plan_free(plan);
            p.rc = APX_ERR_INVAL;
            return p;
        }
    }
    for (const auto & m : c.messages) {
        if (apx028_plan_add_message_json(plan, m.c_str()) < 0) {
            apx028_plan_free(plan);
            p.rc = APX_ERR_INVAL;
            return p;
        }
    }
    if (!c.tools_json.empty()) {
        if (apx028_plan_set_tools_json(plan, c.tools_json.c_str()) < 0) {
            apx028_plan_free(plan);
            p.rc = APX_ERR_INVAL;
            return p;
        }
    }
    if (!c.grammar.empty()) {
        apx028_plan_set_grammar(plan, c.grammar.c_str());
    }
    apx028_plan_set_caps_override(plan, 0, c.caps);
    apx028_plan_set_int(plan, PK_CONT, c.cont);
    apx028_plan_set_int(plan, PK_ADD_GEN, c.add_gen ? 1 : 0);
    apx028_plan_set_int(plan, PK_TOOL_CHOICE, c.tool_choice);
    apx028_plan_set_int(plan, PK_FORCE_PURE, c.force_pure ? 1 : 0);
    apx028_plan_set_int(plan, PK_ENABLE_THINKING, 1);
    apx028_plan_set_int(plan, PK_MARK_INPUT, 1);
    apx028_plan_set_int(plan, PK_ADD_BOS, 0);
    apx028_plan_set_int(plan, PK_ADD_EOS, 0);
    apx028_plan_set_int(plan, PK_REASONING_FORMAT, APX_REASON_FMT_NONE);
    apx028_plan_set_analysis(plan, c.analysis_ready, c.reason_mode,
                             c.reasoning_start.c_str(), c.reasoning_end.c_str());

    p.rc = c.detect_only ? apx028_plan_detect(plan) : apx028_plan_apply(plan);
    if (!c.detect_only) {
        (void) apx028_plan_store_messages_json(plan);
    }
    p.selected    = apx028_plan_result_int(plan, RK_SELECTED_WHICH);
    p.cont        = apx028_plan_result_int(plan, RK_CONT);
    p.add_gen     = apx028_plan_result_int(plan, RK_ADD_GEN);
    p.handler     = apx028_plan_result_int(plan, RK_HANDLER);
    p.format      = apx028_plan_result_int(plan, RK_FORMAT);
    p.supports_thinking = apx028_plan_result_int(plan, RK_SUPPORTS_THINKING);
    p.error       = apx028_plan_result_int(plan, RK_ERROR);
    p.thinking_start = apx028_plan_result_str(plan, RK_THINKING_START);
    p.thinking_end   = apx028_plan_result_str(plan, RK_THINKING_END);
    if (!c.detect_only) {
        p.messages_json = apx028_plan_messages_json(plan);
    }
    p.prompt_text = apx028_plan_prompt_text(plan);
    p.prompt_parts_json = apx028_plan_prompt_json(plan);
    apx028_plan_free(plan);
    return p;
}

static void run_case(const RefCase & c) {
    PlanResult got = run_plan(c);
    if (c.detect_only) {
        RefResult want;
        want.handler = ref_detect(c.src);
        eq_int(c.name, "detect_rc", got.rc, 0);
        eq_int(c.name, "handler", got.handler, want.handler);
        if (c.expected_handler >= 0) {
            eq_int(c.name, "handler_expected", got.handler, c.expected_handler);
            eq_int(c.name, "reference_expected", want.handler, c.expected_handler);
        }
        return;
    }

    RefResult want = ref_apply(c);

    if (want.error != 0) {
        eq_int(c.name, "apply_rc", got.rc, want.error);
        eq_int(c.name, "error", got.error, want.error);
    } else {
        eq_int(c.name, "apply_rc", got.rc, 0);
        eq_int(c.name, "error", got.error, 0);
    }
    eq_int(c.name, "selected", got.selected, want.selected);
    eq_int(c.name, "continuation", got.cont, want.cont);
    eq_int(c.name, "add_generation_prompt", got.add_gen, want.add_gen);
    eq_str(c.name, "prepared_messages", got.messages_json, want.messages_json);
    eq_int(c.name, "handler", got.handler, want.handler);
    eq_int(c.name, "format", got.format, want.format);
    eq_int(c.name, "supports_thinking", got.supports_thinking, want.supports_thinking);
    eq_str(c.name, "thinking_start", got.thinking_start, want.thinking_start);
    eq_str(c.name, "thinking_end", got.thinking_end, want.thinking_end);
    if (c.force_pure) {
        eq_str(c.name, "prompt_text", got.prompt_text, want.prompt_text);
        eq_str(c.name, "prompt_parts", got.prompt_parts_json, want.prompt_parts_json);
    }
}

static const char * CHATML =
    "{%- for message in messages -%}\n"
    "  {{- '<|im_start|>' + message.role + '\\n' + message.content + '<|im_end|>\\n' -}}\n"
    "{%- endfor -%}\n"
    "{%- if add_generation_prompt -%}\n"
    "  {{- '<|im_start|>assistant\\n' -}}\n"
    "{%- endif -%}";

static std::string chatml_with(const std::string & extra) {
    return std::string(CHATML) + extra;
}

int main() {
    {
        RefCase c;
        c.name = "select_default";
        c.src = CHATML;
        c.messages = {"{\"role\":\"user\",\"content\":\"hi\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "select_tool_use";
        c.src = CHATML;
        c.tool_src = chatml_with("[TOOL_USE]");
        c.tools_json = "[{\"type\":\"function\",\"function\":{\"name\":\"f\",\"description\":\"d\",\"parameters\":{}}}]";
        c.messages = {"{\"role\":\"user\",\"content\":\"hi\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "select_tool_use_without_tools";
        c.src = CHATML;
        c.tool_src = chatml_with("[TOOL_USE]");
        c.messages = {"{\"role\":\"user\",\"content\":\"hi\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "continuation_content";
        c.src = CHATML;
        c.messages = {"{\"role\":\"user\",\"content\":\"hi\"}",
                      "{\"role\":\"assistant\",\"content\":\"part\"}"};
        c.cont = APX_CONT_CONTENT;
        run_case(c);
    }
    {
        RefCase c;
        c.name = "continuation_auto_reasoning";
        c.src = CHATML;
        c.messages = {"{\"role\":\"user\",\"content\":\"hi\"}",
                      "{\"role\":\"assistant\",\"content\":\"\",\"reasoning_content\":\"think\"}"};
        c.cont = APX_CONT_AUTO;
        run_case(c);
    }
    {
        RefCase c;
        c.name = "continuation_auto_content";
        c.src = CHATML;
        c.messages = {"{\"role\":\"user\",\"content\":\"hi\"}",
                      "{\"role\":\"assistant\",\"content\":\"answer\"}"};
        c.cont = APX_CONT_AUTO;
        run_case(c);
    }
    {
        RefCase c;
        c.name = "developer_to_system";
        c.src = CHATML;
        c.force_pure = true;
        c.messages = {"{\"role\":\"developer\",\"content\":\"rule\"}",
                      "{\"role\":\"user\",\"content\":\"hi\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "developer_kept_channel";
        c.src = chatml_with("<|channel|>");
        c.force_pure = true;
        c.messages = {"{\"role\":\"developer\",\"content\":\"rule\"}",
                      "{\"role\":\"user\",\"content\":\"hi\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "system_merge";
        c.src = CHATML;
        c.force_pure = true;
        c.caps = APX_C_TOOLS | APX_C_TOOL_CALLS | APX_C_PARALLEL | APX_C_STRING;
        c.messages = {"{\"role\":\"system\",\"content\":\"sys\"}",
                      "{\"role\":\"user\",\"content\":\"hi\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "system_drop";
        c.src = CHATML;
        c.force_pure = true;
        c.caps = APX_C_TOOLS | APX_C_TOOL_CALLS | APX_C_PARALLEL | APX_C_STRING;
        c.messages = {"{\"role\":\"system\",\"content\":\"sys\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "object_arguments";
        c.src = CHATML;
        c.force_pure = true;
        c.caps = APX_C_TOOLS | APX_C_TOOL_CALLS | APX_C_SYSTEM | APX_C_PARALLEL | APX_C_STRING | APX_C_OBJECT_ARGS;
        c.messages = {"{\"role\":\"assistant\",\"content\":\"\",\"tool_calls\":[{\"type\":\"function\","
                      "\"function\":{\"name\":\"f\",\"arguments\":\"{\\\"x\\\":1}\"}}]}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "stepfun_trim";
        c.src = chatml_with("You have access to the following functions in JSONSchema format");
        c.force_pure = true;
        c.messages = {"{\"role\":\"user\",\"content\":\"  hi  \",\"reasoning_content\":\"  r  \"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "typed_content_normalization";
        c.src = CHATML;
        c.force_pure = true;
        c.caps = APX_C_TOOLS | APX_C_TOOL_CALLS | APX_C_SYSTEM | APX_C_PARALLEL | APX_C_TYPED;
        c.messages = {"{\"role\":\"user\",\"content\":\"hi\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "tools_grammar_conflict";
        c.src = CHATML;
        c.tools_json = "[{\"type\":\"function\",\"function\":{\"name\":\"f\",\"description\":\"d\",\"parameters\":{}}}]";
        c.grammar = "root ::= \"x\"";
        c.messages = {"{\"role\":\"user\",\"content\":\"hi\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "tools_grammar_allowed_none";
        c.src = CHATML;
        c.tools_json = "[{\"type\":\"function\",\"function\":{\"name\":\"f\",\"description\":\"d\",\"parameters\":{}}}]";
        c.grammar = "root ::= \"x\"";
        c.tool_choice = APX_TOOL_NONE;
        c.messages = {"{\"role\":\"user\",\"content\":\"hi\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "no_analysis_error";
        c.src = CHATML;
        c.analysis_ready = 0;
        c.messages = {"{\"role\":\"user\",\"content\":\"hi\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "force_pure_render";
        c.src = CHATML;
        c.force_pure = true;
        c.messages = {"{\"role\":\"system\",\"content\":\"sys\"}",
                      "{\"role\":\"user\",\"content\":\"hello\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "reasoning_tag_dispatch";
        c.src = CHATML;
        c.reason_mode = APX_REASON_TAG;
        c.reasoning_start = "  \n";
        c.reasoning_end = " \t ";
        c.messages = {"{\"role\":\"user\",\"content\":\"hi\"}"};
        run_case(c);
    }
    {
        RefCase c;
        c.name = "reasoning_none";
        c.src = CHATML;
        c.reason_mode = APX_REASON_NONE;
        c.messages = {"{\"role\":\"user\",\"content\":\"hi\"}"};
        run_case(c);
    }

    struct DetectCase { const char * name; const char * src; int64_t handler; };
    static const DetectCase detections[] = {
        {"detect_mistral3", "[SYSTEM_PROMPT][TOOL_CALLS][ARGS]", 1},
        {"detect_llm_jp", "chat_format=llm-jp-harmony-v1", 2},
        {"detect_harmony_xml", "<|channel|><|constrain|>xml rfl:parameter", 3},
        {"detect_gpt_oss", "<|channel|>", 4},
        {"detect_muse_spark", "to=commentary<|eom|>", 5},
        {"detect_muse_glimmer", "<atem:function_calls><|eom|>", 6},
        {"detect_functionary", ">>>all >>>${recipient}", 7},
        {"detect_kimi_k2", "<|tool_calls_section_begin|><|tool_call_begin|>", 8},
        {"detect_kimi_k3", "<|open|><|close|><|end_of_msg|>", 9},
        {"detect_k2_horizon", "<|ifm|im_start|><ifm|tool_calls>", 10},
        {"detect_ling3", "<role>ASSISTANT</role><arg_key>", 11},
        {"detect_cohere2moe", "<|START_TEXT|><|START_ACTION|>", 12},
        {"detect_lfm2", "<|tool_list_start|>x<|tool_list_end|>", 13},
        {"detect_lfm2_5", "List of tools: [x]", 14},
        {"detect_gigachat", "<|role_sep|><|message_sep|>", 15},
        {"detect_minimax_m3", "]<]minimax[>[<tool_call><invoke name=", 16},
        {"detect_deepseek", "dsml_token DSML function_calls", 17},
        {"detect_gemma4_legacy", "'<|tool_call>call:'", 18},
        {"detect_gemma4", "'<|tool_call>call:' {#- OpenAI Chat Completions:#}", 19},
        {"detect_minicpm5", "Tool usage guidelines: <function name=\" <param name=\"", 20},
        {"detect_translate_gemma", "[source_lang_code][target_lang_code]", 21},
        {"detect_qwen3_coder", "<tool_call><function=<parameter=", 22},
        {"detect_none", "plain chat template", 0},
    };
    for (const auto & d : detections) {
        RefCase c;
        c.name = d.name;
        c.src = d.src;
        c.detect_only = true;
        c.expected_handler = d.handler;
        run_case(c);
    }

    if (g_failures != 0) {
        std::printf("RESULT FAIL failures=%d checks=%d\n", g_failures, g_checks);
        return 1;
    }
    std::printf("RESULT PASS checks=%d cases=%d\n", g_checks, 42);
    return 0;
}
