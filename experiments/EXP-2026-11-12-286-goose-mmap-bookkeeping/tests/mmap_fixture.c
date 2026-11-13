// Differential gate for the mmap/lock bookkeeping decisions (P7-03).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define fg286m_fail   gate_fail
#define fg286m_st_i64 gate_st
#include "mmap_bookkeeping-functions.c"

void gate_fail(int64_t c) { fprintf(stderr, "FG286M validation code %lld\n", (long long) c); abort(); }
void gate_st(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

static int failures, checks;
static void expect(const char * nm, int64_t got, int64_t want) {
    checks++;
    if (got != want) { failures++; printf("%-30s got %lld want %lld\n", nm, (long long) got, (long long) want); }
    else printf("%-30s OK\n", nm);
}

int main(void) {
    expect("lock: locks, not lazy", fg286m_should_lock(1, 0), 1);
    expect("lock: locks, lazy",     fg286m_should_lock(1, 1), 0);
    expect("lock: no locks",        fg286m_should_lock(0, 0), 0);
    expect("lock: neither",         fg286m_should_lock(0, 1), 0);

    int64_t out[2] = {0, 0};
    // empty range sentinel: first = INT64_MAX, second = 0
    fg286m_range_update(INT64_MAX, 0, 100, 10, (uint64_t) (uintptr_t) out);
    expect("range: first from sentinel", out[0], 100);
    expect("range: second", out[1], 110);
    fg286m_range_update(50, 200, 100, 10, (uint64_t) (uintptr_t) out);
    expect("range: first keeps min", out[0], 50);
    expect("range: second keeps max", out[1], 200);
    fg286m_range_update(50, 200, 40, 20, (uint64_t) (uintptr_t) out);
    expect("range: lower first", out[0], 40);
    expect("range: higher end", out[1], 200);
    fg286m_range_update(50, 200, 180, 50, (uint64_t) (uintptr_t) out);
    expect("range: end extends", out[1], 230);
    printf("checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
