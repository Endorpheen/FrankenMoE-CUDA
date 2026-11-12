// Differential gate for the batch-compatibility predicate (P8-03).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define fg274b_fail      gate_fail
#define fg274b_ld_i64    gate_ld
#define fg274b_from_bits gate_from_bits
#include "batch_compat-functions.c"

void gate_fail(int64_t c) { fprintf(stderr, "FG274B validation code %lld\n", (long long) c); abort(); }
int64_t gate_ld(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
float gate_from_bits(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint32_t fb(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

static int ref_can(int64_t ta, int64_t tb, int64_t ea, int64_t eb, int64_t da, int64_t db,
                   const int64_t * pa, const float * sa, int na, const int64_t * pb, const float * sb, int nb) {
    if (da || db) return 0;
    if (ta != tb || ea != eb || na != nb) return 0;
    for (int i = 0; i < na; ++i) if (pa[i] != pb[i] || !(sa[i] == sb[i])) return 0;
    return 1;
}

static int failures, checks;
static void expect(const char * nm, int got, int want) {
    checks++;
    if (!!got != want) { failures++; printf("%-30s got %d want %d\n", nm, got, want); }
    else printf("%-30s OK\n", nm);
}

int main(void) {
    const int64_t pa[2] = { 1001, 1002 }, pb[2] = { 1001, 1002 }, pc[2] = { 1001, 9999 };
    const float   sa[2] = { 0.5f, 1.0f },  sb[2] = { 0.5f, 1.0f },  sc[2] = { 0.5f, 0.25f };
    const float   z0[1] = { 0.0f }, zm[1] = { -0.0f }, zn[1] = { 0.0f/0.0f };
    const int64_t p1[1] = { 7 }, p1b[1] = { 7 };

#define RUN(nm, args, want) expect(nm, fg274b_can_batch args, want)

    RUN("identical",      (0,0, 0,0, 0,0, (uint64_t)(uintptr_t)pa,(uint64_t)(uintptr_t)sa,2,(uint64_t)(uintptr_t)pb,(uint64_t)(uintptr_t)sb,2), 1);
    RUN("type differs",   (0,1, 0,0, 0,0, (uint64_t)(uintptr_t)pa,(uint64_t)(uintptr_t)sa,2,(uint64_t)(uintptr_t)pb,(uint64_t)(uintptr_t)sb,2), 0);
    RUN("embd differs",   (0,0, 0,1, 0,0, (uint64_t)(uintptr_t)pa,(uint64_t)(uintptr_t)sa,2,(uint64_t)(uintptr_t)pb,(uint64_t)(uintptr_t)sb,2), 0);
    RUN("decision on one",(0,0, 0,0, 1,0, (uint64_t)(uintptr_t)pa,(uint64_t)(uintptr_t)sa,2,(uint64_t)(uintptr_t)pb,(uint64_t)(uintptr_t)sb,2), 0);
    RUN("decision on other",(0,0,0,0, 0,1, (uint64_t)(uintptr_t)pa,(uint64_t)(uintptr_t)sa,2,(uint64_t)(uintptr_t)pb,(uint64_t)(uintptr_t)sb,2), 0);
    RUN("lora ptr differs",(0,0,0,0, 0,0, (uint64_t)(uintptr_t)pa,(uint64_t)(uintptr_t)sa,2,(uint64_t)(uintptr_t)pc,(uint64_t)(uintptr_t)sc,2), 0);
    RUN("lora scale differs",(0,0,0,0,0,0, (uint64_t)(uintptr_t)pa,(uint64_t)(uintptr_t)sa,2,(uint64_t)(uintptr_t)pb,(uint64_t)(uintptr_t)sc,2), 0);
    RUN("lora length differs",(0,0,0,0,0,0,(uint64_t)(uintptr_t)pa,(uint64_t)(uintptr_t)sa,2,(uint64_t)(uintptr_t)p1,(uint64_t)(uintptr_t)sa,1), 0);
    RUN("signed zero equal",(0,0,0,0,0,0, (uint64_t)(uintptr_t)p1,(uint64_t)(uintptr_t)z0,1,(uint64_t)(uintptr_t)p1b,(uint64_t)(uintptr_t)zm,1), 1);
    RUN("nan not equal",  (0,0,0,0,0,0, (uint64_t)(uintptr_t)p1,(uint64_t)(uintptr_t)zn,1,(uint64_t)(uintptr_t)p1b,(uint64_t)(uintptr_t)zn,1), 0);
    expect("ref nan not equal", ref_can(0,0,0,0,0,0,p1,zn,1,p1b,zn,1), 0);
    printf("checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
