// Bridge for goose/misc/unicode-qwen2.goose. The case-insensitive contraction test calls the same unicode_tolower the
// native code called, reached by its mangled name because it is a C++ function; an ASCII-only comparison would differ
// on non-ASCII input and make the gate compare two different rules.
#include <stddef.h>
#include <stdint.h>

int64_t q2_ld_u32(uint64_t b, int64_t o) { return (int64_t) *(const uint32_t *)(uintptr_t)(b + (uint64_t)o); }
int64_t q2_ld_u16(uint64_t b, int64_t o) { return (int64_t) *(const uint16_t *)(uintptr_t)(b + (uint64_t)o); }
int64_t q2_ld_i64(uint64_t b, int64_t o) { return *(const int64_t *)(uintptr_t)(b + (uint64_t)o); }
void    q2_st_i64(uint64_t b, int64_t o, int64_t v) { *(int64_t *)(uintptr_t)(b + (uint64_t)o) = v; }
void    q2_fail(int64_t c) { (void) c; }

unsigned int _Z15unicode_tolowerj(unsigned int cpt);
int64_t q2_tolower(int64_t c) { return (int64_t) _Z15unicode_tolowerj((unsigned int) c); }

int64_t q2_split(int64_t packet);
void fg_qwen2_goose_init(int argc, char ** argv);
const uint16_t * fg_unicode_flags_view_c(void);

static int g_started = 0;

int64_t fg_unicode_qwen2_split(const uint32_t * cpts, size_t n_cpts,
                               const size_t * offsets, size_t n_off,
                               size_t * out, size_t cap) {
    (void) n_cpts;
    if (!g_started) { fg_qwen2_goose_init(0, (char **) 0); g_started = 1; }
    int64_t pkt[6];
    pkt[0] = (int64_t) (uintptr_t) cpts;
    pkt[1] = (int64_t) n_off;
    pkt[2] = (int64_t) (uintptr_t) offsets;
    pkt[3] = (int64_t) (uintptr_t) fg_unicode_flags_view_c();
    pkt[4] = (int64_t) (uintptr_t) out;
    pkt[5] = (int64_t) cap;
    return q2_split((int64_t) (uintptr_t) pkt);
}
