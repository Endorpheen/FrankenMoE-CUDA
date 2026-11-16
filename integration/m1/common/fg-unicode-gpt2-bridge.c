// Bridge for goose/misc/unicode-gpt2.goose: GPT-2 pre-tokenisation.
#include <stddef.h>
#include <stdint.h>

int64_t gp_ld_u32(uint64_t b, int64_t o) { return (int64_t) *(const uint32_t *)(uintptr_t)(b + (uint64_t)o); }
int64_t gp_ld_u16(uint64_t b, int64_t o) { return (int64_t) *(const uint16_t *)(uintptr_t)(b + (uint64_t)o); }
int64_t gp_ld_i64(uint64_t b, int64_t o) { return *(const int64_t *)(uintptr_t)(b + (uint64_t)o); }
void    gp_st_i64(uint64_t b, int64_t o, int64_t v) { *(int64_t *)(uintptr_t)(b + (uint64_t)o) = v; }
void    gp_fail(int64_t c) { (void) c; }

int64_t gp2_split(int64_t packet);
void fg_gpt2_goose_init(int argc, char ** argv);
const uint16_t * fg_unicode_flags_view_c(void);

static int g_started = 0;

int64_t fg_unicode_gpt2_split(const uint32_t * cpts, size_t n_cpts,
                              const size_t * offsets, size_t n_off,
                              size_t * out, size_t cap) {
    (void) n_cpts;
    if (!g_started) { fg_gpt2_goose_init(0, (char **) 0); g_started = 1; }
    int64_t pkt[6];
    pkt[0] = (int64_t) (uintptr_t) cpts;
    pkt[1] = (int64_t) n_off;
    pkt[2] = (int64_t) (uintptr_t) offsets;
    pkt[3] = (int64_t) (uintptr_t) fg_unicode_flags_view_c();
    pkt[4] = (int64_t) (uintptr_t) out;
    pkt[5] = (int64_t) cap;
    return gp2_split((int64_t) (uintptr_t) pkt);
}
