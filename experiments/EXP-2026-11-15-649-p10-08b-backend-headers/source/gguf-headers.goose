// P10-08 S5: the GGUF public header surface, frozen.
//
// Native sources at the base commit (upstream/llama.cpp 0b5be7e4a):
//   ggml/include/gguf.h:41-46       GGUF_MAGIC, GGUF_VERSION, the alignment key,
//                                   GGUF_DEFAULT_ALIGNMENT
//   ggml/include/gguf.h:53-68       enum gguf_type and GGUF_TYPE_COUNT
//   ggml/include/gguf.h:72-80       gguf_init_params, gguf_reader_callback_t
//   ggml/include/gguf.h:87-88       max_chunk_read semantics
//   ggml/src/gguf.cpp:92-106        GGUF_TYPE_SIZE
//   ggml/src/gguf.cpp:109-123       GGUF_TYPE_NAME
//   ggml/src/gguf.cpp:381-412       gguf_reader::read_raw, the callback loop
//   ggml/src/gguf.cpp:621-628       the power-of-two alignment rule
//   ggml/src/gguf.cpp:761-775       the data-offset rule (padded iff n_tensors > 0)
//
// What is frozen:
//   - the four magic bytes, the format version and the default alignment, both
//     as numbers and as the 32-bit little-endian word a reader compares;
//   - the alignment key text, 18 bytes, matched by strcmp;
//   - the thirteen value types of enum gguf_type, their exact name texts and
//     their element sizes. GGUF_TYPE_STRING and GGUF_TYPE_ARRAY have size 0:
//     they are encoded as length plus payload, not as a fixed-size element;
//   - the layout of gguf_init_params, which crosses the header by value;
//   - the reader callback contract: each call asks for min(max_chunk_read,
//     remaining) bytes at an absolute offset, a full read continues the loop, a
//     short read is end-of-file and stops it, and max_chunk_read == 0 means no
//     limit (SIZE_MAX). The count of calls and the size of each call are pure
//     functions of the file length and the chunk limit;
//   - the alignment rules: an alignment must be a non-zero power of two, data
//     offsets are padded up to the alignment only when the file has tensors.
//
// The prefix is ggh_.

import std;

const GGH_NONE: i64 = 0 - 1;

// ── constants and layout entries, name order ────────────────────────────────
const GGH_CONST_N: i64 = 25;

const GGH_CONST_NAME: const u8[:][25] = [
    "GGUF_MAGIC[0]",
    "GGUF_MAGIC[1]",
    "GGUF_MAGIC[2]",
    "GGUF_MAGIC[3]",
    "GGUF_MAGIC_U32",
    "GGUF_VERSION",
    "GGUF_DEFAULT_ALIGNMENT",
    "GGUF_KEY_GENERAL_ALIGNMENT_LEN",
    "GGUF_TYPE_UINT8",
    "GGUF_TYPE_INT8",
    "GGUF_TYPE_UINT16",
    "GGUF_TYPE_INT16",
    "GGUF_TYPE_UINT32",
    "GGUF_TYPE_INT32",
    "GGUF_TYPE_FLOAT32",
    "GGUF_TYPE_BOOL",
    "GGUF_TYPE_STRING",
    "GGUF_TYPE_ARRAY",
    "GGUF_TYPE_UINT64",
    "GGUF_TYPE_INT64",
    "GGUF_TYPE_FLOAT64",
    "GGUF_TYPE_COUNT",
    "sizeof:gguf_init_params",
    "offsetof:gguf_init_params.no_alloc",
    "offsetof:gguf_init_params.ctx"
];

const GGH_CONST_VAL: i64[25] = [
    71, 71, 85, 70,
    1179993927,
    3,
    32,
    17,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
    13,
    16,
    0, 8
];

// ── the value types (gguf.cpp:92-123) ──────────────────────────────────────
const GGH_TYPE_N: i64 = 13;

const GGH_TYPE_VAL: i64[13] = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12];

const GGH_TYPE_TXT: const u8[:][13] = [
    "u8",
    "i8",
    "u16",
    "i16",
    "u32",
    "i32",
    "f32",
    "bool",
    "str",
    "arr",
    "u64",
    "i64",
    "f64"
];

// Element size in bytes; 0 marks a type without a fixed-size element.
const GGH_TYPE_SIZE: i64[13] = [1, 1, 2, 2, 4, 4, 4, 1, 0, 0, 8, 8, 8];

fn ggh_copy(out: u8[:], s: const u8[:]) -> i64 {
    var n: i64 = 0;
    while n < s.len && n < out.len {
        out[n] = s[n];
        n = n + 1;
    }
    return n;
}

// ---- exports --------------------------------------------------------------

export fn ggh_const_count() -> i64 {
    return GGH_CONST_N;
}

export fn ggh_const_name(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= GGH_CONST_N {
        return GGH_NONE;
    }
    return ggh_copy(out, GGH_CONST_NAME[i]);
}

export fn ggh_const_value(i: i64) -> i64 {
    if i < 0 || i >= GGH_CONST_N {
        return GGH_NONE;
    }
    return GGH_CONST_VAL[i];
}

export fn ggh_type_count() -> i64 {
    return GGH_TYPE_N;
}

export fn ggh_type_value(i: i64) -> i64 {
    if i < 0 || i >= GGH_TYPE_N {
        return GGH_NONE;
    }
    return GGH_TYPE_VAL[i];
}

// Exact name text; native returns nullptr for a value outside the enum, the
// port returns -1.
export fn ggh_type_name(t: i64, out: u8[:]) -> i64 {
    var i: i64 = 0;
    while i < GGH_TYPE_N {
        if GGH_TYPE_VAL[i] == t {
            return ggh_copy(out, GGH_TYPE_TXT[i]);
        }
        i = i + 1;
    }
    return GGH_NONE;
}

// Element size, 0 for the two length-prefixed types; native returns 0 for an
// unknown value as well.
export fn ggh_type_size(t: i64) -> i64 {
    var i: i64 = 0;
    while i < GGH_TYPE_N {
        if GGH_TYPE_VAL[i] == t {
            return GGH_TYPE_SIZE[i];
        }
        i = i + 1;
    }
    return 0;
}

export fn ggh_key_general_alignment(out: u8[:]) -> i64 {
    return ggh_copy(out, "general.alignment");
}

// gguf.cpp:621-628: zero is rejected and so is every non-power-of-two.
export fn ggh_alignment_valid(a: i64) -> i64 {
    if a <= 0 {
        return 0;
    }
    if (a & (a - 1)) != 0 {
        return 0;
    }
    return 1;
}

// gguf.cpp:761-775: pad up to the alignment only when the file has tensors.
export fn ggh_data_offset(meta_end: i64, alignment: i64, n_tensors: i64) -> i64 {
    if meta_end < 0 || alignment <= 0 {
        return GGH_NONE;
    }
    if n_tensors <= 0 {
        return meta_end;
    }
    return (meta_end + alignment - 1) & (0 - alignment);
}

// gguf.cpp:381-401: one callback per chunk; chunk = min(max_chunk, remaining),
// and a zero limit means "no limit", i.e. one call for the whole request.
export fn ggh_reader_calls(total: i64, max_chunk: i64) -> i64 {
    if total <= 0 {
        return 0;
    }
    if max_chunk <= 0 {
        return 1;
    }
    var calls: i64 = total / max_chunk;
    if total % max_chunk != 0 {
        calls = calls + 1;
    }
    return calls;
}

export fn ggh_reader_chunk(total: i64, max_chunk: i64, i: i64) -> i64 {
    if total <= 0 || i < 0 || i >= ggh_reader_calls(total, max_chunk) {
        return GGH_NONE;
    }
    var limit: i64 = max_chunk;
    if limit <= 0 || limit > total {
        limit = total;
    }
    var begin: i64 = i * limit;
    var left: i64 = total - begin;
    if left < 0 {
        left = 0;
    }
    if left > limit {
        return limit;
    }
    return left;
}

// A full read continues the loop; anything shorter is end-of-file (native sets
// reached_eof and stops, it does not retry).
export fn ggh_reader_continues(requested: i64, returned: i64) -> i64 {
    if requested <= 0 {
        return 0;
    }
    if returned == requested {
        return 1;
    }
    return 0;
}
