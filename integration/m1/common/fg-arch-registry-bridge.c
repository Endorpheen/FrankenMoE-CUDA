// Bridge for the MoE architecture registry (P9-01 S2, goose/model/arch-registry.goose).
//
// The module has no FFI of its own - its signatures are sl_u8 / sl_i64 and nothing else - so this file only
// adds what the working runtime needs: the default-on switch, and one lazy initialisation of the module's own
// Goose globals.
//
// The generated C defines goose_init for itself; the SLRU policy module already defines one in this DSO, so
// that symbol is renamed here and started from ar_ensure_init instead of at load.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint8_t * data; int64_t len; } sl_u8;
typedef struct { int64_t * data; int64_t len; } sl_i64;

int64_t ar_find(sl_u8 arch);
int64_t ar_count(void);
int64_t ar_arch(int64_t i, sl_u8 out);
int64_t ar_suffix(int64_t i, int64_t slot, sl_u8 out);
int64_t ar_exps(int64_t i);
int64_t ar_match(sl_u8 name, int64_t row, sl_i64 lay);

void fg_arch_registry_goose_init(int argc, char ** argv);

static int g_started = 0;
static void ar_ensure_init(void) {
    if (!g_started) {
        fg_arch_registry_goose_init(0, (char **) 0);
        g_started = 1;
    }
}

// Default on (rule 10): the switch only restores the caller's previous behaviour.
int fg_n01s_enabled(void) {
    const char * v = getenv("GGML_GOOSE_ARCH_REGISTRY");
    return !(v && strcmp(v, "0") == 0);
}

int64_t fg_n01s_find(const char * arch, int64_t n) {
    ar_ensure_init();
    return ar_find((sl_u8) { (uint8_t *) (uintptr_t) arch, n });
}

int64_t fg_n01s_count(void) {
    ar_ensure_init();
    return ar_count();
}

int64_t fg_n01s_match(const char * name, int64_t n, int64_t row, const int64_t * lay, int64_t nlay) {
    ar_ensure_init();
    return ar_match((sl_u8) { (uint8_t *) (uintptr_t) name, n }, row, (sl_i64) { (int64_t *) (uintptr_t) lay, nlay });
}
