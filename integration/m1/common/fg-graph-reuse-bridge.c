// Bridge for goose/cpu/graph-reuse.goose - the graph-input reuse decision.
//
// The module is FFI-light (ld/stw/fail only), so this file supplies those three and a single entry point that
// packs the caller's three arrays into the packet the module reads. No logic of its own.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int64_t gr_run(int64_t packet);
void fg_graph_reuse_goose_init(int argc, char ** argv);

static int64_t g_ld(uint64_t b, int64_t o) { return *(const int64_t *)(uintptr_t)(b + (uint64_t) o); }
static void g_stw(uint64_t b, int64_t o, int64_t v) { *(int64_t *)(uintptr_t)(b + (uint64_t) o) = v; }
static void g_fail(int64_t c);

static int g_started = 0;

// Default on (rule 10); the switch restores the caller's previous behaviour only.
int fg_gr_enabled(void) {
    const char * v = getenv("GGML_GOOSE_GRAPH_REUSE");
    return !(v && strcmp(v, "0") == 0);
}

int64_t fg_gr_check(const int64_t * present, const int64_t * want, const int64_t * have, int64_t n) {
    if (!g_started) {
        fg_graph_reuse_goose_init(0, (char **) 0);
        g_started = 1;
    }
    int64_t packet[3][16];
    int64_t out = -1;
    memset(packet, 0, sizeof packet);
    packet[0][0] = n;
    packet[1][0] = (int64_t) (uintptr_t) present;
    packet[1][1] = (int64_t) (uintptr_t) want;
    packet[1][2] = (int64_t) (uintptr_t) have;
    packet[2][0] = (int64_t) (uintptr_t) &out;
    return gr_run((int64_t) (uintptr_t) packet);
}

static void g_fail(int64_t c) {
    (void) c;
    abort();
}

int64_t grk_ld_i64(uint64_t b, int64_t o) { return g_ld(b, o); }
void    grk_stw(uint64_t b, int64_t o, int64_t v) { g_stw(b, o, v); }
void    grk_fail(int64_t c) { g_fail(c); }
