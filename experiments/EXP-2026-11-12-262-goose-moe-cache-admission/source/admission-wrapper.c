#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void my_fail(int64_t c){ fprintf(stderr, "FG262A validation code %lld\n", (long long) c); abort(); }
static int64_t my_ld(uint64_t b, int64_t i){ int64_t v; memcpy(&v, (void*)(uintptr_t)(b+(uint64_t)i), 8); return v; }
static void my_st(uint64_t b, int64_t i, int64_t v){ memcpy((void*)(uintptr_t)(b+(uint64_t)i), &v, 8); }
#define fg262a_fail my_fail
#define fg262a_ld_i64 my_ld
#define fg262a_st_i64 my_st
#include "admission-functions.c"
