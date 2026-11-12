// Differential gate for the multi-shard file-name rule (P7-03).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define fg284p_fail   gate_fail
#define fg284p_ld_u8  gate_ld8
#include "shard_prefix-functions.c"

void gate_fail(int64_t c) { fprintf(stderr, "FG284P validation code %lld\n", (long long) c); abort(); }
int64_t gate_ld8(uint64_t b, int64_t i) { return *(const uint8_t *) (uintptr_t) (b + (uint64_t) i); }

// native transcription: snprintf postfix + suffix compare, returns prefix length or -1 (native returns 0)
static int64_t ref(const char * path, int split_no, int split_count) {
    char postfix[32];
    snprintf(postfix, sizeof postfix, "-%05d-of-%05d.gguf", split_no + 1, split_count);
    const size_t plen = strlen(path), slen = strlen(postfix);
    if (plen <= slen) return -1;
    const size_t size_prefix = plen - slen;
    if (strcmp(path + size_prefix, postfix) != 0) return -1;
    return (int64_t) size_prefix;
}

static int failures, checks;
static void expect(const char * nm, const char * path, int no, int cnt) {
    const int64_t g = fg284p_split_prefix((uint64_t) (uintptr_t) path, (int64_t) strlen(path), no, cnt);
    const int64_t r = ref(path, no, cnt);
    checks++;
    if (g != r) { failures++; printf("%-34s goose=%lld native=%lld\n", nm, (long long) g, (long long) r); }
    else printf("%-34s OK (%lld)\n", nm, (long long) g);
}

int main(void) {
    expect("match 2 of 4",        "model-00002-of-00004.gguf", 1, 4);
    expect("match 1 of 4",        "model-00001-of-00004.gguf", 0, 4);
    expect("wrong count",         "model-00002-of-00004.gguf", 1, 3);
    expect("wrong index",         "model-00002-of-00004.gguf", 0, 4);
    expect("no postfix",          "model.gguf",                0, 1);
    expect("too short",           "-00001-of-00001.gguf",      0, 1);
    expect("directories in path", "/a/b/model-00003-of-00008.gguf", 2, 8);
    expect("six-digit count",     "m-00001-of-100000.gguf",    0, 100000);
    expect("six-digit index",     "m-100000-of-100000.gguf",   99999, 100000);
    printf("checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
