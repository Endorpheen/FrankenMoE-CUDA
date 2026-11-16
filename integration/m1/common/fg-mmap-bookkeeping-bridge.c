#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static void mmap_fail(int64_t code) {
    fprintf(stderr, "FG286M validation code %lld\n", (long long) code);
    abort();
}

static void mmap_store(uint64_t base, int64_t offset, int64_t value) {
    memcpy((void *) (uintptr_t) (base + (uint64_t) offset), &value, sizeof(value));
}

#define fg286m_fail mmap_fail
#define fg286m_st_i64 mmap_store
#include "../../../experiments/EXP-2026-11-12-286-goose-mmap-bookkeeping/source/mmap_bookkeeping-functions.c"
