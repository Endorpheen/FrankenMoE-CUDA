// Bridge for goose/misc/unicode-newlines.goose. Packet layout is built here; the module reads it with 64-bit loads
// because the first two fields are pointers.
#include <stdint.h>
#include <stddef.h>

int64_t nl_ld_u32(uint64_t base, int64_t byte) { return (int64_t) *(const uint32_t *)(uintptr_t)(base + (uint64_t)byte); }
int64_t nl_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
void    nl_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
void    nl_fail(int64_t code) { (void) code; }

int64_t nl_split(int64_t packet);
void fg_unicode_nl_goose_init(int argc, char ** argv);

static int g_started = 0;

int64_t fg_unicode_newlines_split(const uint32_t * cpts, size_t n_cpts,
                                  const size_t * offsets, size_t n_off,
                                  size_t * out, size_t cap) {
    (void) n_cpts;
    if (!g_started) { fg_unicode_nl_goose_init(0, (char **) 0); g_started = 1; }
    int64_t pkt[5];
    pkt[0] = (int64_t) (uintptr_t) cpts;
    pkt[1] = (int64_t) n_off;
    pkt[2] = (int64_t) (uintptr_t) offsets;
    pkt[3] = (int64_t) (uintptr_t) out;
    pkt[4] = (int64_t) cap;
    return nl_split((int64_t) (uintptr_t) pkt);
}
