#pragma once
/* Dispatch seam inserted into common_chat_templates_apply_jinja by the
 * anchored source patch.  This header is C++ only and uses the public
 * common_chat_template / common_chat_templates_inputs types from chat.h. */
#include "chat.h"

bool apx028_chat_enabled(void);

common_chat_params apx028_chat_apply_templates(
        const common_chat_template *          tmpl_default,
        const common_chat_template *          tmpl_tool_use,
        bool                                  add_bos,
        bool                                  add_eos,
        const common_chat_templates_inputs &  inputs);
