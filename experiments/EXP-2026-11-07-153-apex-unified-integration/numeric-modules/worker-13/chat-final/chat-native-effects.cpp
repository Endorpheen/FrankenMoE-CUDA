// APX028 native parser-engine effects.
//
// These are the only remaining native parser bodies: the dedicated handlers in
// inputs/native/common/parsers/*.cpp and the differential PEG autoparser in
// chat-auto-parser*.cpp / chat-peg-parser.cpp / peg-parser.cpp.  The Goose
// control (chat-control.goose) selects the handler, checks the tools/grammar
// policy and dispatches reasoning/tags; it never calls the whole
// common_chat_templates_apply.
//
// The effect receives the already-prepared canonical plan (messages after
// Goose workarounds) and returns a fully built common_chat_params through
// apx028_plan_set_native_result.  Production links this file with the native
// parser objects; the isolated local check only syntax-checks it.

#include "chat-public.h"
#include "chat-schema.h"

#include "chat.h"
#include "chat-auto-parser.h"
#include "chat-peg-parser.h"
#include "common.h"
#include "json.h"
#include "parsers/parsers.h"

#include <new>
#include <stdexcept>
#include <string>
#include <vector>

static common_json parse_opt(const char * text, bool want_object) {
    if (!text || !*text) {
        return common_json();
    }
    common_json v = common_json::parse(text);
    if (want_object && !v.is_object()) {
        return common_json();
    }
    return v;
}

static autoparser::generation_params build_params(int64_t plan) {
    autoparser::generation_params p;

    // The Goose dispatch calls a parser effect after message preparation but
    // before the final commit; materialize the prepared JSON view here.
    (void) apx028_plan_store_messages_json(plan);

    const char * messages = apx028_plan_messages_json(plan);
    p.messages = messages && *messages ? common_json::parse(messages) : common_json::array();

    if (apx028_plan_tools_count(plan) > 0) {
        p.tools = common_json::parse(apx028_plan_tools_json(plan));
    }
    p.tool_choice = (common_chat_tool_choice) apx028_plan_get_int(plan, PK_TOOL_CHOICE);
    p.reasoning_format = (common_reasoning_format) apx028_plan_get_int(plan, PK_REASONING_FORMAT);
    p.parallel_tool_calls = apx028_plan_get_int(plan, PK_PARALLEL) != 0;
    p.enable_thinking = apx028_plan_get_int(plan, PK_ENABLE_THINKING) != 0;
    p.add_generation_prompt = apx028_plan_get_int(plan, PK_ADD_GEN) != 0;
    p.add_bos = apx028_plan_get_int(plan, PK_ADD_BOS) != 0;
    p.add_eos = apx028_plan_get_int(plan, PK_ADD_EOS) != 0;
    p.continue_final_message = (common_chat_continuation) apx028_plan_get_int(plan, PK_CONT);
    p.mark_input = true;

    const char * grammar = apx028_plan_grammar_text(plan);
    if (grammar && *grammar) {
        p.grammar = grammar;
    }
    const char * schema = apx028_plan_json_schema_text(plan);
    if (schema && *schema) {
        p.json_schema = common_json::parse(schema);
    }
    p.extra_context = parse_opt(apx028_plan_extra_context_json(plan), true);

    const char * cont = apx028_plan_continuation_json(plan);
    if (cont && *cont) {
        common_json arr = common_json::parse(cont);
        if (!arr.is_array()) {
            common_json one = common_json::array();
            one.push_back(arr);
            arr = one;
        }
        std::vector<common_chat_msg> msgs = common_chat_msgs_parse_oaicompat(arr);
        if (!msgs.empty()) {
            p.continue_msg = msgs.front();
        }
    }

    const int64_t native_inputs = apx028_plan_get_native(plan, 1);
    if (native_inputs) {
        const common_chat_templates_inputs * inputs =
            (const common_chat_templates_inputs *) (intptr_t) native_inputs;
        p.now = inputs->now;
    }
    return p;
}

static common_chat_params run_specialized(int64_t plan, int64_t handler) {
    const int64_t native_tmpl = apx028_plan_get_native(plan, 0);
    if (!native_tmpl) {
        throw std::runtime_error("APX028: specialized handler has no native template");
    }
    const common_chat_template & tmpl = *(const common_chat_template *) (intptr_t) native_tmpl;
    autoparser::generation_params p = build_params(plan);

    switch (handler) {
        case APX_H_MISTRAL3:        return common_chat_params_init_ministral_3(tmpl, p);
        case APX_H_LLM_JP:          return common_chat_params_init_llm_jp_harmony(tmpl, p);
        case APX_H_HARMONY_XML:     return common_chat_params_init_harmony_xml(tmpl, p);
        case APX_H_GPT_OSS:         return common_chat_params_init_gpt_oss(tmpl, p);
        case APX_H_MUSE_SPARK:      return common_chat_params_init_muse_spark(tmpl, p);
        case APX_H_MUSE_GLIMMER:    return common_chat_params_init_muse_glimmer(tmpl, p);
        case APX_H_FUNCTIONARY:     return common_chat_params_init_functionary_v3_2(tmpl, p);
        case APX_H_KIMI_K2:         return common_chat_params_init_kimi_k2(tmpl, p);
        case APX_H_KIMI_K3:         return common_chat_params_init_kimi_k3(tmpl, p);
        case APX_H_K2_HORIZON:      return common_chat_params_init_k2_horizon(tmpl, p);
        case APX_H_LING3:           return common_chat_params_init_ling3(tmpl, p);
        case APX_H_COHERE2MOE:      return common_chat_params_init_cohere2moe(tmpl, p);
        case APX_H_LFM2:            return common_chat_params_init_lfm2(tmpl, p, true);
        case APX_H_LFM2_5:          return common_chat_params_init_lfm2(tmpl, p, false);
        case APX_H_GIGACHAT:        return common_chat_params_init_gigachat_v3(tmpl, p);
        case APX_H_MINIMAX_M3:      return common_chat_params_init_minimax_m3(tmpl, p);
        case APX_H_DEEPSEEK:        return common_chat_params_init_deepseek_v3_2(tmpl, p);
        case APX_H_GEMMA4_LEGACY: {
            autoparser::generation_params adjusted = p;
            workaround::convert_tool_responses_gemma4(adjusted.messages);
            return common_chat_params_init_gemma4(tmpl, adjusted);
        }
        case APX_H_GEMMA4:          return common_chat_params_init_gemma4(tmpl, p);
        case APX_H_MINICPM5:        return common_chat_params_init_minicpm5(tmpl, p);
        case APX_H_TRANSLATE_GEMMA: return common_chat_params_init_translate_gemma(tmpl, p);
        case APX_H_QWEN3_CODER:     return common_chat_params_init_qwen3_coder(tmpl, p);
        default:
            throw std::runtime_error("APX028: unknown specialized handler id");
    }
}

static common_chat_params run_autoparser(int64_t plan) {
    const int64_t native_tmpl = apx028_plan_get_native(plan, 0);
    if (!native_tmpl) {
        throw std::runtime_error("APX028: autoparser has no native template");
    }
    const common_chat_template & tmpl = *(const common_chat_template *) (intptr_t) native_tmpl;
    if (!tmpl.analysis) {
        throw std::invalid_argument("APX028: autoparser analysis is missing");
    }
    autoparser::generation_params p = build_params(plan);
    const autoparser::autoparser & a = *tmpl.analysis;
    common_chat_params out = autoparser::peg_generator::generate_parser(tmpl, p, a);

    if (!a.assistant_start.empty()) {
        out.message_delimiters.add(COMMON_CHAT_ROLE_ASSISTANT, a.assistant_start);
    }
    if (!a.user_start.empty()) {
        out.message_delimiters.add(COMMON_CHAT_ROLE_USER, a.user_start);
    }
    out.supports_thinking = a.reasoning.mode != autoparser::reasoning_mode::NONE;
    if (out.supports_thinking) {
        out.thinking_start_tag = trim_whitespace(a.reasoning.start);
        std::string end_tag = trim_whitespace(a.reasoning.end);
        if (!end_tag.empty()) {
            out.thinking_end_tags = { std::move(end_tag) };
        }
    }
    return out;
}

extern "C" int64_t apx028_native_specialized_apply(int64_t plan, int64_t handler) {
    try {
        common_chat_params out = run_specialized(plan, handler);
        apx028_plan_set_native_result(plan, (int64_t) (intptr_t) new common_chat_params(std::move(out)));
        return APX_OK;
    } catch (...) {
        return APX_ERR_INVAL;
    }
}

extern "C" int64_t apx028_native_autoparser_generate(int64_t plan) {
    try {
        common_chat_params out = run_autoparser(plan);
        apx028_plan_set_native_result(plan, (int64_t) (intptr_t) new common_chat_params(std::move(out)));
        return APX_OK;
    } catch (...) {
        return APX_ERR_NO_ANALYSIS;
    }
}
