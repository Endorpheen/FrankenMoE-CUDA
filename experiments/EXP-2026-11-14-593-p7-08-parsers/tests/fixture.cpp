// Differential gate for the second batch of model-output parsers (FG572).
//
// For each case the same raw continuation is parsed twice:
//   native: common_chat_session(tmpls, nullptr, inputs).finish(raw)
//           (no weights, no vocab: the PEG parser is template-driven)
//   port:   the Goose export from draft.goose, compiled to runtime-gen.h/c
// The compared fields are role, content, reasoning_content, the number of
// tool calls and, per call, name, arguments and id:
//   values = 4 per case + 3 per tool call.
//
// Argument types for the schema-typed parsers come from the tool JSON schema
// through common_chat_schema_from_json, exactly the document the native
// parser uses:
//   only string        -> D_STR
//   no string at all   -> D_JSON
//   string among others-> D_MIXED
//
// This file is not built here; tests/build.sh does it.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>

#include "runtime-gen.h"
#include "chat.h"
#include "json.h"
#include "json-schema.h"

using json = common_json;

// The canonical runtime declares these libc shims as extern.
extern "C" double gs_json_strtod(sl_u8 s) {
    std::string tmp((const char *) s.data, (size_t) s.len);
    return std::strtod(tmp.c_str(), nullptr);
}
extern "C" int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
}
extern "C" int64_t gs_json_fmt_g(sl_u8 out, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.17g", x);
}
extern "C" double gs_frexp_mant(double x) {
    int e = 0;
    return std::frexp(x, &e);
}
extern "C" int64_t gs_strftime_epoch(sl_u8 out, int64_t epoch, sl_u8 fmt) {
    const std::string f((const char *) fmt.data, (size_t) fmt.len);
    const std::time_t t = epoch < 0 ? std::time(nullptr) : (std::time_t) epoch;
    const std::tm * tmv = std::localtime(&t);
    if (!tmv) return -1;
    const size_t n = std::strftime((char *) out.data, (size_t) out.len, f.c_str(), tmv);
    return (int64_t) n;
}
extern "C" int64_t gs_to_string_f64(sl_u8 out, double x) {
    const std::string s = std::to_string(x);
    memcpy(out.data, s.data(), s.size());
    return (int64_t) s.size();
}

static int g_cases = 0, g_values = 0, g_fails = 0;

static void bad(const char * name, const std::string & detail) {
    ++g_fails;
    if (g_fails <= 100) {
        printf("FAIL %s: %s\n", name, detail.c_str());
    }
}

static sl_u8 sl(std::string & s) {
    return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() };
}

static std::string visible(const std::string & s, size_t max = 300) {
    std::string r;
    size_t n = 0;
    for (unsigned char c : s) {
        if (n++ > max) { r += "..."; break; }
        if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c == '\\') r += "\\\\";
        else if (c == '"') r += "\\\"";
        else if (c >= 32 && c < 127) r.push_back((char) c);
        else { char b[8]; snprintf(b, sizeof b, "\\x%02X", c); r += b; }
    }
    return r;
}

// ---- argument type table ---------------------------------------------------

struct TypeTable {
    std::string          packed;
    std::vector<int64_t> off;
    std::vector<int64_t> len;
    std::vector<int64_t> type;

    void add(const std::string & name, int64_t ty) {
        off.push_back((int64_t) packed.size());
        len.push_back((int64_t) name.size());
        type.push_back(ty);
        packed += name;
    }
};

// 0 = string only, 1 = no string (JSON), 2 = string among others.
static void fill_table(TypeTable & table, const common_chat_tool & tool) {
    if (tool.parameters.empty()) {
        return;
    }
    json params;
    try {
        params = json::parse(tool.parameters);
    } catch (const std::exception &) {
        return;
    }
    auto doc = common_chat_schema_from_json(params);
    auto * obj = dynamic_cast<const common_chat_schema_object *>(doc.root.get());
    if (!obj) {
        return;
    }
    for (const auto & prop : obj->properties) {
        auto types = prop.schema->value_types();
        int64_t ty;
        if (types.is_only(common_chat_schema::TYPE_STRING)) {
            ty = 0;
        } else if (!types.has(common_chat_schema::TYPE_STRING)) {
            ty = 1;
        } else {
            ty = 2;
        }
        table.add(prop.name, ty);
    }
}

// ---- tools (copied from tests/test-chat.cpp:425-508) -----------------------

static common_chat_tool special_function_tool{
    "special_function",
    "I'm special",
    R"({
        "type": "object",
        "properties": {
            "arg1": {
                "type": "integer",
                "description": "The arg."
            }
        },
        "required": ["arg1"]
    })",
};

static common_chat_tool python_tool{
    "python",
    "an ipython interpreter",
    R"({
        "type": "object",
        "properties": {
            "code": {
                "type": "string",
                "description": "Python code to execute."
            }
        },
        "required": ["code"]
    })",
};

static common_chat_tool html_tool{
    "html",
    "an html validator",
    R"({
        "type": "object",
        "properties": {
            "markup": {
                "type": "string",
                "description": "HTML markup to validate."
            }
        },
        "required": ["markup"]
    })",
};

// ---- cases -----------------------------------------------------------------

enum Family {
    F_TRANSLATE = 0,
    F_FUNCTIONARY,
    F_GIGACHAT,
    F_MUSE_GLIMMER,
    F_MINICPM,
    F_HARMONY_XML,
    F_MUSE_SPARK,
    F_K2_HORIZON,
    F_LLM_JP,
};

struct Case {
    const char * name;
    Family       family;
    const char * template_file;  // nullptr -> the inline Harmony XML template
    const char * raw;
    int          tool;           // -1 none, 0 special, 1 python, 2 html
    bool         extract;        // reasoning_format = AUTO
    bool         enable_thinking;
    bool         parallel;
    int          call_format;    // K2: 0 xml, 1 xml_typed, 2 json
};

static const Case CASES[] = {
    { "translate-content", F_TRANSLATE, "google-translategemma-4b-it.jinja",
      "Hello, world!\nWhat's up?", -1, false, true, false, 0 },
    { "functionary-content", F_FUNCTIONARY, "meetkai-functionary-medium-v3.2.jinja",
      "all\nHello, world!\nWhat's up?", -1, false, true, false, 0 },
    { "functionary-tool", F_FUNCTIONARY, "meetkai-functionary-medium-v3.2.jinja",
      "special_function\n{\"arg1\": 1}", 0, false, true, false, 0 },
    { "gigachat-content-call", F_GIGACHAT, "GigaChat3-10B-A1.8B.jinja",
      "Hello, world!\nWhat's up?<|message_sep|>\n\nfunction call<|role_sep|>\n"
      "{\"name\": \"special_function\", \"arguments\": {\"arg1\": 1}}", 0, false, true, false, 0 },
    { "muse-glimmer-content", F_MUSE_GLIMMER, "muse-glimmer.jinja",
      " to=user<|message|>Hello, world!\nWhat's up?<|eot|>", -1, true, true, false, 0 },
    { "muse-glimmer-tool", F_MUSE_GLIMMER, "muse-glimmer.jinja",
      " to=special_function<|message|><atem:function_calls>\n"
      "<atem:invoke name=\"special_function\">\n"
      "<atem:parameter name=\"arg1\">1</atem:parameter>\n"
      "</atem:invoke>\n</atem:function_calls>", 0, true, true, false, 0 },
    { "muse-glimmer-reasoning", F_MUSE_GLIMMER, "muse-glimmer.jinja",
      " to=self<|message|>Thinking.<|eom|><|start|>assistant to=user<|message|>Hello!<|eot|>",
      -1, true, true, false, 0 },
    { "minicpm-content", F_MINICPM, "openbmb-MiniCPM5-1B.jinja",
      "Hello, world!\nWhat's up?", -1, true, false, false, 0 },
    { "minicpm-tool", F_MINICPM, "openbmb-MiniCPM5-1B.jinja",
      "<function name=\"python\"><param name=\"code\">print('Hello, World!')</param></function>",
      1, true, false, false, 0 },
    { "minicpm-cdata", F_MINICPM, "openbmb-MiniCPM5-1B.jinja",
      "<function name=\"html\"><param name=\"markup\"><![CDATA[<a href=\"/x\">hi</a> </param>]]></param></function>",
      2, true, false, false, 0 },
    { "harmony-xml-content", F_HARMONY_XML, nullptr,
      "<|channel|>final<|message|>Hello<|end|>", -1, false, true, false, 0 },
    { "harmony-xml-tool", F_HARMONY_XML, nullptr,
      " to=special_function<|channel|>analysis <|constrain|>xml<|message|>"
      "<rfl:parameter name=\"arg1\">\n1\n</rfl:parameter>\n<|end|>", 0, false, true, false, 0 },
    { "muse-spark-commentary", F_MUSE_SPARK, "muse-spark.jinja",
      " to=commentary<|message|>Hello, world!\nWhat's up?<|eom|>"
      "<|start|>assistant to=user<|message|>Done!<|eot|>", -1, true, true, false, 0 },
    { "muse-spark-tool", F_MUSE_SPARK, "muse-spark.jinja",
      " to=commentary<|message|>Let me look that up.<|eom|>"
      "<|start|>assistant to=special_function<|message|><atem:function_calls>\n"
      "<atem:invoke name=\"special_function\">\n"
      "<atem:parameter name=\"arg1\">1</atem:parameter>\n"
      "</atem:invoke>\n</atem:function_calls>", 0, true, true, false, 0 },
    { "k2-reasoning-content", F_K2_HORIZON, "IFM-K2-Horizon.jinja",
      "I'm\nthinking</ifm|think>Hello, world!\nWhat's up?", -1, true, true, false, 0 },
    { "k2-xml-tool", F_K2_HORIZON, "IFM-K2-Horizon.jinja",
      "I'm\nthinking</ifm|think><ifm|tool_calls>\n"
      "<ifm|tool_call>special_function\n"
      "<ifm|arg_key>arg1</ifm|arg_key>\n"
      "<ifm|arg_value>1</ifm|arg_value>\n"
      "</ifm|tool_call>\n</ifm|tool_calls>", 0, true, true, false, 0 },
    { "llm-jp-content", F_LLM_JP, "llm-jp-llm-jp-4.1-8b-thinking.jinja",
      "<|channel|> final<|message|> Hello, world!\nWhat's up?", -1, true, true, false, 0 },
    { "llm-jp-tool", F_LLM_JP, "llm-jp-llm-jp-4.1-8b-thinking.jinja",
      "<|channel|> analysis<|message|> I'm\nthinking<|end|>"
      "<|start|> assistant to=functions.special_function<|channel|> commentary "
      "<|constrain|>  json<|message|> {\"arg1\": 1}", 0, true, true, false, 0 },
};

static const char * INLINE_HARMONY =
    "{% for message in messages %}{{ message['role'] }}: {{ message['content'] }}\n"
    "{% endfor %}{% if add_generation_prompt %}<|start|>assistant{% endif %}"
    "{# Harmony XML detection markers: <|channel|> <|constrain|>xml rfl:parameter #}";

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

static const common_chat_tool & tool_by_id(int id) {
    if (id == 0) return special_function_tool;
    if (id == 1) return python_tool;
    return html_tool;
}

// ---- port calls ------------------------------------------------------------

static std::string call_port(const Case & c, const TypeTable & table, const std::string & raw) {
    std::vector<uint8_t> dst(65536, 0);
    std::string          raw_mut = raw;
    sl_u8  s        = sl(raw_mut);
    sl_u8  out      = sl_u8{ dst.data(), (int64_t) dst.size() };
    sl_u8  names    = sl_u8{ (uint8_t *) table.packed.data(), (int64_t) table.packed.size() };
    sl_i64 soff     = sl_i64{ (int64_t *) table.off.data(), (int64_t) table.off.size() };
    sl_i64 slen     = sl_i64{ (int64_t *) table.len.data(), (int64_t) table.len.size() };
    sl_i64 stype    = sl_i64{ (int64_t *) table.type.data(), (int64_t) table.type.size() };
    const int64_t nschema = (int64_t) table.type.size();

    int64_t n = -1;
    const int64_t hastools = c.tool >= 0 ? 1 : 0;
    const int64_t extract  = c.extract ? 1 : 0;
    const int64_t parallel = c.parallel ? 1 : 0;

    switch (c.family) {
        case F_TRANSLATE:
            n = d_translate_gemma(s, out);
            break;
        case F_FUNCTIONARY:
            n = d_functionary(s, hastools, parallel, out);
            break;
        case F_GIGACHAT:
            n = d_gigachat(s, hastools, out);
            break;
        case F_MUSE_GLIMMER:
            n = d_muse_glimmer(s, hastools, extract, names, soff, slen, stype, nschema, out);
            break;
        case F_MINICPM:
            n = d_minicpm5(s, hastools, extract, names, soff, slen, stype, nschema, out);
            break;
        case F_HARMONY_XML:
            n = d_harmony_xml(s, hastools, extract, names, soff, slen, stype, nschema, out);
            break;
        case F_MUSE_SPARK:
            n = d_muse_spark(s, hastools, extract, names, soff, slen, stype, nschema, out);
            break;
        case F_K2_HORIZON:
            n = d_k2_horizon(s, hastools, extract, c.call_format, parallel,
                             names, soff, slen, stype, nschema, out);
            break;
        case F_LLM_JP:
            n = d_llm_jp_harmony(s, hastools, extract, out);
            break;
    }
    if (n < 0 || n > (int64_t) dst.size()) {
        throw std::runtime_error("port returned " + std::to_string(n));
    }
    return std::string((const char *) dst.data(), (size_t) n);
}

// ---- comparison ------------------------------------------------------------

static std::string norm_args(const std::string & s) {
    try {
        return json::parse(s).dump();
    } catch (const std::exception &) {
        return s;
    }
}

static void check(const char * name, const char * field, const std::string & a, const std::string & b) {
    ++g_values;
    if (a != b) {
        bad(name, std::string(field) + ": native=[" + visible(a) + "] port=[" + visible(b) + "]");
    }
}

static void check_n(const char * name, const char * field, int64_t a, int64_t b) {
    ++g_values;
    if (a != b) {
        bad(name, std::string(field) + ": native=" + std::to_string(a) + " port=" + std::to_string(b));
    }
}

static void run_case(const Case & c, const std::string & llama_root) {
    ++g_cases;
    try {
        std::string src;
        if (c.template_file) {
            src = read_file(llama_root + "/models/templates/" + c.template_file);
        } else {
            src = INLINE_HARMONY;
        }

        common_chat_msg user;
        user.role    = "user";
        user.content = "Hey there!";

        common_chat_templates_inputs inputs;
        inputs.messages             = { user };
        inputs.add_generation_prompt = true;
        inputs.reasoning_format     = c.extract ? COMMON_REASONING_FORMAT_AUTO : COMMON_REASONING_FORMAT_NONE;
        inputs.enable_thinking      = c.enable_thinking;
        inputs.parallel_tool_calls  = c.parallel;
        if (c.tool >= 0) {
            inputs.tools = { tool_by_id(c.tool) };
        }

        auto tmpls = common_chat_templates_init(nullptr, src);
        if (!tmpls) {
            bad(c.name, "common_chat_templates_init returned null");
            return;
        }

        common_chat_session session(tmpls.get(), nullptr, inputs);
        const common_chat_msg nm = session.finish(common_chat_input(std::string(c.raw)));

        TypeTable table;
        if (c.tool >= 0) {
            try {
                fill_table(table, tool_by_id(c.tool));
            } catch (const std::exception & e) {
                bad(c.name, std::string("schema table failed: ") + e.what());
                return;
            }
        }

        std::string port_json;
        try {
            port_json = call_port(c, table, std::string(c.raw));
        } catch (const std::exception & e) {
            bad(c.name, std::string("port call threw: ") + e.what());
            return;
        }

        std::vector<common_chat_msg> port_msgs;
        try {
            port_msgs = common_chat_msgs_parse_oaicompat(json::parse("[" + port_json + "]"));
        } catch (const std::exception & e) {
            bad(c.name, std::string("port JSON rejected: ") + e.what() + " json=[" + visible(port_json) + "]");
            return;
        }
        if (port_msgs.size() != 1) {
            bad(c.name, "port JSON did not yield one message");
            return;
        }
        const common_chat_msg & port = port_msgs[0];

        check(c.name, "role", nm.role, port.role);
        check(c.name, "content", nm.content, port.content);
        check(c.name, "reasoning", nm.reasoning_content, port.reasoning_content);
        check_n(c.name, "ncalls", (int64_t) nm.tool_calls.size(), (int64_t) port.tool_calls.size());

        const size_t ncalls = std::max(nm.tool_calls.size(), port.tool_calls.size());
        for (size_t i = 0; i < ncalls; ++i) {
            const std::string idx = "[" + std::to_string(i) + "]";
            const common_chat_tool_call empty;
            const common_chat_tool_call & a = i < nm.tool_calls.size() ? nm.tool_calls[i] : empty;
            const common_chat_tool_call & b = i < port.tool_calls.size() ? port.tool_calls[i] : empty;
            check(c.name, ("name" + idx).c_str(), a.name, b.name);
            check(c.name, ("args" + idx).c_str(), norm_args(a.arguments), norm_args(b.arguments));
            check(c.name, ("id" + idx).c_str(), a.id, b.id);
        }
    } catch (const std::exception & e) {
        bad(c.name, std::string("native threw: ") + e.what());
    }
}

int main(int argc, char ** argv) {
    goose_init(0, nullptr);
    const std::string root = argc > 1 ? argv[1] : ".";
    for (const auto & c : CASES) {
        run_case(c, root);
    }
    printf("FG572_PARSERS cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
