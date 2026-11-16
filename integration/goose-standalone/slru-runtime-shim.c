// Host side of the SLRU policy module (EXP-756): the FFI its generated C calls, and nothing else.
//
//   slru_ld_i32(base, byte)     read  an int32 at base+byte
//   slru_st_i32(base, byte, v)  write an int32 at base+byte
//   slru_fail(code)             a policy invariant was violated
//
// The Goose runtime (slru-runtime.o) already provides gs_panic, gs_idxfail, gs_divfail and gs_rt_start,
// so this file must NOT define them - it did once and the link failed with three duplicate symbols. It
// only adds what the runtime cannot know: the load/store pair and the validation hook, the same three the
// fg263c bridge defines for its own module.
//
// A constructor starts the runtime: the emitted policy panics unless goose_init ran, and libllama has no
// other Goose entry point.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int64_t slru_ld_i32(uint64_t b, int64_t byte) {
    int32_t v;
    memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) byte), 4);
    return v;
}

void slru_st_i32(uint64_t b, int64_t byte, int64_t v) {
    int32_t x = (int32_t) v;
    memcpy((void *) (uintptr_t) (b + (uint64_t) byte), &x, 4);
}

void slru_fail(int64_t c) {
    fprintf(stderr, "SLRU validation code %lld\n", (long long) c);
    abort();
}

void goose_init(int argc, char **argv);

__attribute__((constructor)) static void slru_goose_init(void) {
    goose_init(0, (char **) 0);
}
