#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int64_t qd_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float qd_of_bits(int64_t bits) {
    const uint32_t word = (uint32_t)bits;
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

int64_t qd_iq2_grid(int64_t grid, int64_t index) {
    (void)grid;
    (void)index;
    abort();
}

int64_t qd_iq3_grid(int64_t grid, int64_t index) {
    (void)grid;
    (void)index;
    abort();
}
