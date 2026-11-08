#pragma once
/* APX028 chat/Jinja control public and Goose-facing C ABI.
 *
 * The Goose unit (chat-control.goose) imports only the primitive names below.
 * The public plan API is used by the native patched-TU adapter and by the
 * differential fixture.  All handles are int64_t; strings are canonical
 * byte-vector handles, never C++ containers hidden behind an opaque pointer.
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Goose generated entry, bound in chat-control-wrapper.c */
int64_t apx028_chat_entry(int64_t op, int64_t plan, int64_t a, int64_t b, int64_t c);

/* ---- plan lifecycle / construction (native side) ------------------------- */
int64_t apx028_plan_new(void);
void    apx028_plan_free(int64_t plan);
int64_t apx028_plan_set_template(int64_t plan, int64_t which,
                                 const char * src, const char * bos, const char * eos);
int64_t apx028_plan_add_message_json(int64_t plan, const char * message_json);
int64_t apx028_plan_set_tools_json(int64_t plan, const char * tools_json);
int64_t apx028_plan_add_kwarg(int64_t plan, const char * key, const char * value);
int64_t apx028_plan_set_grammar(int64_t plan, const char * grammar);
int64_t apx028_plan_set_json_schema(int64_t plan, const char * json_schema);
void    apx028_plan_set_int(int64_t plan, int64_t key, int64_t value);
int64_t apx028_plan_get_int(int64_t plan, int64_t key);
void    apx028_plan_set_caps_override(int64_t plan, int64_t which, int64_t caps);
void    apx028_plan_set_analysis(int64_t plan, int64_t ready, int64_t mode,
                                 const char * start, const char * end);
void    apx028_plan_set_boundaries(int64_t plan, const char * assistant_start,
                                   const char * user_start);
void    apx028_plan_set_native(int64_t plan, int64_t native_tmpl,
                               int64_t native_inputs);
int64_t apx028_plan_get_native(int64_t plan, int64_t which);
void    apx028_plan_set_native_result(int64_t plan, int64_t result_ptr);
int64_t apx028_plan_take_native_result(int64_t plan);
int64_t apx028_plan_apply(int64_t plan);
int64_t apx028_plan_detect(int64_t plan);

/* result accessors (native side) */
int64_t     apx028_plan_result_int(int64_t plan, int64_t key);
const char *apx028_plan_result_str(int64_t plan, int64_t key);
const char *apx028_plan_messages_json(int64_t plan);
const char *apx028_plan_prompt_json(int64_t plan);
const char *apx028_plan_prompt_text(int64_t plan);
const char *apx028_plan_tools_json(int64_t plan);
const char *apx028_plan_extra_context_json(int64_t plan);
const char *apx028_plan_continuation_json(int64_t plan);
const char *apx028_plan_grammar_text(int64_t plan);
const char *apx028_plan_json_schema_text(int64_t plan);

/* ---- canonical storage bridge primitives (Goose imports) ----------------- */
int64_t cf_fgc_record_new(int64_t kind);
int64_t cf_fgc_record_get(int64_t handle, int64_t key);
int64_t cf_fgc_record_set(int64_t handle, int64_t key, int64_t value);
int64_t cf_fgc_record_free(int64_t handle);
int64_t cf_fgc_vec_new(int64_t item_bytes);
int64_t cf_fgc_vec_len(int64_t handle);
int64_t cf_fgc_vec_data(int64_t handle);
int64_t cf_fgc_vec_resize(int64_t handle, int64_t count);
int64_t cf_fgc_vec_free(int64_t handle);
int64_t cf_fgc_bytes_new(void);
int64_t cf_fgc_bytes_append(int64_t handle, int64_t src, int64_t n);
int64_t cf_fgc_bytes_free(int64_t handle);
int64_t cf_fgc_str_set(int64_t handle, int64_t src, int64_t n);
int64_t cf_fgc_str_data(int64_t handle);
int64_t cf_fgc_str_len(int64_t handle);
int64_t cf_fgc_str_free(int64_t handle);

/* ---- plan / template / message primitives -------------------------------- */
int64_t apx028_plan_get(int64_t plan, int64_t key);
void    apx028_plan_set(int64_t plan, int64_t key, int64_t value);
int64_t apx028_plan_template(int64_t plan, int64_t which);
int64_t apx028_plan_msgs(int64_t plan);
int64_t apx028_plan_msgs_count(int64_t plan);
int64_t apx028_plan_cont_msgs_count(int64_t plan);
int64_t apx028_plan_tools_count(int64_t plan);
int64_t apx028_plan_grammar_len(int64_t plan);
int64_t apx028_plan_caps(int64_t plan, int64_t tmpl);
int64_t apx028_plan_extra_context(int64_t plan);
int64_t apx028_plan_parse_json_schema(int64_t plan);
int64_t apx028_plan_pop_continuation(int64_t plan);
int64_t apx028_plan_cont_reasoning_len(int64_t plan);
int64_t apx028_plan_cont_content_len(int64_t plan);
int64_t apx028_plan_cont_parts_len(int64_t plan);
int64_t apx028_plan_render_pure(int64_t plan);
int64_t apx028_plan_store_messages_json(int64_t plan);
int64_t apx028_plan_analysis_ready(int64_t plan);
int64_t apx028_plan_reason_mode(int64_t plan);
int64_t apx028_plan_analysis_str(int64_t plan, int64_t which);
const char * apx028_plan_analysis_text(int64_t plan, int64_t which);
int64_t apx028_plan_add_delimiter(int64_t plan, int64_t role, int64_t str_handle);
void    apx028_plan_clear_result(int64_t plan);

int64_t apx028_tmpl_has(int64_t tmpl, int64_t needle);
int64_t apx028_tmpl_caps(int64_t tmpl);
int64_t apx028_tmpl_src_flags(int64_t tmpl);
int64_t apx028_tmpl_src(int64_t tmpl, int64_t field);

int64_t apx028_msg_at(int64_t msgs, int64_t index);
int64_t apx028_msg_role(int64_t msg);
void    apx028_msg_set_role(int64_t msg, int64_t role);
int64_t apx028_msg_content_len(int64_t msg);
int64_t apx028_msg_reasoning_len(int64_t msg);
int64_t apx028_msg_parts_len(int64_t msg);
int64_t apx028_msg_has_tool_calls(int64_t msg);
int64_t apx028_msg_has_content(int64_t msg);
void    apx028_msg_set_empty_content(int64_t msg);
int64_t apx028_msgs_merge_content(int64_t msgs, int64_t dst_index, int64_t src_index);
int64_t apx028_msgs_erase_first(int64_t msgs);
int64_t apx028_msg_parse_args(int64_t msg);
int64_t apx028_msg_trim(int64_t msg);

int64_t apx028_res_get(int64_t plan, int64_t key);
void    apx028_res_set(int64_t plan, int64_t key, int64_t value);
int64_t apx028_res_str_copy(int64_t plan, int64_t key, int64_t src_handle);
int64_t apx028_res_str_take(int64_t plan, int64_t key, int64_t str_handle);
int64_t apx028_res_set_text(int64_t plan, int64_t key, const char * text);

int64_t apx028_str_dup(int64_t handle);
int64_t apx028_str_trim_dup(int64_t handle);
int64_t apx028_str_len(int64_t handle);
void    apx028_str_free(int64_t handle);

/* Native parser-engine effects.  Strong definitions live in the production
 * adapter; this module ships weak "engine unavailable" definitions so an
 * isolated fixture cannot mistake a stub for a completed parser. */
int64_t apx028_native_specialized_apply(int64_t plan, int64_t handler);
int64_t apx028_native_autoparser_generate(int64_t plan);

#ifdef __cplusplus
}
#endif
