// Host-plan fixture for the Goose pad_reflect_1d launcher.
//
// The generated host module owns the length check and the grid computation.
// This fixture supplies the packet and records exactly what the module asks the
// launcher to do, then checks the refusal cases stop before launch.
#include "host-ffi.h"
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstring>

static uint64_t g_packet[16][16];
static uint64_t g_plan[16][16];
static int g_launch_count;
static int64_t g_fail_code;
static jmp_buf g_jmp;

extern "C" {
uint64_t fg413h_get(int64_t p, int64_t k, int64_t i) {
    (void) p; (void) k;
    return g_packet[0][i];
}
int64_t fg413h_plan(int64_t p) { (void) p; return 1; }
void fg413h_put(int64_t p, int64_t k, int64_t i, uint64_t v) {
    (void) p;
    g_plan[k][i] = v;
}
int64_t fg413h_launch(int64_t p) { (void) p; ++g_launch_count; return 0; }
void fg413h_fail(int64_t c) { g_fail_code = c; longjmp(g_jmp, 1); }
}

static int g_checks, g_fails;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) {
        ++g_fails;
        fprintf(stderr, "FAIL %s\n", what);
    }
}

static void reset(void) {
    memset(g_packet, 0, sizeof(g_packet));
    memset(g_plan, 0, sizeof(g_plan));
    g_launch_count = 0;
    g_fail_code = 0;
}

static bool run_case(void) {
    if (setjmp(g_jmp)) {
        return false;
    }
    return fg413h_run(0) != 0;
}

static void fill_valid(int64_t src, int64_t dst, int64_t ne0, int64_t ne00,
                       int64_t ne01, int64_t ne02, int64_t ne03,
                       int64_t p0, int64_t p1) {
    g_packet[0][0] = (uint64_t) src;
    g_packet[0][1] = (uint64_t) dst;
    g_packet[0][2] = (uint64_t) ne0;
    g_packet[0][3] = (uint64_t) ne00;
    g_packet[0][4] = (uint64_t) ne01;
    g_packet[0][5] = (uint64_t) ne02;
    g_packet[0][6] = (uint64_t) ne03;
    g_packet[0][7] = (uint64_t) p0;
    g_packet[0][8] = (uint64_t) p1;
}

int main(void) {
    // 305 columns -> two tiles; 4 rows -> grid.x = 8; y=2, z=3; block 256x1x1.
    reset();
    fill_valid(0x1000, 0x2000, 305, 300, 4, 2, 3, 2, 3);
    check(run_case(), "valid case returns true");
    check(g_launch_count == 1, "valid case launches once");
    check(g_plan[1][0] == 8, "grid.x = ne01 * ceil(ne0/256)");
    check(g_plan[1][1] == 2, "grid.y = ne02");
    check(g_plan[1][2] == 3, "grid.z = ne03");
    check(g_plan[1][3] == 256, "block.x = 256");
    check(g_plan[1][4] == 1, "block.y = 1");
    check(g_plan[1][5] == 1, "block.z = 1");

    // Exactly one tile: ne0 = 256.
    reset();
    fill_valid(0x1000, 0x2000, 256, 256, 3, 1, 1, 0, 0);
    check(run_case(), "single-tile case returns true");
    check(g_plan[1][0] == 3, "single tile -> grid.x = ne01");

    // One column over a tile boundary: ne0 = 257 -> two tiles.
    reset();
    fill_valid(0x1000, 0x2000, 257, 255, 2, 1, 1, 1, 1);
    check(run_case(), "tile-boundary case returns true");
    check(g_plan[1][0] == 4, "257 columns -> two tiles per row");

    // Padded length mismatch must refuse before the launch.
    reset();
    fill_valid(0x1000, 0x2000, 304, 300, 4, 2, 3, 2, 3);
    check(!run_case(), "length mismatch refused");
    check(g_fail_code == 23, "length mismatch code 23");
    check(g_launch_count == 0, "length mismatch does not launch");

    // Zero source extent must refuse.
    reset();
    fill_valid(0x1000, 0x2000, 1, 0, 1, 1, 1, 0, 1);
    check(!run_case(), "zero ne00 refused");
    check(g_fail_code == 21, "zero ne00 code 21");
    check(g_launch_count == 0, "zero ne00 does not launch");

    // Negative padding must refuse.
    reset();
    fill_valid(0x1000, 0x2000, 9, 10, 1, 1, 1, -1, 0);
    check(!run_case(), "negative p0 refused");
    check(g_fail_code == 22, "negative p0 code 22");
    check(g_launch_count == 0, "negative p0 does not launch");

    // Null source pointer must refuse.
    reset();
    fill_valid(0, 0x2000, 8, 8, 1, 1, 1, 0, 0);
    check(!run_case(), "null src refused");
    check(g_fail_code == 20, "null src code 20");
    check(g_launch_count == 0, "null src does not launch");

    printf("FG413H_HOST_PLAN checks=%d failures=%d\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
