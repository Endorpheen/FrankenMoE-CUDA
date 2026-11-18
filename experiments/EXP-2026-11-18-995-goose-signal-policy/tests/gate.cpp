// The signal decision is Goose (goose/server/shutdown-policy.goose), and until this
// cycle the module was in the tree but in no composed unit: a decision nobody could
// reach while the host kept its own "any signal means stop". These checks pin the
// contract the host now relies on.
//
// out is three i64 words written through fg448_st_i64: 0 action (0 nothing, 1 run
// shutdown, 2 force exit), 1 exit code, 2 "second interrupt" message flag.
#include <stdio.h>
#include <stdint.h>
#include "runtime.h"

static int checks = 0;
static int fails = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) {
        fails++;
        printf("FAIL %s\n", what);
    }
}

static int64_t act = -1, code = -1, msg = -1;

static void dispatch(int64_t terminating, int64_t has_handler) {
    int64_t out[3] = { -1, -1, -1 };
    on_signal(terminating, has_handler, (uint64_t) (uintptr_t) out);
    act = out[0];
    code = out[1];
    msg = out[2];
}

int main(void) {
    goose_init(0, (char **) 0);

    dispatch(0, 1);
    check(act == 1, "first interrupt runs the shutdown");
    check(code == 0 && msg == 0, "and sets neither an exit code nor a message");

    dispatch(1, 1);
    check(act == 2, "second interrupt forces exit");
    check(code == 1, "with exit code 1");
    check(msg == 1, "and says so");

    dispatch(0, 0);
    check(act == 0, "no handler means nothing to run");
    check(code == 0 && msg == 0, "and no action is chosen");

    printf("%s checks=%d failures=%d\n", fails == 0 ? "PASS" : "BAD", checks, fails);
    return fails ? 1 : 0;
}
