// Bridge for goose/misc/unicode-utf8.goose. Pure C: no C++ container, no C++ call - this is the first module whose
// dependency on the C++ side is only "someone passes a byte buffer".
#include <stdint.h>

void u8_st_u8(uint64_t b, int64_t o, int64_t v) { *(uint8_t *)(uintptr_t)(b + (uint64_t)o) = (uint8_t)v; }
void u8_st_u32(uint64_t b, int64_t o, int64_t v) { *(uint32_t *)(uintptr_t)(b + (uint64_t)o) = (uint32_t)v; }
int64_t u8_ld_u8(uint64_t b, int64_t o) { return (int64_t) *(const uint8_t *)(uintptr_t)(b + (uint64_t)o); }

int64_t u8_decode(uint64_t buf, int64_t pos, int64_t n, uint64_t out);
void fg_utf8_goose_init(int argc, char ** argv);

static int g_started = 0;
static void ensure(void) { if (!g_started) { fg_utf8_goose_init(0, (char **) 0); g_started = 1; } }

int64_t fg_utf8_decode(const uint8_t * buf, int64_t n, uint8_t * out) {
    ensure();
    return u8_decode((uint64_t) (uintptr_t) buf, 0, n, (uint64_t) (uintptr_t) out);
}
