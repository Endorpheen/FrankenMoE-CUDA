// OpenCL C prelude for a Fried Goose device fragment.
//
// The generated C targets a C11 host. Supply the types, macros and numeric
// operations needed when the device fragment is compiled separately:
//
//   * fixed-width typedefs, because OpenCL C has no <stdint.h>;
//   * GG_ALIGN1, GG_UNALIGNED, _Alignas, UINT64_C, because OpenCL rejects an
//     alignment below the private-address-space minimum (16 bytes) and does not
//     define the C99 integer macros;
//   * checked arithmetic and indexing with the same semantics as the host.
//
// Include this file before the fragment. Nothing here changes the host build.

typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
// OpenCL C makes `long` exactly 64 bits and `long long` 128 bits on this
// target, so the fixed-width typedefs must use `long`.
typedef long int64_t;
typedef unsigned long uint64_t;
typedef unsigned long size_t;

// The generated C already defines the alignment shims for OpenCL and for C11;
// a prelude that includes an older generated file still needs them here.
#ifndef GG_ALIGN1
#define GG_ALIGN1
#define GG_UNALIGNED
#define _Alignas(x)
#endif
#define UINT64_C(x) x##UL
#define INT64_C(x) x##L
#define UINT32_C(x) x##U
#define INT32_C(x) x
#define memset(dst, value, count) __builtin_memset(dst, value, count)

static void gg_device_trap(void) {
    __asm__ volatile("trap;");
    __builtin_unreachable();
}

static void gg_fail(const char *text) {
    gg_device_trap();
}

static int64_t gg_div(int64_t a, int64_t b) {
    if (!b || (a == (-INT64_C(9223372036854775807) - 1) && b == -1)) gg_device_trap();
    return a / b;
}

static int64_t gg_mod(int64_t a, int64_t b) {
    if (!b) gg_device_trap();
    if (b == -1) return 0;
    const int64_t remainder = a % b;
    if (remainder >= 0) return remainder;
    const uint64_t modulus = b < 0 ? (uint64_t)0 - (uint64_t)b : (uint64_t)b;
    return (int64_t)((uint64_t)remainder + modulus);
}

static uint64_t gg_udiv(uint64_t a, uint64_t b) {
    if (!b) gg_device_trap();
    return a / b;
}

static uint64_t gg_umod(uint64_t a, uint64_t b) {
    if (!b) gg_device_trap();
    return a % b;
}

static int64_t gg_index(int64_t index, int64_t len) {
    if ((uint64_t)index >= (uint64_t)len) gg_device_trap();
    return index;
}

static double gg_double_bits(uint64_t bits) {
    return as_double(bits);
}

typedef struct { void *data; int64_t len; } gg_slice;

static gg_slice gg_view(void *data, int64_t len) {
    return (gg_slice){ data, len };
}
