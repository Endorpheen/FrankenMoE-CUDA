// Differential probe for the legacy (use_jinja=false) chat-template path.
// Links the real implementation: src/llama-chat.cpp for llm_chat_detect_template
// and llm_chat_apply_template. No model or weights are needed.
//
// Build: see build.sh in this directory.
#include "llama.h"
#include "llama-chat.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "templates.h"

struct msg { const char * role; const char * content; };

static const char * enum_name(llm_chat_template t) {
#define N(x) case LLM_CHAT_TEMPLATE_##x: return #x;
    switch (t) {
        N(CHATML) N(LLAMA_2) N(LLAMA_2_SYS) N(LLAMA_2_SYS_BOS) N(LLAMA_2_SYS_STRIP)
        N(MISTRAL_V1) N(MISTRAL_V3) N(MISTRAL_V3_TEKKEN) N(MISTRAL_V7) N(MISTRAL_V7_TEKKEN)
        N(PHI_3) N(PHI_4) N(FALCON_3) N(ZEPHYR) N(MONARCH) N(GEMMA) N(ORION) N(OPENCHAT)
        N(VICUNA) N(VICUNA_ORCA) N(DEEPSEEK) N(DEEPSEEK_2) N(DEEPSEEK_3) N(DEEPSEEK_OCR)
        N(COMMAND_R) N(LLAMA_3) N(CHATGLM_3) N(CHATGLM_4) N(GLMEDGE) N(MINICPM)
        N(EXAONE_3) N(EXAONE_4) N(EXAONE_MOE) N(RWKV_WORLD) N(GRANITE_3_X)
        N(GRANITE_4_0) N(GRANITE_4_1) N(GIGACHAT) N(MEGREZ) N(YANDEX) N(BAILING)
        N(BAILING_THINK) N(BAILING2) N(LLAMA4) N(SMOLVLM) N(DOTS1) N(HUNYUAN_MOE)
        N(OPENAI_MOE) N(HUNYUAN_DENSE) N(HUNYUAN_VL) N(KIMI_K2) N(SEED_OSS) N(GROK_2)
        N(PANGU_EMBED) N(SOLAR_OPEN)
        default: return "UNKNOWN";
    }
#undef N
}

// Escapes the string so whitespace and markers are visible in a terminal.
static std::string esc(const std::string & s) {
    std::string out;
    for (unsigned char c : s) {
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

// Same body as llama_chat_apply_template() in src/llama.cpp, which is a thin
// wrapper over the two functions above; kept here so the probe needs no model.
static int32_t apply(const char * tmpl, const std::vector<msg> & chat, bool add_ass, std::string & out) {
    const std::string curr_tmpl(tmpl == nullptr ? "chatml" : tmpl);
    std::vector<const llama_chat_message *> chat_vec;
    std::vector<llama_chat_message> storage;
    storage.reserve(chat.size());
    for (const auto & m : chat) storage.push_back({ m.role, m.content });
    for (const auto & m : storage) chat_vec.push_back(&m);

    llm_chat_template detected = llm_chat_detect_template(curr_tmpl);
    if (detected == LLM_CHAT_TEMPLATE_UNKNOWN) return -1;
    int32_t res = llm_chat_apply_template(detected, chat_vec, out, add_ass);
    return res;
}

static void run(const char * label, const char * tmpl, const std::vector<msg> & chat, bool add_ass, bool full) {
    std::string out;
    int32_t res = apply(tmpl, chat, add_ass, out);
    printf("### %s\n", label);
    printf("template=%s\n", esc(tmpl).c_str());
    printf("detected=%s add_ass=%d res=%d len=%zu\n", enum_name(llm_chat_detect_template(std::string(tmpl))),
           (int) add_ass, res, out.size());
    if (res < 0) {
        printf("prompt=<UNSUPPORTED>\n\n");
        return;
    }
    if (full) {
        printf("prompt=%s\n\n", esc(out).c_str());
    } else {
        printf("prefix=%s\n", esc(out.substr(0, 120)).c_str());
        printf("suffix=%s\n\n", esc(out.size() > 120 ? out.substr(out.size() - 80) : "").c_str());
    }
}

static void hexout(const std::string & s) {
    for (unsigned char c : s) printf("%02x", c);
    printf("\n");
}

struct tcase { const char * label; const char * tmpl; std::vector<msg> chat; bool add_ass; };

static std::vector<tcase> case_list() {
    std::vector<msg> conv = {
        { "system",    "You are a helpful assistant." },
        { "user",      "Hello" },
        { "assistant", "Hi there" },
        { "user",      "Who are you?" },
    };
    std::vector<msg> tool = {
        { "system", "S" }, { "user", "U" }, { "assistant", "A" },
        { "tool", "T" }, { "function", "F" }, { "assistant_tool_call", "C" },
    };
    return {
        { "case-01-chatml", "chatml", conv, true },
        { "case-02-llama3", "llama3", conv, true },
        { "case-03-gemma", "gemma", conv, true },
        { "case-04-llama2-sys", "llama2-sys", conv, true },
        { "case-05-mistral-v1", "mistral-v1", conv, true },
        { "case-06-phi3", "phi3", conv, true },
        { "case-07-deepseek3", "deepseek3", conv, true },
        { "case-08-vicuna-orca", "vicuna-orca", conv, true },
        { "case-09-chatml-noass", "chatml", conv, false },
        { "case-10-yandex", "yandex", conv, true },
        { "case-11-tools-chatml", "chatml", tool, false },
        { "case-12-tools-granite", "granite", tool, false },
        { "case-13-tools-kimi", "kimi-k2", tool, false },
        { "case-14-empty", "chatml", {}, true },
        { "case-15-pangu", "pangu-embedded", { { "user", "Hello" }, { "assistant", "Hi there" } }, true },
        { "case-16-chatglm3", "chatglm3", { { "system", "Be brief." }, { "user", "Hello" } }, true },
        { "case-17-openchat", "openchat", conv, true },
        { "case-18-gigachat", "gigachat", { { "system", "S" }, { "user", "U" }, { "assistant", "A" }, { "user", "U2" } }, true },
        { "case-19-trim", "llama3", { { "user", "  padded  " } }, true },
        { "case-20-parts-chatml", "chatml", { { "user", "C" } }, false },
        // One fixture per formerly missing format. Names are the legacy builtin
        // names; DOTS1 has no name and is reached by its source marker.
        { "case-21-deepseek-ocr", "deepseek-ocr", { { "system", "S" }, { "user", "U" }, { "assistant", "A" } }, false },
        { "case-22-chatglm4", "chatglm4", { { "system", "S" }, { "user", "U" } }, true },
        { "case-23-minicpm", "minicpm", { { "system", "S" }, { "user", " U " }, { "assistant", " A " } }, false },
        { "case-24-exaone3", "exaone3", { { "system", "S" }, { "user", "U" }, { "assistant", "A" } }, true },
        { "case-25-exaone4", "exaone4", { { "system", "S" }, { "user", "U" }, { "tool", "T" }, { "assistant", "A" } }, true },
        { "case-26-exaone-moe", "exaone-moe", { { "system", "S" }, { "user", "U" }, { "tool", "T" }, { "assistant", "A" } }, true },
        { "case-27-megrez", "megrez", { { "system", "S" }, { "user", "U" }, { "assistant", "A" } }, true },
        { "case-28-dots1", "{{ '<|userprompt|>' }}<|endofuserprompt|>", { { "system", "S" }, { "user", "U" }, { "assistant", "A" } }, true },
        { "case-29-seed-oss", "seed_oss", { { "user", "U" }, { "assistant", "  A  " }, { "user", "U2" } }, true },
        { "case-30-rwkv-world", "rwkv-world", { { "user", "U" }, { "assistant", "A" }, { "user", "U2" } }, true },
        { "case-31-smolvlm", "smolvlm", { { "system", "S" }, { "user", "U" }, { "assistant", "A" } }, true },
        { "case-32-bailing", "bailing", { { "system", "S" }, { "user", "U" }, { "assistant", "A" } }, true },
        { "case-33-bailing-think", "bailing-think", { { "user", "U" }, { "assistant", "A" } }, true },
        { "case-34-bailing2", "bailing2", { { "user", "U" }, { "assistant", "A" } }, true },
        { "case-35-hunyuan-dense", "hunyuan-dense", { { "system", "S" }, { "user", "U" }, { "assistant", "A" } }, false },
        { "case-36-hunyuan-vl", "hunyuan-vl", { { "system", "S" }, { "user", "U" }, { "assistant", "A" } }, false },
        { "case-37-hunyuan-moe", "hunyuan-moe", { { "system", "S" }, { "user", "U" }, { "assistant", "A" } }, false },
        { "case-38-grok-2", "grok-2", { { "system", "S" }, { "user", " U " }, { "assistant", "A" } }, true },
        { "case-39-gpt-oss", "gpt-oss", { { "user", "U" }, { "assistant", "A" }, { "user", "U2" } }, true },
    };
}

static int enum_id(const char * name) {
    static const char * all[] = {"CHATML","LLAMA_2","LLAMA_2_SYS","LLAMA_2_SYS_BOS","LLAMA_2_SYS_STRIP","MISTRAL_V1","MISTRAL_V3","MISTRAL_V3_TEKKEN","MISTRAL_V7","MISTRAL_V7_TEKKEN","PHI_3","PHI_4","FALCON_3","ZEPHYR","MONARCH","GEMMA","ORION","OPENCHAT","VICUNA","VICUNA_ORCA","DEEPSEEK","DEEPSEEK_2","DEEPSEEK_3","DEEPSEEK_OCR","COMMAND_R","LLAMA_3","CHATGLM_3","CHATGLM_4","GLMEDGE","MINICPM","EXAONE_3","EXAONE_4","EXAONE_MOE","RWKV_WORLD","GRANITE_3_X","GRANITE_4_0","GRANITE_4_1","GIGACHAT","MEGREZ","YANDEX","BAILING","BAILING_THINK","BAILING2","LLAMA4","SMOLVLM","DOTS1","HUNYUAN_MOE","OPENAI_MOE","HUNYUAN_DENSE","HUNYUAN_VL","KIMI_K2","SEED_OSS","GROK_2","PANGU_EMBED","SOLAR_OPEN"};
    for (int i = 0; i < 55; i++) if (strcmp(all[i], name) == 0) return i;
    return 55;
}

static int names_tsv() {
    const char * nm[] = {"chatml","llama2","llama2-sys","llama2-sys-bos","llama2-sys-strip","mistral-v1","mistral-v3","mistral-v3-tekken","mistral-v7","mistral-v7-tekken","phi3","phi4","falcon3","zephyr","monarch","gemma","orion","openchat","vicuna","vicuna-orca","deepseek","deepseek2","deepseek3","deepseek-ocr","command-r","llama3","chatglm3","chatglm4","glmedge","minicpm","exaone3","exaone4","exaone-moe","rwkv-world","granite","granite-4.0","granite-4.1","gigachat","megrez","yandex","bailing","bailing-think","bailing2","llama4","smolvlm","hunyuan-moe","gpt-oss","hunyuan-dense","hunyuan-vl","kimi-k2","seed_oss","grok-2","pangu-embedded","solar-open","{{ '<|userprompt|>' }}<|endofuserprompt|>"};
    for (const char * s : nm) {
        printf("det\t%s\t%d\n", s, enum_id(enum_name(llm_chat_detect_template(s))));
    }
    return 0;
}

static int cases_tsv() {
    for (const auto & c : case_list()) {
        std::string out;
        int32_t res = apply(c.tmpl, c.chat, c.add_ass, out);
        printf("%s\t%s\t%d\t", c.label, enum_name(llm_chat_detect_template(c.tmpl)), res);
        hexout(res >= 0 ? out : std::string());
    }
    return 0;
}

// Same fixture shape as host.c, for an external diff of the port against native.
static int tsv_mode() {
    std::vector<msg> conv = {
        { "system",    "You are a helpful assistant." },
        { "user",      "Hello" },
        { "assistant", "Hi there" },
        { "user",      "Who are you?" },
    };
    for (int i = 0; i < TPL_N; i++) {
        std::string out;
        int32_t res = apply(TPL_SRC[i], conv, true, out);
        printf("%s\t%s\t%d\t", TPL_NAME[i], enum_name(llm_chat_detect_template(TPL_SRC[i])), res);
        hexout(res >= 0 ? out : std::string());
    }
    std::string out;
    int32_t res = apply(TPL_SRC[4], conv, false, out);
    printf("noass\t%s\t%d\t", enum_name(llm_chat_detect_template(TPL_SRC[4])), res);
    hexout(res >= 0 ? out : std::string());
    return 0;
}

int main(int argc, char ** argv) {
    if (argc > 1 && strcmp(argv[1], "--tsv") == 0) return tsv_mode();
    if (argc > 1 && strcmp(argv[1], "--cases-tsv") == 0) return cases_tsv();
    if (argc > 1 && strcmp(argv[1], "--names-tsv") == 0) return names_tsv();
    const bool full = !(argc > 1 && strcmp(argv[1], "--snippet") == 0);

    std::vector<msg> conv = {
        { "system",    "You are a helpful assistant." },
        { "user",      "Hello" },
        { "assistant", "Hi there" },
        { "user",      "Who are you?" },
    };

    // 1. Detection on real template sources and on builtin names.
    struct det { const char * name; const char * src; };
    const std::vector<det> dets = {
        { "chatml",        "<|im_start|>system\n{{ message['content'] }}<|im_end|>\n" },
        { "llama3",        "<|start_header_id|>{{ message['role'] }}<|end_header_id|>\n\n" },
        { "gemma",         "<start_of_turn>{{ message['role'] }}\n" },
        { "mistral-v1",    "{{ bos_token }}{% for message in messages %}{{ ' [INST] ' + message['content'] + ' [/INST]' }}{% endfor %}" },
        { "mistral-v3",    "{% for message in messages %}{{ '[INST]' + message['content'] }}{% endfor %}{# [AVAILABLE_TOOLS] #}" },
        { "llama2-sys",    "{% if message['role'] == 'system' %}<<SYS>>{% endif %}" },
        { "llama2-sys-bos","{{ bos_token + '[INST] ' }}" },
        { "phi3",          "<|assistant|>x<|end|>" },
        { "deepseek3",     "<|User|>\n<|Assistant|>\n<|end of sentence|>" },
        { "solar-open",    "<|begin|>user<|content|>x<|end|>" },
        { "qwen-like",     "<|im_start|>user\nx<|im_end|>\n" },
        { "unknown",       "{{ messages }}" },
    };
    printf("== detection ==\n");
    for (const auto & d : dets) {
        printf("%-14s -> %s\n", d.name, enum_name(llm_chat_detect_template(d.src)));
    }
    printf("\n");

    // 1b. Detection on 31 real template sources extracted from the upstream test.
    printf("== real templates ==\n");
    for (int i = 0; i < TPL_N; i++) {
        printf("%-55.55s -> %s\n", TPL_NAME[i], enum_name(llm_chat_detect_template(TPL_SRC[i])));
    }
    printf("\n");

    // 2. Builtin-name route: the legacy path accepts the names from
    // llama_chat_builtin_templates() directly as the `tmpl` string.
    printf("== builtin name route ==\n");
    for (const char * n : { "chatml", "llama2", "llama2-sys", "llama3", "gemma", "mistral-v1",
                            "mistral-v3", "phi3", "deepseek3", "command-r", "solar-open", "gpt-oss" }) {
        run(n, n, conv, true, false);
    }

    // 3. Same messages, add_generation_prompt on/off, for the cases used in cases.md.
    struct c { const char * name; const char * tmpl; bool add_ass; };
    const std::vector<c> cases = {
        { "case-01 chatml",        "chatml",    true  },
        { "case-02 llama3",        "llama3",    true  },
        { "case-03 gemma",         "gemma",     true  },
        { "case-04 llama2-sys",    "llama2-sys",true  },
        { "case-05 mistral-v1",    "mistral-v1",true  },
        { "case-06 phi3",          "phi3",      true  },
        { "case-07 deepseek3",     "deepseek3", true  },
        { "case-08 vicuna-orca",   "vicuna-orca", true },
        { "case-09 chatml-noass",  "chatml",    false },
        { "case-10 yandex",        "yandex",    true  },
    };
    printf("== cases ==\n");
    for (const auto & t : cases) run(t.name, t.tmpl, conv, t.add_ass, full);

    // 4. Role coverage of one plain formatter (both roles and tool).
    std::vector<msg> roles = {
        { "system", "S" }, { "user", "U" }, { "assistant", "A" },
        { "tool", "T" }, { "function", "F" }, { "assistant_tool_call", "C" },
    };
    run("roles/tool", "chatml", roles, false, true);
    run("roles/tool/granite", "granite", roles, false, true);
    run("roles/tool/kimi-k2", "kimi-k2", roles, false, true);

    // 5. Empty chat and add_ass.
    std::vector<msg> empty;
    run("empty", "chatml", empty, true, true);
    return 0;
}
