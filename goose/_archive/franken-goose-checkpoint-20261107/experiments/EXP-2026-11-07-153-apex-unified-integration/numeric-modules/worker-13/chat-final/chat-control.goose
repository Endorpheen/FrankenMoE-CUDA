// APX028 chat/Jinja control in Goose.
//
// Owned here (Goose branches and loops):
//   * default vs tool-use template selection;
//   * continue_final_message resolution (AUTO -> REASONING/CONTENT);
//   * message workaround preparation and its exact order;
//   * tools/grammar conflict policy and force-pure-content dispatch;
//   * the dedicated-handler detection chain copied from
//     common_chat_template_detect_params_init;
//   * reasoning/thinking tag dispatch and format selection.
//
// External named effects only: minja lexer/parser/runtime/caps/render, JSON
// parse/dump, canonical raw memory, and the native PEG autoparser / dedicated
// parser bodies.  No whole common_chat_templates_apply native delegate is
// called from this path.

extern "cf_fgc_record_new" fn rec_new(kind:i64)->i64;
extern "cf_fgc_record_get" fn rec_get(h:i64,key:i64)->i64;
extern "cf_fgc_record_set" fn rec_set(h:i64,key:i64,v:i64)->i64;
extern "cf_fgc_vec_len" fn vec_len(h:i64)->i64;
extern "cf_fgc_str_len" fn str_len_h(h:i64)->i64;

extern "apx028_plan_get" fn plan_get(p:i64,k:i64)->i64;
extern "apx028_plan_set" fn plan_set(p:i64,k:i64,v:i64);
extern "apx028_plan_template" fn plan_template(p:i64,which:i64)->i64;
extern "apx028_plan_msgs" fn plan_msgs(p:i64)->i64;
extern "apx028_plan_msgs_count" fn plan_msgs_count(p:i64)->i64;
extern "apx028_plan_cont_msgs_count" fn plan_cont_msgs_count(p:i64)->i64;
extern "apx028_plan_tools_count" fn plan_tools_count(p:i64)->i64;
extern "apx028_plan_grammar_len" fn plan_grammar_len(p:i64)->i64;
extern "apx028_plan_caps" fn plan_caps(p:i64,t:i64)->i64;
extern "apx028_plan_extra_context" fn plan_extra_context(p:i64)->i64;
extern "apx028_plan_parse_json_schema" fn plan_parse_json_schema(p:i64)->i64;
extern "apx028_plan_pop_continuation" fn plan_pop_continuation(p:i64)->i64;
extern "apx028_plan_cont_reasoning_len" fn cont_reasoning_len(p:i64)->i64;
extern "apx028_plan_cont_content_len" fn cont_content_len(p:i64)->i64;
extern "apx028_plan_cont_parts_len" fn cont_parts_len(p:i64)->i64;
extern "apx028_plan_render_pure" fn plan_render_pure(p:i64)->i64;
extern "apx028_plan_store_messages_json" fn plan_store_messages_json(p:i64)->i64;
extern "apx028_plan_analysis_ready" fn plan_analysis_ready(p:i64)->i64;
extern "apx028_plan_reason_mode" fn plan_reason_mode(p:i64)->i64;
extern "apx028_plan_analysis_str" fn plan_analysis_str(p:i64,which:i64)->i64;
extern "apx028_plan_add_delimiter" fn plan_add_delimiter(p:i64,role:i64,s:i64)->i64;
extern "apx028_plan_clear_result" fn plan_clear_result(p:i64);

extern "apx028_tmpl_has" fn tmpl_has(t:i64,needle:i64)->i64;
extern "apx028_tmpl_caps" fn tmpl_caps(t:i64)->i64;
extern "apx028_tmpl_src_flags" fn tmpl_src_flags(t:i64)->i64;

extern "apx028_msg_at" fn msg_at(m:i64,i:i64)->i64;
extern "apx028_msg_role" fn msg_role(m:i64)->i64;
extern "apx028_msg_set_role" fn msg_set_role(m:i64,role:i64);
extern "apx028_msg_content_len" fn msg_content_len(m:i64)->i64;
extern "apx028_msg_reasoning_len" fn msg_reasoning_len(m:i64)->i64;
extern "apx028_msg_parts_len" fn msg_parts_len(m:i64)->i64;
extern "apx028_msg_has_tool_calls" fn msg_has_tool_calls(m:i64)->i64;
extern "apx028_msg_has_content" fn msg_has_content(m:i64)->i64;
extern "apx028_msg_set_empty_content" fn msg_set_empty_content(m:i64);
extern "apx028_msgs_merge_content" fn msgs_merge_content(m:i64,dst:i64,src:i64)->i64;
extern "apx028_msgs_erase_first" fn msgs_erase_first(m:i64)->i64;
extern "apx028_msg_parse_args" fn msg_parse_args(m:i64)->i64;
extern "apx028_msg_trim" fn msg_trim(m:i64)->i64;

extern "apx028_res_get" fn res_get(p:i64,k:i64)->i64;
extern "apx028_res_set" fn res_set(p:i64,k:i64,v:i64);
extern "apx028_res_str_copy" fn res_str_copy(p:i64,k:i64,s:i64)->i64;
extern "apx028_res_str_take" fn res_str_take(p:i64,k:i64,s:i64)->i64;
extern "apx028_res_set_text" fn res_set_text(p:i64,k:i64,t:i64)->i64;
extern "apx028_str_trim_dup" fn str_trim_dup(s:i64)->i64;
extern "apx028_str_len" fn str_len(s:i64)->i64;
extern "apx028_str_free" fn str_free(s:i64);

extern "apx028_native_specialized_apply" fn native_specialized(p:i64,h:i64)->i64;
extern "apx028_native_autoparser_generate" fn native_autoparser(p:i64)->i64;

// plan keys
fn pk_cont()->i64 {return 8;}
fn pk_tool_choice()->i64 {return 9;}
fn pk_selected()->i64 {return 13;}
fn pk_selected_which()->i64 {return 14;}
fn pk_format()->i64 {return 17;}
fn pk_handler()->i64 {return 18;}
fn pk_add_gen()->i64 {return 23;}
fn pk_error()->i64 {return 24;}
fn pk_force_pure()->i64 {return 25;}
fn pk_caps_override()->i64 {return 29;}

// result keys
fn rk_format()->i64 {return 0;}
fn rk_selected()->i64 {return 1;}
fn rk_handler()->i64 {return 2;}
fn rk_cont()->i64 {return 3;}
fn rk_add_gen()->i64 {return 4;}
fn rk_supports_thinking()->i64 {return 5;}
fn rk_thinking_start()->i64 {return 6;}
fn rk_thinking_end()->i64 {return 7;}
fn rk_gen_prompt()->i64 {return 9;}
fn rk_prompt_json()->i64 {return 10;}
fn rk_messages_json()->i64 {return 11;}
fn rk_error()->i64 {return 12;}
fn rk_analysis()->i64 {return 13;}

// caps bits
fn cap_tool_calls()->i64 {return 2;}
fn cap_system()->i64 {return 4;}
fn cap_object_args()->i64 {return 64;}

// roles / continuations / choices / formats / reasoning
fn role_system()->i64 {return 1;}
fn role_developer()->i64 {return 5;}
fn cont_none()->i64 {return 0;}
fn cont_auto()->i64 {return 1;}
fn cont_reasoning()->i64 {return 2;}
fn cont_content()->i64 {return 3;}
fn tool_none()->i64 {return 2;}
fn fmt_peg_native()->i64 {return 2;}
fn reason_tag()->i64 {return 1;}
fn reason_tools_only()->i64 {return 2;}

// status
fn err_grammar_tools()->i64 {return -2;}
fn err_no_analysis()->i64 {return -3;}
fn err_args_json()->i64 {return -4;}
fn err_state()->i64 {return -7;}

// needle ids
fn n_ministral_system()->i64 {return 1;}
fn n_ministral_calls()->i64 {return 2;}
fn n_ministral_args()->i64 {return 3;}
fn n_ministral_call_id()->i64 {return 4;}
fn n_llm_jp()->i64 {return 5;}
fn n_channel()->i64 {return 6;}
fn n_constrain_xml()->i64 {return 7;}
fn n_rfl()->i64 {return 8;}
fn n_muse_commentary()->i64 {return 9;}
fn n_eom()->i64 {return 10;}
fn n_atem()->i64 {return 11;}
fn n_functionary_all()->i64 {return 12;}
fn n_functionary_recip()->i64 {return 13;}
fn n_kimi_section()->i64 {return 14;}
fn n_kimi_begin()->i64 {return 15;}
fn n_kimi_open()->i64 {return 16;}
fn n_kimi_close()->i64 {return 17;}
fn n_kimi_eomsg()->i64 {return 18;}
fn n_ifm_start()->i64 {return 19;}
fn n_ifm_tools()->i64 {return 20;}
fn n_ling_role()->i64 {return 21;}
fn n_arg_key()->i64 {return 22;}
fn n_cohere_text()->i64 {return 23;}
fn n_cohere_action()->i64 {return 24;}
fn n_lfm2_list_start()->i64 {return 25;}
fn n_lfm2_list_end()->i64 {return 26;}
fn n_lfm25_tools()->i64 {return 27;}
fn n_giga_role()->i64 {return 28;}
fn n_giga_msg()->i64 {return 29;}
fn n_giga_fn()->i64 {return 30;}
fn n_minimax_ns()->i64 {return 31;}
fn n_tool_call_tag()->i64 {return 32;}
fn n_invoke_name()->i64 {return 33;}
fn n_dsml_token()->i64 {return 34;}
fn n_dsml_caps()->i64 {return 35;}
fn n_function_calls()->i64 {return 36;}
fn n_tool_calls_word()->i64 {return 37;}
fn n_gemma4_call()->i64 {return 38;}
fn n_gemma4_openai()->i64 {return 39;}
fn n_minicpm_guide()->i64 {return 40;}
fn n_minicpm_function()->i64 {return 41;}
fn n_minicpm_param()->i64 {return 42;}
fn n_translate_src()->i64 {return 43;}
fn n_translate_tgt()->i64 {return 44;}
fn n_function_eq()->i64 {return 45;}
fn n_parameter_eq()->i64 {return 46;}
fn n_qwen3_exclude()->i64 {return 47;}
fn n_stepfun()->i64 {return 48;}

fn h_none()->i64 {return 0;}
fn h_mistral3()->i64 {return 1;}
fn h_llm_jp()->i64 {return 2;}
fn h_harmony_xml()->i64 {return 3;}
fn h_gpt_oss()->i64 {return 4;}
fn h_muse_spark()->i64 {return 5;}
fn h_muse_glimmer()->i64 {return 6;}
fn h_functionary()->i64 {return 7;}
fn h_kimi_k2()->i64 {return 8;}
fn h_kimi_k3()->i64 {return 9;}
fn h_k2_horizon()->i64 {return 10;}
fn h_ling3()->i64 {return 11;}
fn h_cohere2moe()->i64 {return 12;}
fn h_lfm2()->i64 {return 13;}
fn h_lfm2_5()->i64 {return 14;}
fn h_gigachat()->i64 {return 15;}
fn h_minimax_m3()->i64 {return 16;}
fn h_deepseek()->i64 {return 17;}
fn h_gemma4_legacy()->i64 {return 18;}
fn h_gemma4()->i64 {return 19;}
fn h_minicpm5()->i64 {return 20;}
fn h_translate_gemma()->i64 {return 21;}
fn h_qwen3_coder()->i64 {return 22;}

fn has(t:i64,n:i64)->bool {return tmpl_has(t,n)!=0;}

// Exact order of common_chat_template_detect_params_init (chat.cpp:1092-1256).
fn detect(sel:i64)->i64 {
    if has(sel,n_ministral_system()) && has(sel,n_ministral_calls()) &&
       has(sel,n_ministral_args()) && !has(sel,n_ministral_call_id()) {return h_mistral3();}
    if has(sel,n_llm_jp()) {return h_llm_jp();}
    if has(sel,n_channel()) && has(sel,n_constrain_xml()) && has(sel,n_rfl()) {return h_harmony_xml();}
    if has(sel,n_channel()) {return h_gpt_oss();}
    if has(sel,n_muse_commentary()) && has(sel,n_eom()) {return h_muse_spark();}
    if has(sel,n_atem()) && has(sel,n_eom()) {return h_muse_glimmer();}
    if has(sel,n_functionary_all()) && has(sel,n_functionary_recip()) {return h_functionary();}
    if has(sel,n_kimi_section()) && has(sel,n_kimi_begin()) {return h_kimi_k2();}
    if has(sel,n_kimi_open()) && has(sel,n_kimi_close()) && has(sel,n_kimi_eomsg()) {return h_kimi_k3();}
    if has(sel,n_ifm_start()) && has(sel,n_ifm_tools()) {return h_k2_horizon();}
    if has(sel,n_ling_role()) && has(sel,n_arg_key()) {return h_ling3();}
    if has(sel,n_cohere_text()) && has(sel,n_cohere_action()) {return h_cohere2moe();}
    if has(sel,n_lfm2_list_start()) && has(sel,n_lfm2_list_end()) {return h_lfm2();}
    if has(sel,n_lfm25_tools()) && !has(sel,n_lfm2_list_start()) {return h_lfm2_5();}
    if has(sel,n_giga_role()) && has(sel,n_giga_msg()) && !has(sel,n_giga_fn()) {return h_gigachat();}
    if has(sel,n_minimax_ns()) && has(sel,n_tool_call_tag()) && has(sel,n_invoke_name()) {return h_minimax_m3();}
    if has(sel,n_dsml_token()) && has(sel,n_dsml_caps()) &&
       (has(sel,n_function_calls()) || has(sel,n_tool_calls_word())) {return h_deepseek();}
    if has(sel,n_gemma4_call()) {
        if !has(sel,n_gemma4_openai()) {return h_gemma4_legacy();}
        return h_gemma4();
    }
    if has(sel,n_minicpm_guide()) && has(sel,n_minicpm_function()) && has(sel,n_minicpm_param()) {return h_minicpm5();}
    if has(sel,n_translate_src()) && has(sel,n_translate_tgt()) {return h_translate_gemma();}
    if has(sel,n_tool_call_tag()) && has(sel,n_function_eq()) && has(sel,n_parameter_eq()) &&
       !has(sel,n_qwen3_exclude()) {return h_qwen3_coder();}
    return h_none();
}

// workaround::map_developer_role_to_system (chat.cpp:989-997)
fn map_developer(p:i64)->i64 {
    let msgs=plan_msgs(p);let n=vec_len(msgs);var i:i64=0;
    while i<n {
        let m=msg_at(msgs,i);
        if msg_role(m)==role_developer() {msg_set_role(m,role_system());}
        i+=1;
    }
    return 0;
}

// workaround::system_message_not_supported (chat.cpp:1001-1015)
fn system_unsupported(p:i64)->i64 {
    let msgs=plan_msgs(p);let n=plan_msgs_count(p);
    if n<=0 {return 0;}
    if msg_role(msg_at(msgs,0))!=role_system() {return 0;}
    if n>1 {
        let r=msgs_merge_content(msgs,1,0);
        if r<0 {return r;}
    }
    return msgs_erase_first(msgs);
}

// workaround::requires_non_null_content (chat.cpp:1017-1024)
fn require_content(p:i64)->i64 {
    let msgs=plan_msgs(p);let n=plan_msgs_count(p);var i:i64=0;
    while i<n {
        let m=msg_at(msgs,i);
        if msg_has_tool_calls(m)!=0 && msg_has_content(m)==0 {msg_set_empty_content(m);}
        i+=1;
    }
    return 0;
}

// workaround::func_args_not_string (chat.cpp:1026-1044)
fn parse_args(p:i64)->i64 {
    let msgs=plan_msgs(p);let n=plan_msgs_count(p);var i:i64=0;
    while i<n {
        let r=msg_parse_args(msg_at(msgs,i));
        if r<0 {return r;}
        i+=1;
    }
    return 0;
}

// workaround::trim_all_content (chat.cpp:1051-1061)
fn trim_all(p:i64)->i64 {
    let msgs=plan_msgs(p);let n=plan_msgs_count(p);var i:i64=0;
    while i<n {let r=msg_trim(msg_at(msgs,i));if r<0 {return r;}i+=1;}
    return 0;
}

fn apply(p:i64)->i64 {
    plan_clear_result(p);

    // Template selection: tools array and a tool-use template present.
    let has_tools=plan_tools_count(p)>0;
    let def=plan_template(p,0);
    let tu=plan_template(p,1);
    let sel=if has_tools&&tu!=0 {tu} else {def};
    if sel==0 {return err_state();}
    plan_set(p,pk_selected(),sel);
    plan_set(p,pk_selected_which(),if has_tools&&tu!=0 {1} else {0});
    res_set(p,rk_selected(),if has_tools&&tu!=0 {1} else {0});

    // continue_final_message resolution (chat.cpp:1326-1345)
    var cont=plan_get(p,pk_cont());
    var addgen=plan_get(p,pk_add_gen());
    if cont!=cont_none() {
        addgen=0;
        if plan_msgs_count(p)>0 {
            let ignored=plan_pop_continuation(p);
            if cont==cont_auto() {
                cont=cont_content();
                if cont_reasoning_len(p)>0 && cont_content_len(p)==0 && cont_parts_len(p)==0 {
                    cont=cont_reasoning();
                }
            }
        }
    }
    plan_set(p,pk_cont(),cont);
    plan_set(p,pk_add_gen(),addgen);
    res_set(p,rk_cont(),cont);
    res_set(p,rk_add_gen(),addgen);

    // StepFun content trimming happens before rendering (chat.cpp:1309-1315).
    if has(sel,n_stepfun()) {
        let tr=trim_all(p);if tr<0 {return tr;}
    }

    let caps=plan_caps(p,sel);

    // developer -> system for every template except channel-style GPT-OSS
    if !has(sel,n_channel()) {
        let md=map_developer(p);if md<0 {return md;}
    }
    if (caps&cap_system())==0 {
        let su=system_unsupported(p);if su<0 {return su;}
    }
    if (caps&cap_tool_calls())!=0 {
        let rc=require_content(p);if rc<0 {return rc;}
    }
    if (caps&cap_object_args())!=0 {
        let pa=parse_args(p);if pa<0 {return pa;}
    }

    let ignored_ctx=plan_extra_context(p);
    let ignored_schema=plan_parse_json_schema(p);

    // tools + grammar conflict (chat.cpp:1378-1387)
    if has_tools && plan_get(p,pk_tool_choice())!=tool_none() && plan_grammar_len(p)>0 {
        plan_set(p,pk_error(),err_grammar_tools());
        res_set(p,rk_error(),err_grammar_tools());
        return err_grammar_tools();
    }

    // force pure content (chat.cpp:1389-1402)
    if plan_get(p,pk_force_pure())!=0 {
        let rp=plan_render_pure(p);
        if rp<0 {plan_set(p,pk_error(),rp);res_set(p,rk_error(),rp);return rp;}
        plan_set(p,pk_format(),fmt_peg_native());
        res_set(p,rk_format(),fmt_peg_native());
        let ignored=plan_store_messages_json(p);
        return 0;
    }

    let handler=detect(sel);
    plan_set(p,pk_handler(),handler);
    res_set(p,rk_handler(),handler);

    if handler!=h_none() {
        let r=native_specialized(p,handler);
        if r<0 {plan_set(p,pk_error(),r);res_set(p,rk_error(),r);return r;}
        let ignored=plan_store_messages_json(p);
        return 0;
    }

    // No dedicated handler: the differential autoparser is required.
    if plan_analysis_ready(p)==0 {
        plan_set(p,pk_error(),err_no_analysis());
        res_set(p,rk_error(),err_no_analysis());
        res_set(p,rk_analysis(),0);
        return err_no_analysis();
    }
    res_set(p,rk_analysis(),1);

    let ar=native_autoparser(p);
    if ar<0 {plan_set(p,pk_error(),ar);res_set(p,rk_error(),ar);return ar;}
    plan_set(p,pk_format(),fmt_peg_native());
    res_set(p,rk_format(),fmt_peg_native());

    // Reasoning/thinking dispatch from the analysis result (chat.cpp:1427-1434).
    let mode=plan_reason_mode(p);
    if mode==reason_tag() || mode==reason_tools_only() {
        res_set(p,rk_supports_thinking(),1);
        let st=plan_analysis_str(p,2);
        if st!=0 {
            let t=str_trim_dup(st);
            if t!=0 {let ignored=res_str_take(p,rk_thinking_start(),t);}
        }
        let en=plan_analysis_str(p,3);
        if en!=0 {
            let t=str_trim_dup(en);
            if t!=0 {
                if str_len(t)>0 {let ignored=res_str_take(p,rk_thinking_end(),t);}
                else {str_free(t);}
            }
        }
    }

    // message delimiters from the analysis (chat.cpp:1417-1425)
    let a=plan_analysis_str(p,0);
    if a!=0 {
        if str_len(a)>0 {let ignored=plan_add_delimiter(p,2,a);}
    }
    let u=plan_analysis_str(p,1);
    if u!=0 {
        if str_len(u)>0 {let ignored=plan_add_delimiter(p,1,u);}
    }

    let ignored2=plan_store_messages_json(p);
    return 0;
}

fn detect_only(p:i64)->i64 {
    plan_clear_result(p);
    let has_tools=plan_tools_count(p)>0;
    let def=plan_template(p,0);
    let tu=plan_template(p,1);
    let sel=if has_tools&&tu!=0 {tu} else {def};
    if sel==0 {return err_state();}
    plan_set(p,pk_selected(),sel);
    plan_set(p,pk_selected_which(),if has_tools&&tu!=0 {1} else {0});
    res_set(p,rk_selected(),if has_tools&&tu!=0 {1} else {0});
    let h=detect(sel);
    plan_set(p,pk_handler(),h);
    res_set(p,rk_handler(),h);
    return 0;
}

fn entry(op:i64,plan:i64,a:i64,b:i64,c:i64)->i64 {
    if op==0 {return 1;}
    if op==1 {return apply(plan);}
    if op==2 {return detect_only(plan);}
    return err_state();
}

fn main() {print(entry(0,0,0,0,0));}
