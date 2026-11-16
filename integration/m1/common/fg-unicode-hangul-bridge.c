// Bridge for goose/misc/unicode-hangul.goose: the algorithmic half of Hangul decomposition.
#include <stdint.h>

void hg_st_u32(uint64_t buf, int64_t byte, int64_t v) { *(uint32_t *)(uintptr_t)(buf + (uint64_t)byte) = (uint32_t)v; }

int64_t hg_decompose(int64_t cpt, uint64_t out);
void fg_hangul_goose_init(int argc, char ** argv);

static int g_started = 0;

int64_t fg_unicode_hangul_decompose(uint32_t cpt, uint32_t * out) {
    if (!g_started) { fg_hangul_goose_init(0, (char **) 0); g_started = 1; }
    return hg_decompose((int64_t) cpt, (uint64_t) (uintptr_t) out);
}
