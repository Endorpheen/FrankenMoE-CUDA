#pragma once
/* APX028 chat/Jinja control schema.
 *
 * Every record, key, flag and enum below is consumed by the Goose control unit
 * (chat-control.goose), the typed service projection (chat-services.cpp) and
 * the native patched-TU adapter.  The values mirror public definitions in
 * inputs/native/common/chat.h and common.h; no native private struct or
 * container layout is reproduced here.
 */

/* record kinds (worker-8 canonical records) */
#define APX_KIND_PLAN      INT64_C(9001)
#define APX_KIND_TEMPLATE  INT64_C(9002)
#define APX_KIND_MSG       INT64_C(9003)
#define APX_KIND_PART      INT64_C(9004)
#define APX_KIND_TOOLCALL  INT64_C(9005)
#define APX_KIND_TOOL      INT64_C(9006)
#define APX_KIND_RESULT    INT64_C(9007)

/* plan record keys */
#define PK_TMPL_DEFAULT       INT64_C(0)
#define PK_TMPL_TOOL_USE      INT64_C(1)
#define PK_MSGS               INT64_C(2)
#define PK_TOOLS              INT64_C(3)
#define PK_KWARGS_JSON        INT64_C(4)
#define PK_GRAMMAR            INT64_C(5)
#define PK_JSON_SCHEMA        INT64_C(6)
#define PK_FLAGS              INT64_C(7)
#define PK_CONT               INT64_C(8)
#define PK_TOOL_CHOICE        INT64_C(9)
#define PK_REASONING_FORMAT   INT64_C(10)
#define PK_ADD_BOS            INT64_C(11)
#define PK_ADD_EOS            INT64_C(12)
#define PK_SELECTED           INT64_C(13)
#define PK_SELECTED_WHICH     INT64_C(14)
#define PK_CONT_MSG           INT64_C(15)
#define PK_RESULT             INT64_C(16)
#define PK_FORMAT             INT64_C(17)
#define PK_HANDLER            INT64_C(18)
#define PK_ANALYSIS_READY     INT64_C(19)
#define PK_REASON_MODE        INT64_C(20)
#define PK_REASON_START       INT64_C(21)
#define PK_REASON_END         INT64_C(22)
#define PK_ADD_GEN            INT64_C(23)
#define PK_ERROR              INT64_C(24)
#define PK_FORCE_PURE         INT64_C(25)
#define PK_PARALLEL           INT64_C(26)
#define PK_ENABLE_THINKING    INT64_C(27)
#define PK_MARK_INPUT         INT64_C(28)
#define PK_CAPS_OVERRIDE      INT64_C(29)
#define PK_VENDOR             INT64_C(30)
#define PK_ASSISTANT_START    INT64_C(31)
#define PK_USER_START         INT64_C(32)
#define PK_JSON_SCHEMA_PARSED INT64_C(33)
#define PK_VIEW               INT64_C(34)

/* result record keys */
#define RK_FORMAT             INT64_C(0)
#define RK_SELECTED_WHICH     INT64_C(1)
#define RK_HANDLER            INT64_C(2)
#define RK_CONT               INT64_C(3)
#define RK_ADD_GEN            INT64_C(4)
#define RK_SUPPORTS_THINKING  INT64_C(5)
#define RK_THINKING_START     INT64_C(6)
#define RK_THINKING_END       INT64_C(7)
#define RK_GRAMMAR            INT64_C(8)
#define RK_GEN_PROMPT         INT64_C(9)
#define RK_PROMPT_JSON        INT64_C(10)
#define RK_MESSAGES_JSON      INT64_C(11)
#define RK_ERROR              INT64_C(12)
#define RK_ANALYSIS_READY     INT64_C(13)
#define RK_TOOL_CHOICE        INT64_C(14)
#define RK_PARALLEL           INT64_C(15)
#define RK_PROMPT_TEXT        INT64_C(16)

/* template record keys */
#define TK_SRC        INT64_C(0)
#define TK_BOS        INT64_C(1)
#define TK_EOS        INT64_C(2)
#define TK_CAPS       INT64_C(3)
#define TK_SRC_FLAGS  INT64_C(4)
#define TK_VENDOR     INT64_C(5)

/* message record keys */
#define MK_ROLE       INT64_C(0)
#define MK_CONTENT    INT64_C(1)
#define MK_REASONING  INT64_C(2)
#define MK_NAME       INT64_C(3)
#define MK_TCID       INT64_C(4)
#define MK_PARTS      INT64_C(5)
#define MK_TOOLCALLS  INT64_C(6)
#define MK_FLAGS      INT64_C(7)

/* message flags */
#define MF_HAS_CONTENT    INT64_C(1)
#define MF_HAS_REASONING  INT64_C(2)
#define MF_HAS_NAME       INT64_C(4)
#define MF_HAS_TCID       INT64_C(8)

/* part record keys */
#define PT_TYPE  INT64_C(0)
#define PT_TEXT  INT64_C(1)

/* tool call record keys */
#define TC_NAME       INT64_C(0)
#define TC_ARGS       INT64_C(1)
#define TC_ID         INT64_C(2)
#define TC_IS_OBJECT  INT64_C(3)

/* tool record keys */
#define TL_NAME  INT64_C(0)
#define TL_DESC  INT64_C(1)
#define TL_PARAM INT64_C(2)

/* plan flag bits */
#define APX_F_ADD_BOS          INT64_C(1)
#define APX_F_ADD_EOS          INT64_C(2)
#define APX_F_ADD_GEN          INT64_C(4)
#define APX_F_USE_JINJA        INT64_C(8)
#define APX_F_FORCE_PURE       INT64_C(16)
#define APX_F_PARALLEL         INT64_C(32)
#define APX_F_ENABLE_THINKING  INT64_C(64)
#define APX_F_MARK_INPUT       INT64_C(128)

/* template capability bits (jinja::caps, public) */
#define APX_C_TOOLS               INT64_C(1)
#define APX_C_TOOL_CALLS          INT64_C(2)
#define APX_C_SYSTEM              INT64_C(4)
#define APX_C_PARALLEL            INT64_C(8)
#define APX_C_STRING              INT64_C(16)
#define APX_C_TYPED               INT64_C(32)
#define APX_C_OBJECT_ARGS         INT64_C(64)
#define APX_C_PRESERVE_REASONING  INT64_C(128)
#define APX_C_REASONING_EFFORT    INT64_C(256)

/* template source flags */
#define APX_S_CHANNEL  INT64_C(1)
#define APX_S_STEPFUN  INT64_C(2)

/* roles (common_chat_role) */
#define APX_ROLE_UNKNOWN    INT64_C(0)
#define APX_ROLE_SYSTEM     INT64_C(1)
#define APX_ROLE_ASSISTANT  INT64_C(2)
#define APX_ROLE_USER       INT64_C(3)
#define APX_ROLE_TOOL       INT64_C(4)
#define APX_ROLE_DEVELOPER  INT64_C(5)

/* continuation (common_chat_continuation) */
#define APX_CONT_NONE       INT64_C(0)
#define APX_CONT_AUTO       INT64_C(1)
#define APX_CONT_REASONING  INT64_C(2)
#define APX_CONT_CONTENT    INT64_C(3)

/* tool choice (common_chat_tool_choice) */
#define APX_TOOL_AUTO      INT64_C(0)
#define APX_TOOL_REQUIRED  INT64_C(1)
#define APX_TOOL_NONE      INT64_C(2)

/* reasoning format (common_reasoning_format) */
#define APX_REASON_FMT_NONE             INT64_C(0)
#define APX_REASON_FMT_AUTO             INT64_C(1)
#define APX_REASON_FMT_DEEPSEEK_LEGACY  INT64_C(2)
#define APX_REASON_FMT_DEEPSEEK         INT64_C(3)

/* format (common_chat_format) */
#define APX_FMT_CONTENT_ONLY  INT64_C(0)
#define APX_FMT_PEG_SIMPLE    INT64_C(1)
#define APX_FMT_PEG_NATIVE    INT64_C(2)
#define APX_FMT_PEG_GEMMA4    INT64_C(3)
#define APX_FMT_PEG_MINIMAX   INT64_C(4)

/* autoparser reasoning mode (autoparser::reasoning_mode) */
#define APX_REASON_NONE       INT64_C(0)
#define APX_REASON_TAG        INT64_C(1)
#define APX_REASON_TOOLS_ONLY INT64_C(2)

/* specialized handler ids, in common_chat_template_detect_params_init order */
#define APX_H_NONE             INT64_C(0)
#define APX_H_MISTRAL3         INT64_C(1)
#define APX_H_LLM_JP           INT64_C(2)
#define APX_H_HARMONY_XML      INT64_C(3)
#define APX_H_GPT_OSS          INT64_C(4)
#define APX_H_MUSE_SPARK       INT64_C(5)
#define APX_H_MUSE_GLIMMER     INT64_C(6)
#define APX_H_FUNCTIONARY      INT64_C(7)
#define APX_H_KIMI_K2          INT64_C(8)
#define APX_H_KIMI_K3          INT64_C(9)
#define APX_H_K2_HORIZON       INT64_C(10)
#define APX_H_LING3            INT64_C(11)
#define APX_H_COHERE2MOE       INT64_C(12)
#define APX_H_LFM2             INT64_C(13)
#define APX_H_LFM2_5           INT64_C(14)
#define APX_H_GIGACHAT         INT64_C(15)
#define APX_H_MINIMAX_M3       INT64_C(16)
#define APX_H_DEEPSEEK         INT64_C(17)
#define APX_H_GEMMA4_LEGACY    INT64_C(18)
#define APX_H_GEMMA4           INT64_C(19)
#define APX_H_MINICPM5         INT64_C(20)
#define APX_H_TRANSLATE_GEMMA  INT64_C(21)
#define APX_H_QWEN3_CODER      INT64_C(22)

/* needle ids: each maps to one exact substring from chat.cpp */
#define APX_N_MINISTRAL_SYSTEM   INT64_C(1)
#define APX_N_MINISTRAL_CALLS    INT64_C(2)
#define APX_N_MINISTRAL_ARGS     INT64_C(3)
#define APX_N_MINISTRAL_CALL_ID  INT64_C(4)
#define APX_N_LLM_JP             INT64_C(5)
#define APX_N_CHANNEL            INT64_C(6)
#define APX_N_CONSTRAIN_XML      INT64_C(7)
#define APX_N_RFL_PARAMETER      INT64_C(8)
#define APX_N_MUSE_COMMENTARY    INT64_C(9)
#define APX_N_EOM                INT64_C(10)
#define APX_N_ATEM               INT64_C(11)
#define APX_N_FUNCTIONARY_ALL    INT64_C(12)
#define APX_N_FUNCTIONARY_RECIP  INT64_C(13)
#define APX_N_KIMI_SECTION       INT64_C(14)
#define APX_N_KIMI_BEGIN         INT64_C(15)
#define APX_N_KIMI_OPEN          INT64_C(16)
#define APX_N_KIMI_CLOSE         INT64_C(17)
#define APX_N_KIMI_EOMSG         INT64_C(18)
#define APX_N_IFM_START          INT64_C(19)
#define APX_N_IFM_TOOLS          INT64_C(20)
#define APX_N_LING_ROLE          INT64_C(21)
#define APX_N_ARG_KEY            INT64_C(22)
#define APX_N_COHERE_TEXT        INT64_C(23)
#define APX_N_COHERE_ACTION      INT64_C(24)
#define APX_N_LFM2_LIST_START    INT64_C(25)
#define APX_N_LFM2_LIST_END      INT64_C(26)
#define APX_N_LFM25_TOOLS        INT64_C(27)
#define APX_N_GIGA_ROLE          INT64_C(28)
#define APX_N_GIGA_MSG           INT64_C(29)
#define APX_N_GIGA_FN            INT64_C(30)
#define APX_N_MINIMAX_NS         INT64_C(31)
#define APX_N_TOOL_CALL_TAG      INT64_C(32)
#define APX_N_INVOKE_NAME        INT64_C(33)
#define APX_N_DSML_TOKEN         INT64_C(34)
#define APX_N_DSML_CAPS          INT64_C(35)
#define APX_N_FUNCTION_CALLS     INT64_C(36)
#define APX_N_TOOL_CALLS_WORD    INT64_C(37)
#define APX_N_GEMMA4_CALL        INT64_C(38)
#define APX_N_GEMMA4_OPENAI      INT64_C(39)
#define APX_N_MINICPM_GUIDE      INT64_C(40)
#define APX_N_MINICPM_FUNCTION   INT64_C(41)
#define APX_N_MINICPM_PARAM      INT64_C(42)
#define APX_N_TRANSLATE_SRC      INT64_C(43)
#define APX_N_TRANSLATE_TGT      INT64_C(44)
#define APX_N_FUNCTION_EQ        INT64_C(45)
#define APX_N_PARAMETER_EQ       INT64_C(46)
#define APX_N_QWEN3_EXCLUDE      INT64_C(47)
#define APX_N_STEPFUN            INT64_C(48)

/* status codes */
#define APX_OK              INT64_C(0)
#define APX_ERR_INVAL       INT64_C(-1)
#define APX_ERR_GRAMMAR_TOOLS INT64_C(-2)
#define APX_ERR_NO_ANALYSIS INT64_C(-3)
#define APX_ERR_ARGS_JSON   INT64_C(-4)
#define APX_ERR_NO_ENGINE   INT64_C(-5)
#define APX_ERR_RENDER      INT64_C(-6)
#define APX_ERR_STATE       INT64_C(-7)

/* Goose entry operations */
#define APX_OP_SELFTEST INT64_C(0)
#define APX_OP_APPLY    INT64_C(1)
#define APX_OP_DETECT   INT64_C(2)
