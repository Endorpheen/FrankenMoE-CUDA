// Host-plan fixture for the Goose reduce_rows launcher.
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
uint64_t fg414h_get(int64_t p, int64_t k, int64_t i) { (void) p; (void) k; return g_packet[0][i]; }
int64_t fg414h_plan(int64_t p) { (void) p; return 1; }
void fg414h_put(int64_t p, int64_t k, int64_t i, uint64_t v) { (void) p; g_plan[k][i] = v; }
int64_t fg414h_launch(int64_t p) { (void) p; ++g_launch_count; return 0; }
void fg414h_fail(int64_t c) { g_fail_code = c; longjmp(g_jmp, 1); }
}

static int g_checks, g_fails;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fails; fprintf(stderr, "FAIL %s\n", what); }
}
static void reset(void) {
    memset(g_packet, 0, sizeof(g_packet));
    memset(g_plan, 0, sizeof(g_plan));
    g_launch_count = 0;
    g_fail_code = 0;
}
static bool run_case(void) {
    if (setjmp(g_jmp)) { return false; }
    return fg414h_run(0) != 0;
}
static void fill(int64_t src, int64_t dst, int64_t ncols, int64_t nrows, int64_t nsm,
                 int64_t strided, int64_t ne1, int64_t ne2) {
    g_packet[0][0] = (uint64_t) src;
    g_packet[0][1] = (uint64_t) dst;
    g_packet[0][2] = (uint64_t) ncols;
    g_packet[0][3] = (uint64_t) nrows;
    g_packet[0][4] = (uint64_t) nsm;
    g_packet[0][5] = (uint64_t) strided;
    g_packet[0][6] = 0;
    g_packet[0][7] = (uint64_t) ne1;
    g_packet[0][8] = (uint64_t) ne2;
}

int main(void) {
    reset();
    fill(0x1000, 0x2000, 1, 1, 108, 0, 1, 1);
    check(run_case(), "one row returns true");
    check(g_launch_count == 1, "one row launches once");
    check(g_plan[1][0] == 1 && g_plan[1][3] == 512, "one row -> 512 threads");

    reset();
    fill(0x1000, 0x2000, 1000, 500, 100, 0, 1, 1);
    check(run_case(), "many narrow rows return true");
    check(g_plan[1][0] == 500 && g_plan[1][3] == 32, "nrows/nsm >= 2, ncols<1024 -> 32");

    reset();
    fill(0x1000, 0x2000, 1024, 500, 100, 0, 1, 1);
    check(run_case(), "many wide rows return true");
    check(g_plan[1][0] == 500 && g_plan[1][3] == 128, "nrows/nsm >= 2, ncols>=1024 -> 128");

    reset();
    fill(0x1000, 0x2000, 2048, 199, 100, 1, 199, 1);
    check(run_case(), "just below the ratio boundary returns true");
    check(g_plan[1][3] == 512, "nrows/nsm < 2 -> 512");

    reset();
    fill(0x1000, 0x2000, 2048, 200, 100, 1, 200, 1);
    check(run_case(), "ratio boundary returns true");
    check(g_plan[1][3] == 128, "nrows/nsm == 2 -> heuristic");

    reset();
    fill(0x1000, 0x2000, 100, 0, 108, 0, 1, 1);
    check(!run_case() && g_fail_code == 21 && g_launch_count == 0, "zero rows refused");

    reset();
    fill(0x1000, 0x2000, 0, 10, 108, 0, 1, 1);
    check(!run_case() && g_fail_code == 21 && g_launch_count == 0, "zero columns refused");

    reset();
    fill(0x1000, 0x2000, 100, 10, 0, 0, 1, 1);
    check(!run_case() && g_fail_code == 21 && g_launch_count == 0, "zero SM count refused");

    reset();
    fill(0x1000, 0x2000, 100, 10, 108, 1, 0, 1);
    check(!run_case() && g_fail_code == 22 && g_launch_count == 0, "zero ne1 refused");

    reset();
    fill(0, 0x2000, 100, 10, 108, 0, 1, 1);
    check(!run_case() && g_fail_code == 20 && g_launch_count == 0, "null src refused");

    printf("FG414H_HOST_PLAN checks=%d failures=%d\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
