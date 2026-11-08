// Exported wrapper for parent-review/hash-primitives.c.
// Same isolation and entry contract as the storage wrapper.
#include "graph-bridge.h"

#if defined(__has_include)
#  if __has_include("generated/hash-primitives.c")
#    define FG153_HP_GENERATED_FILE "generated/hash-primitives.c"
#  else
#    define FG153_HP_GENERATED_FILE "parent-review/hash-primitives.c"
#  endif
#endif
#ifndef FG153_HP_GENERATED_FILE
#  define FG153_HP_GENERATED_FILE "parent-review/hash-primitives.c"
#endif
#define main goose_hash_primitives_generated_main
#include FG153_HP_GENERATED_FILE
#undef main

#ifndef FG153_HP_GENERATED_ENTRY
#define FG153_HP_GENERATED_ENTRY gg_function_1
#endif

int64_t hp_entry(int64_t p0, int64_t p1, int64_t p2, int64_t p3,
                 int64_t p4, int64_t p5, int64_t p6) {
    return FG153_HP_GENERATED_ENTRY(p0, p1, p2, p3, p4, p5, p6, 1);
}

// Strong hash-table symbols required by graph-services.c. They bind the
// generated hp_entry modes; no weak fallback and no numeric emitter ID.
int64_t fg153_hp_find(int64_t hs, int64_t t) { return hp_entry(0, hs, t, 0, 0, 0, 0); }
int64_t fg153_hp_insert(int64_t hs, int64_t t) { return hp_entry(1, hs, t, 0, 0, 0, 0); }
int64_t fg153_hp_find_or_insert(int64_t hs, int64_t t) { return hp_entry(2, hs, t, 0, 0, 0, 0); }
int64_t fg153_hp_contains(int64_t hs, int64_t t) { return hp_entry(3, hs, t, 0, 0, 0, 0); }
