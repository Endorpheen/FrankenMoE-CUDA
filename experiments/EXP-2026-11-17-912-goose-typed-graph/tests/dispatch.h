// Test-only redirection of public GGML constructor calls to a Goose module.
#pragma once
#include <stdint.h>
#include <string.h>
int64_t ggt_dispatch(const uint64_t * values);
int ggt_test_enabled(void);
static inline uint64_t fg158_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}
#define FG158_ARG(value) ((uint64_t) (uintptr_t) (value))
#define FG158_CALL(mode,c,a,b,d,op,ip,...) \
    ggt_dispatch((const uint64_t[16]) {FG158_ARG(mode),FG158_ARG(c),FG158_ARG(a),FG158_ARG(b),FG158_ARG(d),FG158_ARG(op),FG158_ARG(ip),__VA_ARGS__})
