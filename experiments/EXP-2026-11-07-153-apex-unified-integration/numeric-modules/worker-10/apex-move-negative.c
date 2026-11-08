/* worker-10 negative corpus: invalid params must be rejected by can_run and
 * the generated Goose entry must trap instead of writing out of bounds. */
#include "apex-move-bridge.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;
static void check(int ok, const char * what) {
    if (!ok) { ++failures; fprintf(stderr, "NEG FAIL %s\n", what); }
}

int main(void) {
    uint8_t src[64] = {0}, dst[64] = {0};
    fg153_am_params p;
    memset(&p, 0, sizeof p);
    p.v[FG153_AM_P_SRC] = (int64_t)(uintptr_t)src;
    p.v[FG153_AM_P_DST] = (int64_t)(uintptr_t)dst;
    p.v[FG153_AM_P_ST] = 0;   /* F32 */
    p.v[FG153_AM_P_DT] = 99;  /* unsupported */
    p.v[FG153_AM_P_NE00] = p.v[FG153_AM_P_NE10] = 1;
    p.v[FG153_AM_P_NE01] = p.v[FG153_AM_P_NE11] = 1;
    p.v[FG153_AM_P_NE02] = p.v[FG153_AM_P_NE12] = 1;
    p.v[FG153_AM_P_NE03] = p.v[FG153_AM_P_NE13] = 1;
    p.v[FG153_AM_P_NB10] = 4;
    p.v[FG153_AM_P_N] = 1;

    check(fg153_am_can_run(FG153_AM_OP_COPY_SCALAR, &p) == 0, "reject unsupported dst type");
    check(fg153_am_can_run(FG153_AM_OP_COPY_BLOCKS, &p) == 0, "reject non-Q4 block op");
    check(fg153_am_can_run(FG153_AM_OP_GETROWS, &p) == 0, "reject unsupported getrows");
    check(fg153_am_can_run(FG153_AM_OP_SETROWS, &p) == 0, "reject unsupported setrows");

    pid_t pid = fork();
    if (pid == 0) {
        fg153_am_call(FG153_AM_OP_COPY_SCALAR, 1, (int64_t)(uintptr_t)&p);
        _exit(0);
    }
    int st = 0;
    check(waitpid(pid, &st, 0) == pid, "wait child");
    check(!(WIFEXITED(st) && WEXITSTATUS(st) == 0), "Goose entry traps on unsupported type");

    p.v[FG153_AM_P_DT] = 0; /* F32 */
    p.v[FG153_AM_P_NE00] = p.v[FG153_AM_P_NE10] = 32;
    p.v[FG153_AM_P_NB10] = 4;
    p.v[FG153_AM_P_N] = 31;
    p.v[FG153_AM_P_ST] = 0;
    p.v[FG153_AM_P_DT] = 2; /* Q4_0 */
    check(fg153_am_can_run(FG153_AM_OP_COPY_BLOCKS, &p) == 0, "reject non-multiple Q4 length");

    printf("{\"status\":\"%s\",\"negative_cases\":4}\n", failures ? "FAIL" : "PASS_APEX_MOVE_NEGATIVE");
    return failures ? 1 : 0;
}
