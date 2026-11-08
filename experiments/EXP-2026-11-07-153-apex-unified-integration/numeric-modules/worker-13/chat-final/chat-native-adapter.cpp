// APX028 native patched-TU adapter.
//
// Converts the public common_chat_template / common_chat_templates_inputs to
// the canonical plan, runs the Goose control entry, and converts the result
// back to common_chat_params.  It never calls common_chat_templates_apply and
// never re-runs the native workaround/format/selection policy.
//
// The PEG autoparser and dedicated parser bodies are provided by a separate
// production effect object (chat-native-effects.cpp).  When present they hand
// back a fully built common_chat_params, including the PEG arena; when absent
// the canonical control still returns the prompt/format/reasoning decision.

#include "apx028-chat-native.h"

#include "chat-auto-parser.h"
#include "chat-peg-parser.h"
#include "chat-public.h"
#include "chat-schema.h"
#include "json.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

bool apx028_chat_enabled(void) {
    const char * env = std::getenv("APX028_CHAT");
    if (!env || !*env) {
        return true;
    }
    std::string v(env);
    if (v == "0" || v == "off" || v == "false" || v == "no") {
        return false;
    }
    return true;
}

static std::string get_str(int64_t plan, int64_t key) {
    const char * p = apx028_plan_result_str(plan, key);
    return p ? std::string(p) : std::string();
}

static void add_prompt_parts(const int64_t plan, common_chat_params & params) {
    std::string json_text = get_str(plan, RK_PROMPT_JSON);
    if (!json_text.empty()) {
        try {
            common_json arr = common_json::parse(json_text);
            if (arr.is_array()) {
                for (size_t i = 0; i < arr.size(); i++) {
                    const common_json & part = arr.at(i);
                    common_chat_prompt_part out;
                    out.text     = part.value("text", "");
                    out.is_input = part.value("input", false);
                    params.prompt.parts.push_back(std::move(out));
                }
            }
        } catch (...) {
        }
    }
    if (params.prompt.parts.empty()) {
        params.prompt.parts.push_back({get_str(plan, RK_PROMPT_TEXT), false});
    }
}

common_chat_params apx028_chat_apply_templates(
        const common_chat_template *         tmpl_default,
        const common_chat_template *         tmpl_tool_use,
        bool                                 add_bos,
        bool                                 add_eos,
        const common_chat_templates_inputs & inputs) {
    if (!tmpl_default) {
        throw std::invalid_argument("APX028: missing default chat template");
    }
    int64_t plan = apx028_plan_new();
    if (!plan) {
        throw std::runtime_error("APX028: cannot allocate canonical chat plan");
    }

    try {
        int64_t rc = apx028_plan_set_template(plan, 0, tmpl_default->source().c_str(),
                                              tmpl_default->bos_token().c_str(),
                                              tmpl_default->eos_token().c_str());
        if (rc < 0) {
            throw std::invalid_argument("APX028: default template parse failed");
        }
        if (tmpl_tool_use) {
            rc = apx028_plan_set_template(plan, 1, tmpl_tool_use->source().c_str(),
                                          tmpl_tool_use->bos_token().c_str(),
                                          tmpl_tool_use->eos_token().c_str());
            if (rc < 0) {
                throw std::invalid_argument("APX028: tool-use template parse failed");
            }
        }
        for (const auto & msg : inputs.messages) {
            std::string text = msg.to_json_oaicompat(false).dump();
            rc = apx028_plan_add_message_json(plan, text.c_str());
            if (rc < 0) {
                throw std::invalid_argument("APX028: message JSON conversion failed");
            }
        }
        if (!inputs.tools.empty()) {
            std::string tools = common_chat_tools_to_json_oaicompat(inputs.tools).dump();
            rc = apx028_plan_set_tools_json(plan, tools.c_str());
            if (rc < 0) {
                throw std::invalid_argument("APX028: tool JSON conversion failed");
            }
        }
        if (!inputs.grammar.empty()) {
            apx028_plan_set_grammar(plan, inputs.grammar.c_str());
        }
        if (!inputs.json_schema.empty()) {
            apx028_plan_set_json_schema(plan, inputs.json_schema.c_str());
        }
        for (const auto & kv : inputs.chat_template_kwargs) {
            rc = apx028_plan_add_kwarg(plan, kv.first.c_str(), kv.second.c_str());
            if (rc < 0) {
                throw std::invalid_argument("APX028: invalid chat_template_kwargs JSON value");
            }
        }

        int64_t flags = APX_F_USE_JINJA | APX_F_MARK_INPUT;
        if (add_bos) { flags |= APX_F_ADD_BOS; }
        if (add_eos) { flags |= APX_F_ADD_EOS; }
        if (inputs.add_generation_prompt) { flags |= APX_F_ADD_GEN; }
        if (inputs.force_pure_content) { flags |= APX_F_FORCE_PURE; }
        if (inputs.parallel_tool_calls) { flags |= APX_F_PARALLEL; }
        if (inputs.enable_thinking) { flags |= APX_F_ENABLE_THINKING; }

        apx028_plan_set_int(plan, PK_FLAGS, flags);
        apx028_plan_set_int(plan, PK_CONT, (int64_t) inputs.continue_final_message);
        apx028_plan_set_int(plan, PK_TOOL_CHOICE, (int64_t) inputs.tool_choice);
        apx028_plan_set_int(plan, PK_REASONING_FORMAT, (int64_t) inputs.reasoning_format);
        apx028_plan_set_int(plan, PK_ADD_BOS, add_bos ? 1 : 0);
        apx028_plan_set_int(plan, PK_ADD_EOS, add_eos ? 1 : 0);
        apx028_plan_set_int(plan, PK_ADD_GEN, inputs.add_generation_prompt ? 1 : 0);
        apx028_plan_set_int(plan, PK_FORCE_PURE, inputs.force_pure_content ? 1 : 0);
        apx028_plan_set_int(plan, PK_PARALLEL, inputs.parallel_tool_calls ? 1 : 0);
        apx028_plan_set_int(plan, PK_ENABLE_THINKING, inputs.enable_thinking ? 1 : 0);
        apx028_plan_set_int(plan, PK_MARK_INPUT, 1);

        if (tmpl_default->analysis) {
            const autoparser::autoparser & a = *tmpl_default->analysis;
            apx028_plan_set_analysis(plan, 1, (int64_t) a.reasoning.mode,
                                     a.reasoning.start.c_str(), a.reasoning.end.c_str());
            apx028_plan_set_boundaries(plan, a.assistant_start.c_str(), a.user_start.c_str());
        } else {
            apx028_plan_set_analysis(plan, 0, APX_REASON_NONE, "", "");
        }
        apx028_plan_set_native(plan, (int64_t) (intptr_t) tmpl_default,
                               (int64_t) (intptr_t) &inputs);

        int64_t applied = apx028_plan_apply(plan);
        if (applied < 0) {
            int64_t code = applied;
            std::string detail = applied == APX_ERR_NO_ANALYSIS
                ? tmpl_default->analysis_error
                : std::string();
            apx028_plan_free(plan);
            if (code == APX_ERR_GRAMMAR_TOOLS) {
                throw std::runtime_error("Cannot specify grammar with tools");
            }
            if (code == APX_ERR_NO_ANALYSIS) {
                throw std::invalid_argument(
                    "Unable to generate parser for this template. Automatic parser generation failed: " + detail);
            }
            throw std::runtime_error("APX028 chat control failed with code " + std::to_string(code));
        }

        int64_t native_params = apx028_plan_take_native_result(plan);
        if (native_params) {
            common_chat_params out = *(common_chat_params *) (intptr_t) native_params;
            delete (common_chat_params *) (intptr_t) native_params;
            apx028_plan_free(plan);
            return out;
        }

        common_chat_params params;
        params.format = (common_chat_format) apx028_plan_result_int(plan, RK_FORMAT);
        add_prompt_parts(plan, params);
        params.grammar              = get_str(plan, RK_GRAMMAR);
        params.generation_prompt    = get_str(plan, RK_GEN_PROMPT);
        params.supports_thinking    = apx028_plan_result_int(plan, RK_SUPPORTS_THINKING) != 0;
        params.thinking_start_tag   = get_str(plan, RK_THINKING_START);
        std::string thinking_end    = get_str(plan, RK_THINKING_END);
        if (!thinking_end.empty()) {
            params.thinking_end_tags.push_back(thinking_end);
        }
        {
            const char * assistant_start = apx028_plan_analysis_text(plan, 0);
            const char * user_start      = apx028_plan_analysis_text(plan, 1);
            if (assistant_start && *assistant_start) {
                params.message_delimiters.add(COMMON_CHAT_ROLE_ASSISTANT, assistant_start);
            }
            if (user_start && *user_start) {
                params.message_delimiters.add(COMMON_CHAT_ROLE_USER, user_start);
            }
        }
        if (inputs.force_pure_content) {
            const std::string gp = params.generation_prompt;
            params.parser = build_chat_peg_parser([&gp](common_chat_peg_builder & p) {
                return p.literal(gp) << p.content(p.rest());
            });
        }
        apx028_plan_free(plan);
        return params;
    } catch (...) {
        apx028_plan_free(plan);
        throw;
    }
}
