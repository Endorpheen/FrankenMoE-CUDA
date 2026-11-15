// P10-08 S3: the text side of the public ABI.
//
// Native sources at the base commit:
//   src/llama-model-loader.cpp  llama_ftype_name                     31-76
//   src/llama.cpp               llama_flash_attn_type_name           37-47
//                               llama_load_mode_name                 49-63
//                               llama_load_mode_from_str             65-74
//   src/llama-model.cpp         llama_model_meta_key_str        2992-3007
//   ggml/src/ggml.c             ggml_status_to_string              437-446
//
// These are exported functions whose result is a string, so the exact bytes are
// part of the ABI: a caller parses them ("Q4_K - Medium"), prints them into a log
// or a JSON field, or feeds them back through the matching parser. The tables
// are transcribed once from those functions and frozen here; the gate compiles
// the same tables out of the sources on every build.
//
// Rules that are reproduced rather than tabulated:
//   - llama_ftype_name masks the ftype with ~LLAMA_FTYPE_GUESSED (1024), looks
//     the result up, and keeps the "(guessed) " prefix only when the bit was
//     set;
//   - llama_load_mode_from_str matches whole strings with strcmp, so the match is
//     case sensitive and accepts nothing else; native throws
//     std::invalid_argument, which cannot cross this ABI, so an unknown string
//     returns NM_PARSE_UNKNOWN;
//   - llama_flash_attn_type_name and llama_load_mode_name abort on an unknown
//     value in native; the port returns ABI-style -1 for the same input;
//   - llama_model_meta_key_str returns nullptr for an unknown key, modelled as
//     -1 from nm_meta_key_str;
//   - ggml_status_to_string has a catch-all text for any other value.
//
// All functions write into a caller buffer and return the number of bytes
// written, so the port never hands out a pointer into static storage.
//
// The prefix is nm_.

import std;

const NM_NONE: i64 = 0 - 1;

// llama_load_mode_from_str on an unknown string: native throws, this ABI has no
// exception channel.
const NM_PARSE_UNKNOWN: i64 = 0 - 99;
const NM_FTYPE_GUESSED: i64 = 1024;

const NM_FTYPE_N: i64 = 36;
const NM_FTYPE_VAL: i64[36] = [
    0,
    1,
    32,
    40,
    41,
    2,
    3,
    8,
    9,
    7,
    38,
    39,
    10,
    21,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    36,
    37,
    19,
    20,
    28,
    29,
    22,
    23,
    24,
    31,
    25,
    30,
    26,
    27
];
const NM_FTYPE_TXT: const u8[:][36] = [
    "all F32",
    "F16",
    "BF16",
    "Q1_0",
    "Q2_0",
    "Q4_0",
    "Q4_1",
    "Q5_0",
    "Q5_1",
    "Q8_0",
    "MXFP4 MoE",
    "NVFP4",
    "Q2_K - Medium",
    "Q2_K - Small",
    "Q3_K - Small",
    "Q3_K - Medium",
    "Q3_K - Large",
    "Q4_K - Small",
    "Q4_K - Medium",
    "Q5_K - Small",
    "Q5_K - Medium",
    "Q6_K",
    "TQ1_0 - 1.69 bpw ternary",
    "TQ2_0 - 2.06 bpw ternary",
    "IQ2_XXS - 2.0625 bpw",
    "IQ2_XS - 2.3125 bpw",
    "IQ2_S - 2.5 bpw",
    "IQ2_M - 2.7 bpw",
    "IQ3_XS - 3.3 bpw",
    "IQ3_XXS - 3.0625 bpw",
    "IQ1_S - 1.5625 bpw",
    "IQ1_M - 1.75 bpw",
    "IQ4_NL - 4.5 bpw",
    "IQ4_XS - 4.25 bpw",
    "IQ3_S - 3.4375 bpw",
    "IQ3_S mix - 3.66 bpw"
];
const NM_FTYPE_DEFAULT: const u8[:] = "unknown, may not work";
const NM_FTYPE_PREFIX: const u8[:] = "(guessed) ";

const NM_LOAD_N: i64 = 6;
const NM_LOAD_VAL: i64[6] = [
    -1,
    0,
    1,
    2,
    3,
    4
];
const NM_LOAD_TXT: const u8[:][6] = [
    "auto",
    "none",
    "mmap",
    "mlock",
    "mmap+mlock",
    "dio"
];

const NM_FROM_STR_N: i64 = 6;
const NM_FROM_STR_TXT: const u8[:][6] = [
    "auto",
    "none",
    "mmap",
    "mlock",
    "mmap+mlock",
    "dio"
];
const NM_FROM_STR_VAL: i64[6] = [
    -1,
    0,
    1,
    2,
    3,
    4
];

const NM_FLASH_N: i64 = 3;
const NM_FLASH_VAL: i64[3] = [
    -1,
    0,
    1
];
const NM_FLASH_TXT: const u8[:][3] = [
    "auto",
    "disabled",
    "enabled"
];

const NM_META_N: i64 = 12;
const NM_META_VAL: i64[12] = [
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11
];
const NM_META_TXT: const u8[:][12] = [
    "general.sampling.sequence",
    "general.sampling.top_k",
    "general.sampling.top_p",
    "general.sampling.min_p",
    "general.sampling.xtc_probability",
    "general.sampling.xtc_threshold",
    "general.sampling.temp",
    "general.sampling.penalty_last_n",
    "general.sampling.penalty_repeat",
    "general.sampling.mirostat",
    "general.sampling.mirostat_tau",
    "general.sampling.mirostat_eta"
];

const NM_STATUS_N: i64 = 4;
const NM_STATUS_VAL: i64[4] = [
    -2,
    -1,
    0,
    1
];
const NM_STATUS_TXT: const u8[:][4] = [
    "GGML status: error (failed to allocate memory)",
    "GGML status: error (operation failed)",
    "GGML status: success",
    "GGML status: warning (operation aborted)"
];
const NM_STATUS_DEFAULT: const u8[:] = "GGML status: unknown";

fn nm_put(s: const u8[:], out: u8[:], at0: i64) -> i64 {
    var at: i64 = at0;
    var i: i64 = 0;
    while i < s.len && at < out.len {
        out[at] = s[i];
        at = at + 1;
        i = i + 1;
    }
    return at;
}

// Index into a (value, text) table, or -1.
fn nm_find(values: const i64[:], n: i64, key: i64) -> i64 {
    var i: i64 = 0;
    while i < n && i < values.len {
        if values[i] == key {
            return i;
        }
        i = i + 1;
    }
    return NM_NONE;
}

// llama_ftype_name, including the "(guessed) " prefix rule.
export fn nm_ftype_name(ftype: i64, out: u8[:]) -> i64 {
    let masked: i64 = ftype & (0 - (NM_FTYPE_GUESSED + 1));
    let idx: i64 = nm_find(NM_FTYPE_VAL, NM_FTYPE_N, masked);
    var name: const u8[:] = NM_FTYPE_DEFAULT;
    if idx >= 0 {
        name = NM_FTYPE_TXT[idx];
    }
    var at: i64 = 0;
    if (ftype & NM_FTYPE_GUESSED) != 0 {
        at = nm_put(NM_FTYPE_PREFIX, out, at);
    }
    return nm_put(name, out, at);
}

export fn nm_load_mode_name(mode: i64, out: u8[:]) -> i64 {
    let idx: i64 = nm_find(NM_LOAD_VAL, NM_LOAD_N, mode);
    if idx < 0 {
        return NM_NONE;
    }
    return nm_put(NM_LOAD_TXT[idx], out, 0);
}

export fn nm_load_mode_from_str(s: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < NM_FROM_STR_N {
        if NM_FROM_STR_TXT[i] == s {
            return NM_FROM_STR_VAL[i];
        }
        i = i + 1;
    }
    return NM_PARSE_UNKNOWN;
}

export fn nm_flash_attn_type_name(mode: i64, out: u8[:]) -> i64 {
    let idx: i64 = nm_find(NM_FLASH_VAL, NM_FLASH_N, mode);
    if idx < 0 {
        return NM_NONE;
    }
    return nm_put(NM_FLASH_TXT[idx], out, 0);
}

export fn nm_meta_key_str(key: i64, out: u8[:]) -> i64 {
    let idx: i64 = nm_find(NM_META_VAL, NM_META_N, key);
    if idx < 0 {
        return NM_NONE;
    }
    return nm_put(NM_META_TXT[idx], out, 0);
}

export fn nm_status_text(status: i64, out: u8[:]) -> i64 {
    let idx: i64 = nm_find(NM_STATUS_VAL, NM_STATUS_N, status);
    if idx < 0 {
        return nm_put(NM_STATUS_DEFAULT, out, 0);
    }
    return nm_put(NM_STATUS_TXT[idx], out, 0);
}
