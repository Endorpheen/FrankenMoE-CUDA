// APX028 chat/Jinja control service projection.
//
// This translation unit is the named-effect side of the split:
//   * worker-8 canonical storage owns records, message vectors and strings;
//   * minja owns template lex/parse/caps/render (explicit vendor effect);
//   * common_json owns JSON parse/dump (explicit vendor effect);
//   * Goose owns every branch and loop in chat-control.goose.
//
// No function here calls common_chat_templates_apply or any whole native chat
// delegate.  The PEG autoparser and dedicated parser bodies are separate weak
// imports, never a fallback that returns success.

#include "chat-public.h"
#include "chat-schema.h"
#include "canonical-storage-bindings.h"

#include "json.h"
#include "jinja/caps.h"
#include "jinja/lexer.h"
#include "jinja/parser.h"
#include "jinja/runtime.h"
#include "jinja/value.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <new>
#include <sstream>
#include <string>
#include <vector>

/* ------------------------------------------------------------------ vendor */
struct TmplVendor {
    jinja::program prog;
};

struct PlanVendor {
    int64_t native_tmpl   = 0;
    int64_t native_inputs = 0;
    int64_t native_result = 0;
};

/* --------------------------------------------------------------- helpers */
static std::string s_get(int64_t h) {
    if (!h) {
        return std::string();
    }
    const char * p = fgc_str_data(h);
    int64_t      n = fgc_str_len(h);
    if (!p || n <= 0) {
        return std::string();
    }
    return std::string(p, (size_t) n);
}

static int64_t s_new(const std::string & s) {
    int64_t h = fgc_bytes_new();
    if (!h) {
        return 0;
    }
    if (fgc_str_set(h, s.data(), (int64_t) s.size()) != 0) {
        fgc_str_free(h);
        return 0;
    }
    return h;
}

static int64_t s_new_cstr(const char * p) {
    return s_new(std::string(p ? p : ""));
}

static int64_t rec_getv(int64_t h, int64_t k) {
    return h ? fgc_record_get(h, k) : 0;
}

static void rec_setv(int64_t h, int64_t k, int64_t v) {
    if (h) {
        (void) fgc_record_set(h, k, v);
    }
}

static void str_replace_key(int64_t rec, int64_t key, const std::string & s) {
    int64_t old = rec_getv(rec, key);
    if (old) {
        fgc_str_free(old);
    }
    int64_t h = s_new(s);
    rec_setv(rec, key, h);
}

static int64_t vec_at(int64_t vec, int64_t index) {
    int64_t n = fgc_vec_len(vec);
    if (index < 0 || index >= n) {
        return 0;
    }
    int64_t * data = (int64_t *) fgc_vec_data(vec);
    return data ? data[index] : 0;
}

static void vec_push(int64_t vec, int64_t value) {
    int64_t n = fgc_vec_len(vec);
    if (fgc_vec_resize(vec, n + 1) != 0) {
        return;
    }
    int64_t * data = (int64_t *) fgc_vec_data(vec);
    if (data) {
        data[n] = value;
    }
}

static std::string trim_ws(const std::string & in) {
    size_t b = 0;
    size_t e = in.size();
    while (b < e && std::isspace((unsigned char) in[b])) {
        b++;
    }
    while (e > b && std::isspace((unsigned char) in[e - 1])) {
        e--;
    }
    return in.substr(b, e - b);
}

static int64_t role_from_name(const std::string & role) {
    if (role == "system")    { return APX_ROLE_SYSTEM; }
    if (role == "assistant") { return APX_ROLE_ASSISTANT; }
    if (role == "user")      { return APX_ROLE_USER; }
    if (role == "tool")      { return APX_ROLE_TOOL; }
    if (role == "developer") { return APX_ROLE_DEVELOPER; }
    return APX_ROLE_UNKNOWN;
}

static const char * role_name(int64_t role) {
    switch (role) {
        case APX_ROLE_SYSTEM:    return "system";
        case APX_ROLE_ASSISTANT: return "assistant";
        case APX_ROLE_USER:      return "user";
        case APX_ROLE_TOOL:      return "tool";
        case APX_ROLE_DEVELOPER: return "developer";
        default:                 return "";
    }
}

static const char * needle_text(int64_t id) {
    switch (id) {
        case APX_N_MINISTRAL_SYSTEM:  return "[SYSTEM_PROMPT]";
        case APX_N_MINISTRAL_CALLS:   return "[TOOL_CALLS]";
        case APX_N_MINISTRAL_ARGS:    return "[ARGS]";
        case APX_N_MINISTRAL_CALL_ID: return "[CALL_ID]";
        case APX_N_LLM_JP:            return "chat_format=llm-jp-harmony-v1";
        case APX_N_CHANNEL:           return "<|channel|>";
        case APX_N_CONSTRAIN_XML:     return "<|constrain|>xml";
        case APX_N_RFL_PARAMETER:     return "rfl:parameter";
        case APX_N_MUSE_COMMENTARY:   return "to=commentary";
        case APX_N_EOM:               return "<|eom|>";
        case APX_N_ATEM:              return "<atem:function_calls>";
        case APX_N_FUNCTIONARY_ALL:   return ">>>all";
        case APX_N_FUNCTIONARY_RECIP: return ">>>${recipient}";
        case APX_N_KIMI_SECTION:      return "<|tool_calls_section_begin|>";
        case APX_N_KIMI_BEGIN:        return "<|tool_call_begin|>";
        case APX_N_KIMI_OPEN:         return "<|open|>";
        case APX_N_KIMI_CLOSE:        return "<|close|>";
        case APX_N_KIMI_EOMSG:        return "<|end_of_msg|>";
        case APX_N_IFM_START:         return "<|ifm|im_start|>";
        case APX_N_IFM_TOOLS:         return "<ifm|tool_calls>";
        case APX_N_LING_ROLE:         return "<role>ASSISTANT</role>";
        case APX_N_ARG_KEY:           return "<arg_key>";
        case APX_N_COHERE_TEXT:       return "<|START_TEXT|>";
        case APX_N_COHERE_ACTION:     return "<|START_ACTION|>";
        case APX_N_LFM2_LIST_START:   return "<|tool_list_start|>";
        case APX_N_LFM2_LIST_END:     return "<|tool_list_end|>";
        case APX_N_LFM25_TOOLS:       return "List of tools: [";
        case APX_N_GIGA_ROLE:         return "<|role_sep|>";
        case APX_N_GIGA_MSG:          return "<|message_sep|>";
        case APX_N_GIGA_FN:           return "<|function_call|>";
        case APX_N_MINIMAX_NS:        return "]<]minimax[>[";
        case APX_N_TOOL_CALL_TAG:     return "<tool_call>";
        case APX_N_INVOKE_NAME:       return "<invoke name=";
        case APX_N_DSML_TOKEN:        return "dsml_token";
        case APX_N_DSML_CAPS:         return "DSML";
        case APX_N_FUNCTION_CALLS:    return "function_calls";
        case APX_N_TOOL_CALLS_WORD:   return "tool_calls";
        case APX_N_GEMMA4_CALL:       return "'<|tool_call>call:'";
        case APX_N_GEMMA4_OPENAI:     return "{#- OpenAI Chat Completions:";
        case APX_N_MINICPM_GUIDE:     return "Tool usage guidelines:";
        case APX_N_MINICPM_FUNCTION:  return "<function name=\"";
        case APX_N_MINICPM_PARAM:     return "<param name=\"";
        case APX_N_TRANSLATE_SRC:     return "[source_lang_code]";
        case APX_N_TRANSLATE_TGT:     return "[target_lang_code]";
        case APX_N_FUNCTION_EQ:       return "<function=";
        case APX_N_PARAMETER_EQ:      return "<parameter=";
        case APX_N_QWEN3_EXCLUDE:     return "'<tool_call><function=' ~ tool_call.name ~ '>'";
        case APX_N_STEPFUN:           return "You have access to the following functions in JSONSchema format";
        default:                      return nullptr;
    }
}

/* ---------------------------------------------------------- free helpers */
static void part_free(int64_t p) {
    if (!p) {
        return;
    }
    int64_t type = rec_getv(p, PT_TYPE);
    int64_t text = rec_getv(p, PT_TEXT);
    if (type) { fgc_str_free(type); }
    if (text) { fgc_str_free(text); }
    fgc_record_free(p);
}

static void toolcall_free(int64_t t) {
    if (!t) {
        return;
    }
    int64_t name = rec_getv(t, TC_NAME);
    int64_t args = rec_getv(t, TC_ARGS);
    int64_t id   = rec_getv(t, TC_ID);
    if (name) { fgc_str_free(name); }
    if (args) { fgc_str_free(args); }
    if (id)   { fgc_str_free(id); }
    fgc_record_free(t);
}

static void tool_free(int64_t t) {
    if (!t) {
        return;
    }
    int64_t name = rec_getv(t, TL_NAME);
    int64_t desc = rec_getv(t, TL_DESC);
    int64_t par  = rec_getv(t, TL_PARAM);
    if (name) { fgc_str_free(name); }
    if (desc) { fgc_str_free(desc); }
    if (par)  { fgc_str_free(par); }
    fgc_record_free(t);
}

static void vec_free_handles(int64_t vec, void (*fn)(int64_t)) {
    if (!vec) {
        return;
    }
    int64_t n = fgc_vec_len(vec);
    for (int64_t i = 0; i < n; i++) {
        int64_t h = vec_at(vec, i);
        if (h) {
            fn(h);
        }
    }
    fgc_vec_free(vec);
}

static void msg_free(int64_t m) {
    if (!m) {
        return;
    }
    int64_t content = rec_getv(m, MK_CONTENT);
    int64_t reasoning = rec_getv(m, MK_REASONING);
    int64_t name = rec_getv(m, MK_NAME);
    int64_t tcid = rec_getv(m, MK_TCID);
    if (content)   { fgc_str_free(content); }
    if (reasoning) { fgc_str_free(reasoning); }
    if (name)      { fgc_str_free(name); }
    if (tcid)      { fgc_str_free(tcid); }
    int64_t parts = rec_getv(m, MK_PARTS);
    if (parts) {
        vec_free_handles(parts, part_free);
    }
    int64_t calls = rec_getv(m, MK_TOOLCALLS);
    if (calls) {
        vec_free_handles(calls, toolcall_free);
    }
    fgc_record_free(m);
}

static void tmpl_free(int64_t t) {
    if (!t) {
        return;
    }
    int64_t src = rec_getv(t, TK_SRC);
    int64_t bos = rec_getv(t, TK_BOS);
    int64_t eos = rec_getv(t, TK_EOS);
    if (src) { fgc_str_free(src); }
    if (bos) { fgc_str_free(bos); }
    if (eos) { fgc_str_free(eos); }
    int64_t vendor = rec_getv(t, TK_VENDOR);
    if (vendor) {
        delete (TmplVendor *) (intptr_t) vendor;
    }
    fgc_record_free(t);
}

static void res_clear(int64_t plan) {
    int64_t res = rec_getv(plan, PK_RESULT);
    if (!res) {
        return;
    }
    static const int64_t str_keys[] = {
        RK_THINKING_START, RK_THINKING_END, RK_GRAMMAR, RK_GEN_PROMPT,
        RK_PROMPT_JSON, RK_MESSAGES_JSON, RK_PROMPT_TEXT,
    };
    for (int64_t key : str_keys) {
        int64_t h = rec_getv(res, key);
        if (h) { fgc_str_free(h); }
        rec_setv(res, key, 0);
    }
    for (int64_t key = 0; key <= RK_PARALLEL; key++) {
        bool is_str = false;
        for (int64_t sk : str_keys) {
            if (sk == key) { is_str = true; break; }
        }
        if (!is_str) {
            rec_setv(res, key, 0);
        }
    }
}

/* --------------------------------------------------------- canonical plan */
extern "C" int64_t apx028_plan_new(void) {
    int64_t plan = fgc_record_new(APX_KIND_PLAN);
    if (!plan) {
        return 0;
    }
    int64_t msgs = fgc_vec_new(8);
    int64_t tools = fgc_vec_new(8);
    int64_t res = fgc_record_new(APX_KIND_RESULT);
    if (!msgs || !tools || !res) {
        if (msgs) { fgc_vec_free(msgs); }
        if (tools) { fgc_vec_free(tools); }
        if (res) { fgc_record_free(res); }
        fgc_record_free(plan);
        return 0;
    }
    PlanVendor * vendor = new (std::nothrow) PlanVendor();
    if (!vendor) {
        fgc_vec_free(msgs);
        fgc_vec_free(tools);
        fgc_record_free(res);
        fgc_record_free(plan);
        return 0;
    }
    rec_setv(plan, PK_MSGS, msgs);
    rec_setv(plan, PK_TOOLS, tools);
    rec_setv(plan, PK_RESULT, res);
    rec_setv(plan, PK_VENDOR, (int64_t) (intptr_t) vendor);
    rec_setv(plan, PK_CONT, APX_CONT_NONE);
    rec_setv(plan, PK_TOOL_CHOICE, APX_TOOL_AUTO);
    rec_setv(plan, PK_REASONING_FORMAT, APX_REASON_FMT_NONE);
    rec_setv(plan, PK_ADD_GEN, 1);
    rec_setv(plan, PK_ENABLE_THINKING, 1);
    rec_setv(plan, PK_MARK_INPUT, 1);
    rec_setv(plan, PK_CAPS_OVERRIDE, -1);
    rec_setv(plan, PK_FLAGS, APX_F_USE_JINJA | APX_F_ADD_GEN | APX_F_ENABLE_THINKING | APX_F_MARK_INPUT);
    return plan;
}

extern "C" void apx028_plan_free(int64_t plan) {
    if (!plan) {
        return;
    }
    int64_t msgs = rec_getv(plan, PK_MSGS);
    if (msgs) { vec_free_handles(msgs, msg_free); }
    int64_t tools = rec_getv(plan, PK_TOOLS);
    if (tools) { vec_free_handles(tools, tool_free); }
    msg_free(rec_getv(plan, PK_CONT_MSG));
    tmpl_free(rec_getv(plan, PK_TMPL_DEFAULT));
    tmpl_free(rec_getv(plan, PK_TMPL_TOOL_USE));
    res_clear(plan);
    int64_t res = rec_getv(plan, PK_RESULT);
    if (res) { fgc_record_free(res); }
    static const int64_t str_keys[] = {
        PK_KWARGS_JSON, PK_GRAMMAR, PK_JSON_SCHEMA, PK_REASON_START,
        PK_REASON_END, PK_ASSISTANT_START, PK_USER_START, PK_VIEW,
    };
    for (int64_t key : str_keys) {
        int64_t h = rec_getv(plan, key);
        if (h) { fgc_str_free(h); }
    }
    int64_t vendor = rec_getv(plan, PK_VENDOR);
    if (vendor) {
        delete (PlanVendor *) (intptr_t) vendor;
    }
    fgc_record_free(plan);
}

extern "C" int64_t apx028_plan_set_template(int64_t plan, int64_t which,
                                            const char * src, const char * bos, const char * eos) {
    if (!plan || !src || (which != 0 && which != 1)) {
        return APX_ERR_INVAL;
    }
    int64_t rec = fgc_record_new(APX_KIND_TEMPLATE);
    if (!rec) {
        return APX_ERR_STATE;
    }
    int64_t s = s_new_cstr(src);
    int64_t b = s_new_cstr(bos ? bos : "");
    int64_t e = s_new_cstr(eos ? eos : "");
    if (!s || !b || !e) {
        if (s) { fgc_str_free(s); }
        if (b) { fgc_str_free(b); }
        if (e) { fgc_str_free(e); }
        fgc_record_free(rec);
        return APX_ERR_STATE;
    }
    rec_setv(rec, TK_SRC, s);
    rec_setv(rec, TK_BOS, b);
    rec_setv(rec, TK_EOS, e);

    TmplVendor * vendor = nullptr;
    int64_t caps = 0;
    int64_t src_flags = 0;
    try {
        jinja::lexer lexer;
        auto lexer_res = lexer.tokenize(src);
        vendor = new TmplVendor();
        vendor->prog = jinja::parse_from_tokens(lexer_res);
        jinja::caps c = jinja::caps_get(vendor->prog);
        caps |= c.supports_tools ? APX_C_TOOLS : 0;
        caps |= c.supports_tool_calls ? APX_C_TOOL_CALLS : 0;
        caps |= c.supports_system_role ? APX_C_SYSTEM : 0;
        caps |= c.supports_parallel_tool_calls ? APX_C_PARALLEL : 0;
        caps |= c.supports_string_content ? APX_C_STRING : 0;
        caps |= c.supports_typed_content ? APX_C_TYPED : 0;
        caps |= c.supports_object_arguments ? APX_C_OBJECT_ARGS : 0;
        caps |= c.supports_preserve_reasoning ? APX_C_PRESERVE_REASONING : 0;
        caps |= c.supports_reasoning_effort ? APX_C_REASONING_EFFORT : 0;
    } catch (...) {
        delete vendor;
        fgc_str_free(s);
        fgc_str_free(b);
        fgc_str_free(e);
        fgc_record_free(rec);
        return APX_ERR_INVAL;
    }
    std::string text(src);
    if (text.find("<|channel|>") != std::string::npos) {
        src_flags |= APX_S_CHANNEL;
    }
    if (text.find("You have access to the following functions in JSONSchema format") != std::string::npos) {
        src_flags |= APX_S_STEPFUN;
    }
    rec_setv(rec, TK_CAPS, caps);
    rec_setv(rec, TK_SRC_FLAGS, src_flags);
    rec_setv(rec, TK_VENDOR, (int64_t) (intptr_t) vendor);

    int64_t old = rec_getv(plan, which == 0 ? PK_TMPL_DEFAULT : PK_TMPL_TOOL_USE);
    if (old) {
        tmpl_free(old);
    }
    rec_setv(plan, which == 0 ? PK_TMPL_DEFAULT : PK_TMPL_TOOL_USE, rec);
    return 0;
}

static int64_t msg_from_json(const common_json & j) {
    if (!j.is_object()) {
        return 0;
    }
    int64_t m = fgc_record_new(APX_KIND_MSG);
    if (!m) {
        return 0;
    }
    int64_t flags = 0;
    std::string role = j.contains("role") ? (std::string) j.at("role") : std::string();
    rec_setv(m, MK_ROLE, role_from_name(role));

    // common_chat_msg::to_json_oaicompat always writes a content field.
    flags |= MF_HAS_CONTENT;
    int64_t content = 0;
    int64_t parts = 0;
    if (j.contains("content")) {
        const common_json & c = j.at("content");
        if (c.is_string()) {
            content = s_new((std::string) c);
        } else if (c.is_array()) {
            parts = fgc_vec_new(8);
            if (!parts) { msg_free(m); return 0; }
            for (size_t i = 0; i < c.size(); i++) {
                const common_json & p = c.at(i);
                int64_t part = fgc_record_new(APX_KIND_PART);
                if (!part) { msg_free(m); return 0; }
                std::string type = p.contains("type") ? (std::string) p.at("type") : std::string();
                std::string text = p.contains("text") ? (std::string) p.at("text") : std::string();
                rec_setv(part, PT_TYPE, s_new(type));
                rec_setv(part, PT_TEXT, s_new(text));
                vec_push(parts, part);
            }
        }
    }
    if (!content && !parts) {
        content = s_new(std::string());
    }
    if (content) {
        rec_setv(m, MK_CONTENT, content);
    }
    if (parts) {
        rec_setv(m, MK_PARTS, parts);
    }

    if (j.contains("reasoning_content")) {
        rec_setv(m, MK_REASONING, s_new((std::string) j.at("reasoning_content")));
        flags |= MF_HAS_REASONING;
    }
    if (j.contains("name")) {
        rec_setv(m, MK_NAME, s_new((std::string) j.at("name")));
        flags |= MF_HAS_NAME;
    }
    if (j.contains("tool_call_id")) {
        rec_setv(m, MK_TCID, s_new((std::string) j.at("tool_call_id")));
        flags |= MF_HAS_TCID;
    }

    if (j.contains("tool_calls") && j.at("tool_calls").is_array()) {
        const common_json & calls = j.at("tool_calls");
        int64_t vec = fgc_vec_new(8);
        if (!vec) { msg_free(m); return 0; }
        for (size_t i = 0; i < calls.size(); i++) {
            const common_json & tc = calls.at(i);
            const common_json & fn = tc.contains("function") ? tc.at("function") : tc;
            int64_t call = fgc_record_new(APX_KIND_TOOLCALL);
            if (!call) { fgc_vec_free(vec); msg_free(m); return 0; }
            std::string name = fn.contains("name") ? (std::string) fn.at("name") : std::string();
            std::string args;
            int64_t is_object = 0;
            if (fn.contains("arguments")) {
                const common_json & a = fn.at("arguments");
                if (a.is_string()) {
                    args = (std::string) a;
                } else {
                    args = a.dump();
                    is_object = 1;
                }
            }
            std::string id = tc.contains("id") ? (std::string) tc.at("id") : std::string();
            rec_setv(call, TC_NAME, s_new(name));
            rec_setv(call, TC_ARGS, s_new(args));
            rec_setv(call, TC_ID, s_new(id));
            rec_setv(call, TC_IS_OBJECT, is_object);
            vec_push(vec, call);
        }
        rec_setv(m, MK_TOOLCALLS, vec);
    }

    rec_setv(m, MK_FLAGS, flags);
    return m;
}

extern "C" int64_t apx028_plan_add_message_json(int64_t plan, const char * message_json) {
    if (!plan || !message_json) {
        return APX_ERR_INVAL;
    }
    try {
        common_json j = common_json::parse(message_json);
        int64_t m = msg_from_json(j);
        if (!m) {
            return APX_ERR_STATE;
        }
        int64_t msgs = rec_getv(plan, PK_MSGS);
        vec_push(msgs, m);
        return 0;
    } catch (...) {
        return APX_ERR_INVAL;
    }
}

extern "C" int64_t apx028_plan_set_tools_json(int64_t plan, const char * tools_json) {
    if (!plan || !tools_json) {
        return APX_ERR_INVAL;
    }
    try {
        common_json j = common_json::parse(tools_json);
        if (!j.is_array()) {
            return APX_ERR_INVAL;
        }
        int64_t tools = rec_getv(plan, PK_TOOLS);
        vec_free_handles(tools, tool_free);
        tools = fgc_vec_new(8);
        rec_setv(plan, PK_TOOLS, tools);
        for (size_t i = 0; i < j.size(); i++) {
            const common_json & tool = j.at(i);
            const common_json & fn = tool.contains("function") ? tool.at("function") : tool;
            int64_t t = fgc_record_new(APX_KIND_TOOL);
            if (!t) { return APX_ERR_STATE; }
            std::string name = fn.contains("name") ? (std::string) fn.at("name") : std::string();
            std::string desc = fn.contains("description") ? (std::string) fn.at("description") : std::string();
            std::string par = fn.contains("parameters") ? fn.at("parameters").dump() : std::string("{}");
            rec_setv(t, TL_NAME, s_new(name));
            rec_setv(t, TL_DESC, s_new(desc));
            rec_setv(t, TL_PARAM, s_new(par));
            vec_push(tools, t);
        }
        return 0;
    } catch (...) {
        return APX_ERR_INVAL;
    }
}

extern "C" int64_t apx028_plan_add_kwarg(int64_t plan, const char * key, const char * value) {
    if (!plan || !key || !value) {
        return APX_ERR_INVAL;
    }
    try {
        common_json obj = common_json::object();
        int64_t old = rec_getv(plan, PK_KWARGS_JSON);
        if (old) {
            std::string prev = s_get(old);
            if (!prev.empty()) {
                obj = common_json::parse(prev);
            }
        }
        obj[std::string(key)] = common_json::parse(value);
        std::string dumped = obj.dump();
        str_replace_key(plan, PK_KWARGS_JSON, dumped);
        return 0;
    } catch (...) {
        return APX_ERR_INVAL;
    }
}

extern "C" int64_t apx028_plan_set_grammar(int64_t plan, const char * grammar) {
    if (!plan) { return APX_ERR_INVAL; }
    str_replace_key(plan, PK_GRAMMAR, std::string(grammar ? grammar : ""));
    return 0;
}

extern "C" int64_t apx028_plan_set_json_schema(int64_t plan, const char * json_schema) {
    if (!plan) { return APX_ERR_INVAL; }
    str_replace_key(plan, PK_JSON_SCHEMA, std::string(json_schema ? json_schema : ""));
    return 0;
}

extern "C" void apx028_plan_set_int(int64_t plan, int64_t key, int64_t value) {
    rec_setv(plan, key, value);
}

extern "C" int64_t apx028_plan_get_int(int64_t plan, int64_t key) {
    return rec_getv(plan, key);
}

extern "C" void apx028_plan_set_caps_override(int64_t plan, int64_t which, int64_t caps) {
    (void) which;
    rec_setv(plan, PK_CAPS_OVERRIDE, caps);
}

extern "C" void apx028_plan_set_analysis(int64_t plan, int64_t ready, int64_t mode,
                                         const char * start, const char * end) {
    rec_setv(plan, PK_ANALYSIS_READY, ready);
    rec_setv(plan, PK_REASON_MODE, mode);
    str_replace_key(plan, PK_REASON_START, std::string(start ? start : ""));
    str_replace_key(plan, PK_REASON_END, std::string(end ? end : ""));
}

extern "C" void apx028_plan_set_boundaries(int64_t plan, const char * assistant_start, const char * user_start) {
    if (!plan) { return; }
    str_replace_key(plan, PK_ASSISTANT_START, std::string(assistant_start ? assistant_start : ""));
    str_replace_key(plan, PK_USER_START, std::string(user_start ? user_start : ""));
}

extern "C" void apx028_plan_set_native(int64_t plan, int64_t native_tmpl, int64_t native_inputs) {
    int64_t v = rec_getv(plan, PK_VENDOR);
    if (v) {
        PlanVendor * vendor = (PlanVendor *) (intptr_t) v;
        vendor->native_tmpl = native_tmpl;
        vendor->native_inputs = native_inputs;
    }
}

extern "C" int64_t apx028_plan_get_native(int64_t plan, int64_t which) {
    int64_t v = rec_getv(plan, PK_VENDOR);
    if (!v) {
        return 0;
    }
    PlanVendor * vendor = (PlanVendor *) (intptr_t) v;
    return which == 0 ? vendor->native_tmpl : vendor->native_inputs;
}

extern "C" void apx028_plan_set_native_result(int64_t plan, int64_t result_ptr) {
    int64_t v = rec_getv(plan, PK_VENDOR);
    if (v) {
        ((PlanVendor *) (intptr_t) v)->native_result = result_ptr;
    }
}

extern "C" int64_t apx028_plan_take_native_result(int64_t plan) {
    int64_t v = rec_getv(plan, PK_VENDOR);
    if (!v) {
        return 0;
    }
    PlanVendor * vendor = (PlanVendor *) (intptr_t) v;
    int64_t result = vendor->native_result;
    vendor->native_result = 0;
    return result;
}

extern "C" int64_t apx028_plan_apply(int64_t plan) {
    if (!plan) {
        return APX_ERR_INVAL;
    }
    return apx028_chat_entry(APX_OP_APPLY, plan, 0, 0, 0);
}

extern "C" int64_t apx028_plan_detect(int64_t plan) {
    if (!plan) {
        return APX_ERR_INVAL;
    }
    return apx028_chat_entry(APX_OP_DETECT, plan, 0, 0, 0);
}

/* --------------------------------------------------------------- plan get */
extern "C" int64_t apx028_plan_get(int64_t plan, int64_t key) { return rec_getv(plan, key); }
extern "C" void    apx028_plan_set(int64_t plan, int64_t key, int64_t value) { rec_setv(plan, key, value); }

extern "C" int64_t apx028_plan_template(int64_t plan, int64_t which) {
    return rec_getv(plan, which == 0 ? PK_TMPL_DEFAULT : PK_TMPL_TOOL_USE);
}

extern "C" int64_t apx028_plan_msgs(int64_t plan) { return rec_getv(plan, PK_MSGS); }

extern "C" int64_t apx028_plan_msgs_count(int64_t plan) {
    return fgc_vec_len(rec_getv(plan, PK_MSGS));
}

extern "C" int64_t apx028_plan_cont_msgs_count(int64_t plan) {
    return rec_getv(plan, PK_CONT_MSG) ? 1 : 0;
}

extern "C" int64_t apx028_plan_tools_count(int64_t plan) {
    return fgc_vec_len(rec_getv(plan, PK_TOOLS));
}

extern "C" int64_t apx028_plan_grammar_len(int64_t plan) {
    return fgc_str_len(rec_getv(plan, PK_GRAMMAR));
}

extern "C" int64_t apx028_tmpl_caps(int64_t tmpl) { return rec_getv(tmpl, TK_CAPS); }

extern "C" int64_t apx028_plan_caps(int64_t plan, int64_t tmpl) {
    int64_t o = rec_getv(plan, PK_CAPS_OVERRIDE);
    return o >= 0 ? o : rec_getv(tmpl, TK_CAPS);
}

extern "C" int64_t apx028_tmpl_src_flags(int64_t tmpl) { return rec_getv(tmpl, TK_SRC_FLAGS); }

extern "C" int64_t apx028_tmpl_src(int64_t tmpl, int64_t field) {
    if (field == 0) { return rec_getv(tmpl, TK_SRC); }
    if (field == 1) { return rec_getv(tmpl, TK_BOS); }
    if (field == 2) { return rec_getv(tmpl, TK_EOS); }
    return 0;
}

extern "C" int64_t apx028_tmpl_has(int64_t tmpl, int64_t needle) {
    const char * text = needle_text(needle);
    if (!text) {
        return 0;
    }
    std::string src = s_get(rec_getv(tmpl, TK_SRC));
    return src.find(text) != std::string::npos ? 1 : 0;
}

/* ------------------------------------------------------------- messages */
extern "C" int64_t apx028_msg_at(int64_t msgs, int64_t index) { return vec_at(msgs, index); }
extern "C" int64_t apx028_msg_role(int64_t msg) { return rec_getv(msg, MK_ROLE); }
extern "C" void    apx028_msg_set_role(int64_t msg, int64_t role) { rec_setv(msg, MK_ROLE, role); }

extern "C" int64_t apx028_msg_content_len(int64_t msg) {
    int64_t h = rec_getv(msg, MK_CONTENT);
    return h ? fgc_str_len(h) : -1;
}

extern "C" int64_t apx028_msg_reasoning_len(int64_t msg) {
    int64_t h = rec_getv(msg, MK_REASONING);
    return h ? fgc_str_len(h) : 0;
}

extern "C" int64_t apx028_msg_parts_len(int64_t msg) {
    return fgc_vec_len(rec_getv(msg, MK_PARTS));
}

extern "C" int64_t apx028_msg_has_tool_calls(int64_t msg) {
    return fgc_vec_len(rec_getv(msg, MK_TOOLCALLS)) > 0 ? 1 : 0;
}

extern "C" int64_t apx028_msg_has_content(int64_t msg) {
    return (rec_getv(msg, MK_FLAGS) & MF_HAS_CONTENT) != 0 ? 1 : 0;
}

extern "C" void apx028_msg_set_empty_content(int64_t msg) {
    int64_t old = rec_getv(msg, MK_CONTENT);
    if (old) { fgc_str_free(old); }
    rec_setv(msg, MK_CONTENT, s_new(std::string()));
    rec_setv(msg, MK_FLAGS, rec_getv(msg, MK_FLAGS) | MF_HAS_CONTENT);
}

extern "C" int64_t apx028_msgs_merge_content(int64_t msgs, int64_t dst_index, int64_t src_index) {
    int64_t dst = vec_at(msgs, dst_index);
    int64_t src = vec_at(msgs, src_index);
    if (!dst || !src) {
        return APX_ERR_STATE;
    }
    std::string merged = s_get(rec_getv(src, MK_CONTENT)) + "\n" + s_get(rec_getv(dst, MK_CONTENT));
    str_replace_key(dst, MK_CONTENT, merged);
    rec_setv(dst, MK_FLAGS, rec_getv(dst, MK_FLAGS) | MF_HAS_CONTENT);
    return 0;
}

extern "C" int64_t apx028_msgs_erase_first(int64_t msgs) {
    int64_t n = fgc_vec_len(msgs);
    if (n <= 0) {
        return 0;
    }
    int64_t * data = (int64_t *) fgc_vec_data(msgs);
    if (!data) {
        return APX_ERR_STATE;
    }
    int64_t first = data[0];
    for (int64_t i = 1; i < n; i++) {
        data[i - 1] = data[i];
    }
    fgc_vec_resize(msgs, n - 1);
    msg_free(first);
    return 0;
}

extern "C" int64_t apx028_msg_parse_args(int64_t msg) {
    int64_t calls = rec_getv(msg, MK_TOOLCALLS);
    int64_t n = fgc_vec_len(calls);
    for (int64_t i = 0; i < n; i++) {
        int64_t call = vec_at(calls, i);
        if (!call) { continue; }
        if (rec_getv(call, TC_IS_OBJECT)) { continue; }
        std::string args = s_get(rec_getv(call, TC_ARGS));
        try {
            common_json parsed = common_json::parse(args);
            if (!parsed.is_object() && !parsed.is_array() && !parsed.is_null()) {
                return APX_ERR_ARGS_JSON;
            }
            str_replace_key(call, TC_ARGS, parsed.dump());
            rec_setv(call, TC_IS_OBJECT, 1);
        } catch (...) {
            return APX_ERR_ARGS_JSON;
        }
    }
    return 0;
}

extern "C" int64_t apx028_msg_trim(int64_t msg) {
    int64_t content = rec_getv(msg, MK_CONTENT);
    if (content) {
        str_replace_key(msg, MK_CONTENT, trim_ws(s_get(content)));
    }
    int64_t reasoning = rec_getv(msg, MK_REASONING);
    if (reasoning) {
        str_replace_key(msg, MK_REASONING, trim_ws(s_get(reasoning)));
    }
    int64_t parts = rec_getv(msg, MK_PARTS);
    int64_t n = fgc_vec_len(parts);
    for (int64_t i = 0; i < n; i++) {
        int64_t part = vec_at(parts, i);
        if (!part) { continue; }
        std::string type = s_get(rec_getv(part, PT_TYPE));
        if (type == "text") {
            str_replace_key(part, PT_TEXT, trim_ws(s_get(rec_getv(part, PT_TEXT))));
        }
    }
    return 0;
}

/* --------------------------------------------------------------- result */
extern "C" int64_t apx028_res_get(int64_t plan, int64_t key) {
    return rec_getv(rec_getv(plan, PK_RESULT), key);
}

extern "C" void apx028_res_set(int64_t plan, int64_t key, int64_t value) {
    rec_setv(rec_getv(plan, PK_RESULT), key, value);
}

extern "C" int64_t apx028_res_str_copy(int64_t plan, int64_t key, int64_t src_handle) {
    int64_t res = rec_getv(plan, PK_RESULT);
    int64_t old = rec_getv(res, key);
    if (old) { fgc_str_free(old); }
    int64_t h = src_handle ? s_new(s_get(src_handle)) : 0;
    rec_setv(res, key, h);
    return h ? 0 : APX_ERR_STATE;
}

extern "C" int64_t apx028_res_str_take(int64_t plan, int64_t key, int64_t str_handle) {
    int64_t res = rec_getv(plan, PK_RESULT);
    int64_t old = rec_getv(res, key);
    if (old) { fgc_str_free(old); }
    rec_setv(res, key, str_handle);
    return 0;
}

extern "C" int64_t apx028_res_set_text(int64_t plan, int64_t key, const char * text) {
    int64_t h = s_new_cstr(text);
    if (!h) { return APX_ERR_STATE; }
    return apx028_res_str_take(plan, key, h);
}

extern "C" int64_t apx028_str_dup(int64_t handle) { return handle ? s_new(s_get(handle)) : 0; }
extern "C" int64_t apx028_str_trim_dup(int64_t handle) { return handle ? s_new(trim_ws(s_get(handle))) : 0; }
extern "C" int64_t apx028_str_len(int64_t handle) { return fgc_str_len(handle); }
extern "C" void    apx028_str_free(int64_t handle) { if (handle) { fgc_str_free(handle); } }

/* ------------------------------------------------------------ JSON view */
static std::string concat_text_parts(int64_t msg) {
    int64_t parts = rec_getv(msg, MK_PARTS);
    int64_t n = fgc_vec_len(parts);
    std::string out;
    bool last_media = false;
    for (int64_t i = 0; i < n; i++) {
        int64_t part = vec_at(parts, i);
        if (!part) { continue; }
        std::string type = s_get(rec_getv(part, PT_TYPE));
        std::string text = s_get(rec_getv(part, PT_TEXT));
        if (type == "text") {
            if (!last_media && !out.empty()) {
                out += '\n';
            }
            last_media = false;
            out += text;
        } else if (type == "media_marker") {
            last_media = true;
            out += text;
        }
    }
    return out;
}

static common_json msg_to_json(int64_t msg, int64_t caps) {
    common_json o = common_json::object();
    o["role"] = std::string(role_name(rec_getv(msg, MK_ROLE)));

    int64_t flags = rec_getv(msg, MK_FLAGS);
    bool only_string = ((caps & APX_C_STRING) != 0) && ((caps & APX_C_TYPED) == 0);
    bool only_typed  = ((caps & APX_C_STRING) == 0) && ((caps & APX_C_TYPED) != 0);
    int64_t content = rec_getv(msg, MK_CONTENT);
    int64_t parts = rec_getv(msg, MK_PARTS);
    bool has_content = (flags & MF_HAS_CONTENT) != 0;
    int64_t n_parts = fgc_vec_len(parts);

    if (n_parts > 0 && only_string) {
        o["content"] = concat_text_parts(msg);
    } else if (n_parts > 0 && only_typed) {
        common_json arr = common_json::array();
        for (int64_t i = 0; i < n_parts; i++) {
            int64_t part = vec_at(parts, i);
            if (!part) { continue; }
            arr.push_back({
                { "type", s_get(rec_getv(part, PT_TYPE)) },
                { "text", s_get(rec_getv(part, PT_TEXT)) },
            });
        }
        o["content"] = arr;
    } else if (n_parts > 0) {
        // neither string-only nor typed-only: default JSON object shape keeps parts
        common_json arr = common_json::array();
        for (int64_t i = 0; i < n_parts; i++) {
            int64_t part = vec_at(parts, i);
            if (!part) { continue; }
            arr.push_back({
                { "type", s_get(rec_getv(part, PT_TYPE)) },
                { "text", s_get(rec_getv(part, PT_TEXT)) },
            });
        }
        o["content"] = arr;
    } else if (has_content && only_typed) {
        common_json arr = common_json::array();
        arr.push_back({{"type", "text"}, {"text", s_get(content)}});
        o["content"] = arr;
    } else if (has_content) {
        o["content"] = s_get(content);
    } else {
        o["content"] = std::string();
    }

    if (flags & MF_HAS_REASONING) {
        o["reasoning_content"] = s_get(rec_getv(msg, MK_REASONING));
    }
    if (flags & MF_HAS_NAME) {
        o["name"] = s_get(rec_getv(msg, MK_NAME));
    }
    if (flags & MF_HAS_TCID) {
        o["tool_call_id"] = s_get(rec_getv(msg, MK_TCID));
    }

    int64_t calls = rec_getv(msg, MK_TOOLCALLS);
    int64_t n_calls = fgc_vec_len(calls);
    if (n_calls > 0) {
        common_json arr = common_json::array();
        for (int64_t i = 0; i < n_calls; i++) {
            int64_t call = vec_at(calls, i);
            if (!call) { continue; }
            std::string name = s_get(rec_getv(call, TC_NAME));
            std::string args = s_get(rec_getv(call, TC_ARGS));
            common_json args_json;
            if (rec_getv(call, TC_IS_OBJECT)) {
                try {
                    args_json = common_json::parse(args);
                } catch (...) {
                    args_json = args;
                }
            } else {
                args_json = args;
            }
            common_json fn = common_json::object();
            fn["name"] = name;
            fn["arguments"] = args_json;
            common_json tc = common_json::object();
            tc["type"] = std::string("function");
            tc["function"] = fn;
            std::string id = s_get(rec_getv(call, TC_ID));
            if (!id.empty()) {
                tc["id"] = id;
            }
            arr.push_back(tc);
        }
        o["tool_calls"] = arr;
    }
    return o;
}

static common_json msgs_to_json(int64_t plan, int64_t tmpl) {
    int64_t caps = apx028_plan_caps(plan, tmpl);
    common_json arr = common_json::array();
    int64_t msgs = rec_getv(plan, PK_MSGS);
    int64_t n = fgc_vec_len(msgs);
    for (int64_t i = 0; i < n; i++) {
        int64_t m = vec_at(msgs, i);
        if (m) {
            arr.push_back(msg_to_json(m, caps));
        }
    }
    return arr;
}

static common_json tools_to_json(int64_t plan) {
    common_json arr = common_json::array();
    int64_t tools = rec_getv(plan, PK_TOOLS);
    int64_t n = fgc_vec_len(tools);
    for (int64_t i = 0; i < n; i++) {
        int64_t t = vec_at(tools, i);
        if (!t) { continue; }
        std::string params = s_get(rec_getv(t, TL_PARAM));
        common_json par;
        try {
            par = common_json::parse(params.empty() ? std::string("{}") : params);
        } catch (...) {
            par = common_json::object();
        }
        common_json fn = common_json::object();
        fn["name"] = s_get(rec_getv(t, TL_NAME));
        fn["description"] = s_get(rec_getv(t, TL_DESC));
        fn["parameters"] = par;
        common_json obj = common_json::object();
        obj["type"] = std::string("function");
        obj["function"] = fn;
        arr.push_back(obj);
    }
    return arr;
}

static common_json extra_context(int64_t plan) {
    common_json ctx = common_json::object();
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    {
        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%b %d %Y");
        ctx["datetime"] = ss.str();
    }
    {
        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%d %b %Y");
        ctx["date_string"] = ss.str();
    }
    int64_t kwargs = rec_getv(plan, PK_KWARGS_JSON);
    if (kwargs) {
        std::string text = s_get(kwargs);
        if (!text.empty()) {
            try {
                common_json obj = common_json::parse(text);
                if (obj.is_object()) {
                    for (const auto & [k, v] : obj.items()) {
                        ctx[k] = v;
                    }
                }
            } catch (...) {
            }
        }
    }
    return ctx;
}

extern "C" int64_t apx028_plan_extra_context(int64_t plan) {
    (void) plan;
    return 0;
}

extern "C" int64_t apx028_plan_parse_json_schema(int64_t plan) {
    int64_t schema = rec_getv(plan, PK_JSON_SCHEMA);
    if (!schema) {
        rec_setv(plan, PK_JSON_SCHEMA_PARSED, 0);
        return 0;
    }
    try {
        std::string text = s_get(schema);
        if (!text.empty()) {
            (void) common_json::parse(text);
        }
        rec_setv(plan, PK_JSON_SCHEMA_PARSED, 1);
        return 0;
    } catch (...) {
        rec_setv(plan, PK_JSON_SCHEMA_PARSED, 0);
        return APX_ERR_INVAL;
    }
}

extern "C" int64_t apx028_plan_pop_continuation(int64_t plan) {
    int64_t msgs = rec_getv(plan, PK_MSGS);
    int64_t n = fgc_vec_len(msgs);
    if (n <= 0) {
        return 0;
    }
    int64_t m = vec_at(msgs, n - 1);
    rec_setv(plan, PK_CONT_MSG, m);
    fgc_vec_resize(msgs, n - 1);
    return m;
}

static int64_t cont_field_len(int64_t plan, int64_t field) {
    int64_t m = rec_getv(plan, PK_CONT_MSG);
    if (!m) { return 0; }
    return field == 0 ? apx028_msg_reasoning_len(m)
         : field == 1 ? (apx028_msg_content_len(m) < 0 ? 0 : apx028_msg_content_len(m))
                      : apx028_msg_parts_len(m);
}

extern "C" int64_t apx028_plan_cont_reasoning_len(int64_t plan) { return cont_field_len(plan, 0); }
extern "C" int64_t apx028_plan_cont_content_len(int64_t plan) { return cont_field_len(plan, 1); }
extern "C" int64_t apx028_plan_cont_parts_len(int64_t plan) { return cont_field_len(plan, 2); }

/* --------------------------------------------------------------- render */
struct RenderedPrompt {
    std::vector<common_json> parts; // {text, input}
    std::string text;
};

static bool str_starts_with(const std::string & s, const std::string & p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

static bool str_ends_with(const std::string & s, const std::string & p) {
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}

static RenderedPrompt direct_render(int64_t plan, int64_t tmpl, bool add_gen, int64_t reasoning_format) {
    RenderedPrompt out;
    int64_t vendor_handle = rec_getv(tmpl, TK_VENDOR);
    TmplVendor * vendor = (TmplVendor *) (intptr_t) vendor_handle;
    if (!vendor) {
        return out;
    }
    std::string src = s_get(rec_getv(tmpl, TK_SRC));
    std::string bos = s_get(rec_getv(tmpl, TK_BOS));
    std::string eos = s_get(rec_getv(tmpl, TK_EOS));
    bool mark_input = rec_getv(plan, PK_MARK_INPUT) != 0;
    bool enable_thinking = rec_getv(plan, PK_ENABLE_THINKING) != 0;

    common_json inp = common_json::object();
    inp["messages"] = msgs_to_json(plan, tmpl);
    inp["bos_token"] = bos;
    inp["eos_token"] = eos;
    inp["enable_thinking"] = enable_thinking;
    if (apx028_plan_tools_count(plan) > 0) {
        inp["tools"] = tools_to_json(plan);
    }
    common_json ec = extra_context(plan);
    if (ec.is_object()) {
        for (const auto & [k, v] : ec.items()) {
            inp[k] = v;
        }
    }
    if (add_gen) {
        inp["add_generation_prompt"] = true;
    }
    (void) reasoning_format;

    jinja::context ctx(src);
    if (inp.contains("preserve_reasoning") && inp.at("preserve_reasoning").is_boolean()) {
        jinja::caps_apply_preserve_reasoning(ctx, inp.at("preserve_reasoning").get<bool>());
    }
    if (inp.contains("reasoning_effort") && inp.at("reasoning_effort").is_string() && !inp.at("reasoning_effort").empty()) {
        jinja::caps_apply_reasoning_effort(ctx, (std::string) inp.at("reasoning_effort"));
    }
    jinja::global_from_json(ctx, inp, mark_input);
    ctx.set_val("bos_token", jinja::mk_val<jinja::value_string>(bos));
    ctx.set_val("eos_token", jinja::mk_val<jinja::value_string>(eos));

    jinja::runtime runtime(ctx);
    jinja::value results = runtime.execute(vendor->prog);
    jinja::value_string parts = jinja::runtime::gather_string_parts(results);

    std::vector<std::pair<std::string, bool>> tmp;
    for (const auto & part : parts->val_str.parts) {
        tmp.push_back({part.val, part.is_input});
    }
    bool add_bos = rec_getv(plan, PK_ADD_BOS) != 0;
    bool add_eos = rec_getv(plan, PK_ADD_EOS) != 0;
    if (add_bos && !tmp.empty() && !tmp.front().second && str_starts_with(tmp.front().first, bos)) {
        tmp.front().first.erase(0, bos.size());
    }
    if (add_eos && !tmp.empty() && !tmp.back().second && str_ends_with(tmp.back().first, eos)) {
        tmp.back().first.resize(tmp.back().first.size() - eos.size());
    }
    for (const auto & p : tmp) {
        common_json part = common_json::object();
        part["text"] = p.first;
        part["input"] = p.second;
        out.parts.push_back(part);
        out.text += p.first;
    }
    return out;
}

static void store_prompt(int64_t plan, const RenderedPrompt & r) {
    common_json arr = common_json::array();
    for (const auto & p : r.parts) {
        arr.push_back(p);
    }
    int64_t h = s_new(arr.dump());
    if (h) {
        (void) apx028_res_str_take(plan, RK_PROMPT_JSON, h);
    }
    int64_t t = s_new(r.text);
    if (t) {
        (void) apx028_res_str_take(plan, RK_PROMPT_TEXT, t);
    }
}

extern "C" int64_t apx028_plan_render_pure(int64_t plan) {
    int64_t sel = rec_getv(plan, PK_SELECTED);
    if (!sel) { return APX_ERR_STATE; }
    bool add_gen = rec_getv(plan, PK_ADD_GEN) != 0;
    int64_t reasoning = rec_getv(plan, PK_REASONING_FORMAT);
    RenderedPrompt prompt = direct_render(plan, sel, add_gen, APX_REASON_FMT_NONE);
    store_prompt(plan, prompt);

    RenderedPrompt no_gen = direct_render(plan, sel, false, reasoning);
    RenderedPrompt yes_gen = direct_render(plan, sel, true, reasoning);
    size_t prefix = 0;
    size_t limit = std::min(no_gen.text.size(), yes_gen.text.size());
    while (prefix < limit && no_gen.text[prefix] == yes_gen.text[prefix]) {
        prefix++;
    }
    int64_t gp = s_new(yes_gen.text.substr(prefix));
    if (gp) {
        (void) apx028_res_str_take(plan, RK_GEN_PROMPT, gp);
    }
    int64_t grammar = rec_getv(plan, PK_GRAMMAR);
    if (grammar) {
        (void) apx028_res_str_copy(plan, RK_GRAMMAR, grammar);
    }
    return 0;
}

extern "C" int64_t apx028_plan_store_messages_json(int64_t plan) {
    int64_t sel = rec_getv(plan, PK_SELECTED);
    if (!sel) { return APX_ERR_STATE; }
    common_json arr = msgs_to_json(plan, sel);
    int64_t h = s_new(arr.dump());
    if (!h) { return APX_ERR_STATE; }
    return apx028_res_str_take(plan, RK_MESSAGES_JSON, h);
}

extern "C" int64_t apx028_plan_analysis_ready(int64_t plan) {
    return rec_getv(plan, PK_ANALYSIS_READY);
}

extern "C" int64_t apx028_plan_reason_mode(int64_t plan) {
    return rec_getv(plan, PK_REASON_MODE);
}

extern "C" int64_t apx028_plan_analysis_str(int64_t plan, int64_t which) {
    switch (which) {
        case 0: return rec_getv(plan, PK_ASSISTANT_START);
        case 1: return rec_getv(plan, PK_USER_START);
        case 2: return rec_getv(plan, PK_REASON_START);
        case 3: return rec_getv(plan, PK_REASON_END);
        default: return 0;
    }
}

extern "C" const char * apx028_plan_analysis_text(int64_t plan, int64_t which) {
    int64_t h = apx028_plan_analysis_str(plan, which);
    if (!h) { return ""; }
    const char * p = fgc_str_data(h);
    return p ? p : "";
}

extern "C" int64_t apx028_plan_add_delimiter(int64_t plan, int64_t role, int64_t str_handle) {
    (void) role;
    (void) str_handle;
    // Delimiter pairs are retained in the analysis strings and owned by the
    // plan; the native adapter reads them back.  No new owned container.
    return 0;
}

extern "C" void apx028_plan_clear_result(int64_t plan) {
    res_clear(plan);
}

/* ---------------------------------------------------------- result view */
extern "C" int64_t apx028_plan_result_int(int64_t plan, int64_t key) {
    return apx028_res_get(plan, key);
}

extern "C" const char * apx028_plan_result_str(int64_t plan, int64_t key) {
    int64_t h = apx028_res_get(plan, key);
    if (!h) { return ""; }
    const char * p = fgc_str_data(h);
    return p ? p : "";
}

extern "C" const char * apx028_plan_messages_json(int64_t plan) {
    return apx028_plan_result_str(plan, RK_MESSAGES_JSON);
}

extern "C" const char * apx028_plan_prompt_json(int64_t plan) {
    return apx028_plan_result_str(plan, RK_PROMPT_JSON);
}

extern "C" const char * apx028_plan_prompt_text(int64_t plan) {
    return apx028_plan_result_str(plan, RK_PROMPT_TEXT);
}

/* ------------------------------------- additional views for native effects
 * These are read-only JSON/text projections of the same canonical plan state;
 * they allocate a fresh string owned by the plan so a caller can read it until
 * the next call or plan free. */
static const char * store_view(int64_t plan, int64_t key, const std::string & text) {
    int64_t h = s_new(text);
    if (!h) { return ""; }
    int64_t old = rec_getv(plan, key);
    if (old) { fgc_str_free(old); }
    rec_setv(plan, key, h);
    const char * p = fgc_str_data(h);
    return p ? p : "";
}

extern "C" const char * apx028_plan_tools_json(int64_t plan) {
    return store_view(plan, PK_VIEW, tools_to_json(plan).dump());
}

extern "C" const char * apx028_plan_extra_context_json(int64_t plan) {
    return store_view(plan, PK_VIEW, extra_context(plan).dump());
}

extern "C" const char * apx028_plan_continuation_json(int64_t plan) {
    int64_t m = rec_getv(plan, PK_CONT_MSG);
    if (!m) { return ""; }
    int64_t sel = rec_getv(plan, PK_SELECTED);
    int64_t caps = sel ? apx028_plan_caps(plan, sel) : (APX_C_STRING);
    return store_view(plan, PK_VIEW, msg_to_json(m, caps).dump());
}

extern "C" const char * apx028_plan_grammar_text(int64_t plan) {
    int64_t h = rec_getv(plan, PK_GRAMMAR);
    return h ? fgc_str_data(h) : "";
}

extern "C" const char * apx028_plan_json_schema_text(int64_t plan) {
    int64_t h = rec_getv(plan, PK_JSON_SCHEMA);
    return h ? fgc_str_data(h) : "";
}

/* ---------------------------------------------- native parser effects
 * Weak definitions: an isolated link must fail closed, never report success
 * for a parser body that is not present.  The production adapter supplies
 * strong definitions over the real PEG autoparser and parsers/*.cpp. */
extern "C" __attribute__((weak)) int64_t apx028_native_specialized_apply(int64_t plan, int64_t handler) {
    (void) plan;
    (void) handler;
    return APX_ERR_NO_ENGINE;
}

extern "C" __attribute__((weak)) int64_t apx028_native_autoparser_generate(int64_t plan) {
    (void) plan;
    return APX_ERR_NO_ENGINE;
}
