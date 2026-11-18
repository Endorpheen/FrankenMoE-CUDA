// Proves test-runner.goose is not merely generatable: the generated C compiles,
// links on its own, and the exported decisions answer correctly when called from C.
// This is the check P10-09 was missing - "cannot generate standalone C" was recorded
// as a blocker, and nobody had called the result.
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "t.h"

static int checks = 0;
static int fails = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) {
        fails++;
        printf("FAIL %s\n", what);
    }
}

static sl_u8 S(const char *s) {
    sl_u8 v;
    v.data = (uint8_t *) (uintptr_t) s;
    v.len = (int64_t) strlen(s);
    return v;
}

int main(void) {
    // The generated runtime refuses exported calls before its own initialisation,
    // which is a property of the generator the first version of this gate missed.
    goose_init(0, (char **) 0);

    // tr_match: a POSIX-ERE subset - unanchored, ^ anchors, | alternates.
    check(tr_match(S("abc"), S("abc")) == 1, "a literal pattern matches itself");
    check(tr_match(S("abc"), S("abd")) == 0, "a literal pattern does not match a sibling");
    check(tr_match(S(""), S("anything")) == 1, "an empty pattern matches");
    check(tr_match(S("b"), S("abc")) == 1, "an unanchored pattern is searched");
    check(tr_match(S("^b"), S("abc")) == 0, "a ^ pattern is anchored");
    check(tr_match(S("^a"), S("abc")) == 1, "a ^ pattern at the start matches");
    check(tr_match(S("abc|xyz"), S("xyz")) == 1, "the second alternative matches");

    // tr_selected: ctest's -R / -E rules.
    check(tr_selected(S("t1"), S(""), S("")) == 1, "an empty filter selects everything");
    check(tr_selected(S("t1"), S("t2"), S("")) == 0, "a filter that does not match excludes");
    check(tr_selected(S("t1"), S(""), S("t1")) == 0, "an exclude that matches excludes");
    check(tr_selected(S("t1"), S("t1"), S("t2")) == 1, "both filters are consulted");

    // tr_classify: the verdict from the return code, with the fixture rule.
    check(tr_classify(0, 0, 0, 0, 0) == 3, "no return code is Not Run");
    check(tr_classify(0, 1, 0, 0, 1) == 3, "a blocked dependency is Not Run");
    check(tr_classify(0, 1, 0, 0, 0) == 0, "return code 0 is Pass");
    check(tr_classify(77, 1, 77, 1, 0) == 2, "a matching skip code is Skip");
    check(tr_classify(77, 1, 0, 0, 0) == 1, "any other non-zero code is Fail");

    // tr_status: disabled is decided before the command is ever consulted.
    check(tr_status(0, 1, 0, 0, 0, 1) == 4, "a disabled test is Disabled, not Pass");
    check(tr_status(1, 1, 0, 0, 0, 0) == 1, "an enabled failing test is Fail");

    // tr_percent: (100 * good + t / 2) / t over the non-disabled total.
    check(tr_percent(4, 0, 0) == 100, "everything reported and passing is 100");
    check(tr_percent(0, 0, 0) == 0, "nothing reported is 0, not a division fault");
    check(tr_percent(4, 0, 2) == 50, "half of four passing is 50");
    check(tr_percent(2, 2, 0) == 0, "all tests disabled is 0");

    printf("%s checks=%d failures=%d\n", fails == 0 ? "PASS" : "BAD", checks, fails);
    return fails ? 1 : 0;
}
