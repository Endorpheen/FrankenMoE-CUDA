// P10-08 S1: the ggml quantization type table, as the public ABI exposes it.
//
// Native sources at the base commit:
//   ggml/src/ggml.c         type_traits[GGML_TYPE_COUNT]            lines 631-945
//                           ggml_blck_size                1326-1330
//                           ggml_type_size                1332-1336
//                           ggml_row_size                 1338-1343
//                           ggml_type_sizef               1345-1349
//                           ggml_type_name                1351-1355
//                           ggml_is_quantized             1357-1361
//   ggml/include/ggml.h     enum ggml_type                             389-434
//   ggml/src/ggml-common.h  QK* block sizes, block_* struct definitions
//
// This is the part of the ABI that decides file layout: the block size and the
// byte size of one block of every type, plus the name a consumer sees and
// whether the type is quantized. A tensor's nb[0] is ggml_type_size(type) and
// nb[1] follows ne[0]/ggml_blck_size(type), so these four facts per type are
// the whole contract between a writer and a reader of a GGUF file.
//
// The table below is a transcription of the committed native table, slot by
// slot, in enum order. It was produced once from the base-commit sources by
// tests/gen_port_tables.py and is frozen here as literals; the differential gate
// re-derives the native side from ggml.c on every build, so any drift on either
// side fails the gate instead of agreeing with a stale copy.
//
// The prefix is abi_.

import std;

// Returned for every input the native accessors cannot answer: an out-of-range
// type (native asserts) or a division by a zero block size (native asserts).
const ABI_NONE: i64 = 0 - 1;

const ABI_COUNT: i64 = 43;

// ggml_type_name(type). Two removed slots carry the literal "DEPRECATED", three
// removed slots carry the "REMOVED, use ... with runtime repacking" text: those
// strings are what a consumer enumerating the type space still sees.
const AB_NAME: const u8[:][43] = [
    "f32",
    "f16",
    "q4_0",
    "q4_1",
    "DEPRECATED",
    "DEPRECATED",
    "q5_0",
    "q5_1",
    "q8_0",
    "q8_1",
    "q2_K",
    "q3_K",
    "q4_K",
    "q5_K",
    "q6_K",
    "q8_K",
    "iq2_xxs",
    "iq2_xs",
    "iq3_xxs",
    "iq1_s",
    "iq4_nl",
    "iq3_s",
    "iq2_s",
    "iq4_xs",
    "i8",
    "i16",
    "i32",
    "i64",
    "f64",
    "iq1_m",
    "bf16",
    "TYPE_Q4_0_4_4 REMOVED, use Q4_0 with runtime repacking",
    "TYPE_Q4_0_4_8 REMOVED, use Q4_0 with runtime repacking",
    "TYPE_Q4_0_8_8 REMOVED, use Q4_0 with runtime repacking",
    "tq1_0",
    "tq2_0",
    "TYPE_IQ4_NL_4_4 REMOVED, use IQ4_NL with runtime repacking",
    "TYPE_IQ4_NL_4_8 REMOVED, use IQ4_NL with runtime repacking",
    "TYPE_IQ4_NL_8_8 REMOVED, use IQ4_NL with runtime repacking",
    "mxfp4",
    "nvfp4",
    "q1_0",
    "q2_0"
];

// ggml_blck_size(type).
const AB_BLCK: i64[43] = [
    1, // 0 f32
    1, // 1 f16
    32, // 2 q4_0
    32, // 3 q4_1
    0, // 4 DEPRECATED
    0, // 5 DEPRECATED
    32, // 6 q5_0
    32, // 7 q5_1
    32, // 8 q8_0
    32, // 9 q8_1
    256, // 10 q2_K
    256, // 11 q3_K
    256, // 12 q4_K
    256, // 13 q5_K
    256, // 14 q6_K
    256, // 15 q8_K
    256, // 16 iq2_xxs
    256, // 17 iq2_xs
    256, // 18 iq3_xxs
    256, // 19 iq1_s
    32, // 20 iq4_nl
    256, // 21 iq3_s
    256, // 22 iq2_s
    256, // 23 iq4_xs
    1, // 24 i8
    1, // 25 i16
    1, // 26 i32
    1, // 27 i64
    1, // 28 f64
    256, // 29 iq1_m
    1, // 30 bf16
    0, // 31 TYPE_Q4_0_4_4 REMOVED, use Q4_0 with runtime repacking
    0, // 32 TYPE_Q4_0_4_8 REMOVED, use Q4_0 with runtime repacking
    0, // 33 TYPE_Q4_0_8_8 REMOVED, use Q4_0 with runtime repacking
    256, // 34 tq1_0
    256, // 35 tq2_0
    0, // 36 TYPE_IQ4_NL_4_4 REMOVED, use IQ4_NL with runtime repacking
    0, // 37 TYPE_IQ4_NL_4_8 REMOVED, use IQ4_NL with runtime repacking
    0, // 38 TYPE_IQ4_NL_8_8 REMOVED, use IQ4_NL with runtime repacking
    32, // 39 mxfp4
    64, // 40 nvfp4
    128, // 41 q1_0
    64, // 42 q2_0
];

// ggml_type_size(type).
const AB_SIZE: i64[43] = [
    4, // 0 f32
    2, // 1 f16
    18, // 2 q4_0
    20, // 3 q4_1
    0, // 4 DEPRECATED
    0, // 5 DEPRECATED
    22, // 6 q5_0
    24, // 7 q5_1
    34, // 8 q8_0
    36, // 9 q8_1
    84, // 10 q2_K
    110, // 11 q3_K
    144, // 12 q4_K
    176, // 13 q5_K
    210, // 14 q6_K
    292, // 15 q8_K
    66, // 16 iq2_xxs
    74, // 17 iq2_xs
    98, // 18 iq3_xxs
    50, // 19 iq1_s
    18, // 20 iq4_nl
    110, // 21 iq3_s
    82, // 22 iq2_s
    136, // 23 iq4_xs
    1, // 24 i8
    2, // 25 i16
    4, // 26 i32
    8, // 27 i64
    8, // 28 f64
    56, // 29 iq1_m
    2, // 30 bf16
    0, // 31 TYPE_Q4_0_4_4 REMOVED, use Q4_0 with runtime repacking
    0, // 32 TYPE_Q4_0_4_8 REMOVED, use Q4_0 with runtime repacking
    0, // 33 TYPE_Q4_0_8_8 REMOVED, use Q4_0 with runtime repacking
    54, // 34 tq1_0
    66, // 35 tq2_0
    0, // 36 TYPE_IQ4_NL_4_4 REMOVED, use IQ4_NL with runtime repacking
    0, // 37 TYPE_IQ4_NL_4_8 REMOVED, use IQ4_NL with runtime repacking
    0, // 38 TYPE_IQ4_NL_8_8 REMOVED, use IQ4_NL with runtime repacking
    17, // 39 mxfp4
    36, // 40 nvfp4
    18, // 41 q1_0
    18, // 42 q2_0
];

// ggml_is_quantized(type).
const AB_ISQ: i64[43] = [
    0, // 0 f32
    0, // 1 f16
    1, // 2 q4_0
    1, // 3 q4_1
    0, // 4 DEPRECATED
    0, // 5 DEPRECATED
    1, // 6 q5_0
    1, // 7 q5_1
    1, // 8 q8_0
    1, // 9 q8_1
    1, // 10 q2_K
    1, // 11 q3_K
    1, // 12 q4_K
    1, // 13 q5_K
    1, // 14 q6_K
    1, // 15 q8_K
    1, // 16 iq2_xxs
    1, // 17 iq2_xs
    1, // 18 iq3_xxs
    1, // 19 iq1_s
    1, // 20 iq4_nl
    1, // 21 iq3_s
    1, // 22 iq2_s
    1, // 23 iq4_xs
    0, // 24 i8
    0, // 25 i16
    0, // 26 i32
    0, // 27 i64
    0, // 28 f64
    1, // 29 iq1_m
    0, // 30 bf16
    0, // 31 TYPE_Q4_0_4_4 REMOVED, use Q4_0 with runtime repacking
    0, // 32 TYPE_Q4_0_4_8 REMOVED, use Q4_0 with runtime repacking
    0, // 33 TYPE_Q4_0_8_8 REMOVED, use Q4_0 with runtime repacking
    1, // 34 tq1_0
    1, // 35 tq2_0
    0, // 36 TYPE_IQ4_NL_4_4 REMOVED, use IQ4_NL with runtime repacking
    0, // 37 TYPE_IQ4_NL_4_8 REMOVED, use IQ4_NL with runtime repacking
    0, // 38 TYPE_IQ4_NL_8_8 REMOVED, use IQ4_NL with runtime repacking
    1, // 39 mxfp4
    1, // 40 nvfp4
    1, // 41 q1_0
    1, // 42 q2_0
];

fn ab_range(t: i64) -> bool {
    return t >= 0 && t < ABI_COUNT;
}

fn ab_copy(s: const u8[:], out: u8[:]) -> i64 {
    var i: i64 = 0;
    while i < s.len && i < out.len {
        out[i] = s[i];
        i = i + 1;
    }
    return i;
}

export fn abi_type_count() -> i64 {
    return ABI_COUNT;
}

export fn abi_blck_size(t: i64) -> i64 {
    if !ab_range(t) {
        return ABI_NONE;
    }
    return AB_BLCK[t];
}

export fn abi_type_size(t: i64) -> i64 {
    if !ab_range(t) {
        return ABI_NONE;
    }
    return AB_SIZE[t];
}

export fn abi_is_quantized(t: i64) -> i64 {
    if !ab_range(t) {
        return ABI_NONE;
    }
    return AB_ISQ[t];
}

// Length of the name written, or ABI_NONE when the type has no name.
export fn abi_type_name(t: i64, out: u8[:]) -> i64 {
    if !ab_range(t) {
        return ABI_NONE;
    }
    if AB_NAME[t].len == 0 {
        return ABI_NONE;
    }
    return ab_copy(AB_NAME[t], out);
}

// ggml_row_size(type, ne) = type_size * ne / blck_size, with the native
// multiplication before the division. The native function asserts that ne is a
// multiple of the block size; a caller that breaks the assertion is refused
// here with ABI_NONE instead of being truncated silently.
export fn abi_row_size(t: i64, ne: i64) -> i64 {
    if !ab_range(t) {
        return ABI_NONE;
    }
    let blck: i64 = AB_BLCK[t];
    if blck <= 0 || ne < 0 || ne % blck != 0 {
        return ABI_NONE;
    }
    return AB_SIZE[t] * ne / blck;
}

// ggml_nbytes of a contiguous 2-D tensor: one row of ne0 elements, ne1 rows.
export fn abi_nbytes_2d(t: i64, ne0: i64, ne1: i64) -> i64 {
    let row: i64 = abi_row_size(t, ne0);
    if row < 0 || ne1 < 0 {
        return ABI_NONE;
    }
    return row * ne1;
}
