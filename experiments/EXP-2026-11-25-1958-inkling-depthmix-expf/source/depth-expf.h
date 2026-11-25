/*
 * Adapted from the range reduction and exp2 table of Arm optimized-routines.
 * Copyright (c) 2017-2026, Arm Limited.
 * SPDX-License-Identifier: MIT
 *
 * https://github.com/ARM-software/optimized-routines/blob/master/math/expf.c
 * https://github.com/ARM-software/optimized-routines/blob/master/math/exp2f_data.c
 */
__constant ulong depth_exp2[32] = {
    0x3ff0000000000000UL, 0x3fefd9b0d3158574UL, 0x3fefb5586cf9890fUL, 0x3fef9301d0125b51UL,
    0x3fef72b83c7d517bUL, 0x3fef54873168b9aaUL, 0x3fef387a6e756238UL, 0x3fef1e9df51fdee1UL,
    0x3fef06fe0a31b715UL, 0x3feef1a7373aa9cbUL, 0x3feedea64c123422UL, 0x3feece086061892dUL,
    0x3feebfdad5362a27UL, 0x3feeb42b569d4f82UL, 0x3feeab07dd485429UL, 0x3feea47eb03a5585UL,
    0x3feea09e667f3bcdUL, 0x3fee9f75e8ec5f74UL, 0x3feea11473eb0187UL, 0x3feea589994cce13UL,
    0x3feeace5422aa0dbUL, 0x3feeb737b0cdc5e5UL, 0x3feec49182a3f090UL, 0x3feed503b23e255dUL,
    0x3feee89f995ad3adUL, 0x3feeff76f2fb5e47UL, 0x3fef199bdd85529cUL, 0x3fef3720dcef9069UL,
    0x3fef5818dcfba487UL, 0x3fef7c97337b9b5fUL, 0x3fefa4afa2a490daUL, 0x3fefd0765b6e4540UL
};

static double depth_fma(double a, double b, double c) {
    double result;
    __asm__("fma.rn.f64 %0, %1, %2, %3;" : "=d"(result) : "d"(a), "d"(b), "d"(c));
    return result;
}

// Match x86 FMA libm for nonpositive DepthMix score differences.
static float depth_expf(float x) {
    if (x < -0x1.9fe368p6f) return 0.0f;
    const double multiplier = 0x1.71547652b82fep+5;
    const double shift = 0x1.8p52;
    const double rounded = depth_fma((double)x, multiplier, shift);
    const ulong encoded = as_ulong(rounded);
    const double residue = depth_fma((double)x, multiplier, shift - rounded);
    const double high = depth_fma(0x1.c6af84b912394p-20, residue, 0x1.ebfce50fac4f3p-13);
    const double low = depth_fma(0x1.62e42ff0c52d6p-6, residue, 1.0);
    const double scale = as_double(depth_exp2[encoded & 31] + (encoded << 47));
    return (float)(depth_fma(high, residue * residue, low) * scale);
}
