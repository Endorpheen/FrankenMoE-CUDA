// LOCAL CHECK ONLY parser-engine effect projection.
//
// The real dedicated handlers and PEG autoparser are production effects in
// chat-native-effects.cpp, which links the native parser objects.  This file
// only satisfies the two symbols for the isolated control fixture so the
// Goose reasoning/format dispatch after the engine hand-off can be observed.
// It intentionally performs no parsing and is never linked into production.

#include "chat-public.h"
#include "chat-schema.h"

extern "C" int64_t apx028_native_specialized_apply(int64_t plan, int64_t handler) {
    (void) plan;
    (void) handler;
    return APX_OK;
}

extern "C" int64_t apx028_native_autoparser_generate(int64_t plan) {
    (void) apx028_res_set_text(plan, RK_GEN_PROMPT, "");
    return APX_OK;
}
