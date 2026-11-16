// Bridge for goose/misc/unicode-byte-map.goose: byte -> codepoint for all 256 bytes.
#include <stdint.h>

void b2u_st_u32(uint64_t buf, int64_t byte, int64_t v) { *(uint32_t *)(uintptr_t)(buf + (uint64_t)byte) = (uint32_t)v; }

int64_t b2u_map(uint64_t out);
void fg_b2u_goose_init(int argc, char ** argv);

static int g_started = 0;

int64_t fg_unicode_byte_map(uint32_t * out) {
    if (!g_started) { fg_b2u_goose_init(0, (char **) 0); g_started = 1; }
    return b2u_map((uint64_t) (uintptr_t) out);
}
