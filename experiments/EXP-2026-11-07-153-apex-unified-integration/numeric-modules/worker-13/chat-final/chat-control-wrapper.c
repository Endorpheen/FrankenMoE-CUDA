/* Verified strong entry wrapper for the APX028 chat Goose control unit. */
#include "chat-public.h"

#if defined(__has_include)
#  if __has_include("generated/chat-control.c")
#    define APX028_GENERATED_FILE "generated/chat-control.c"
#  endif
#endif
#ifndef APX028_GENERATED_FILE
#  define APX028_GENERATED_FILE "generated/chat-control.c"
#endif

#define main apx028_generated_main
#include APX028_GENERATED_FILE
#undef main

#ifndef APX028_GENERATED_ENTRY
#define APX028_GENERATED_ENTRY gg_function_1
#endif

int64_t apx028_chat_entry(int64_t op, int64_t plan, int64_t a, int64_t b, int64_t c) {
    return APX028_GENERATED_ENTRY(op, plan, a, b, c, 1);
}
