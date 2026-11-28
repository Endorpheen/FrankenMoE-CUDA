static uint32_t i2m_load_u8(uint64_t address) {
    return *(__global const uint8_t *)address;
}

static int32_t i2m_load_i8(uint64_t address) {
    return *(__global const int8_t *)address;
}

static uint32_t i2m_load_u16(uint64_t address) {
    return *(__global const uint16_t *)address;
}

static float i2m_load_half(uint64_t address) {
    float result;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(result) : "h"((uint16_t)i2m_load_u16(address)));
    return result;
}

static float i2m_load(uint64_t address) {
    return *(__global const float *)address;
}

static void i2m_store(uint64_t address, float value) {
    *(__global float *)address = value;
}

static float i2m_fma(float a, float b, float sum) {
    float result;
    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(result) : "f"(a), "f"(b), "f"(sum));
    return result;
}

static float i2m_shuffle(float value, int32_t distance) {
    float result;
    __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=f"(result) : "f"(value), "r"(distance));
    return result;
}

static uint64_t i2m_job_word(uint64_t jobs, int64_t index) { return ((__global const uint64_t *)jobs)[index]; }

#include "inkling-matrix-device.h"

#include "matrix-image-entries.h"
