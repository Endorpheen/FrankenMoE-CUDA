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
// GGUF writer in Goose (P7-02 S1 + S2 + S3), from ggml/src/gguf.cpp:
// the metadata context and setters (S1), gguf_write_out (S2), and the tensor
// descriptors plus the data section (S3): gguf_add_tensor,
// gguf_set_tensor_type, gguf_set_tensor_data, cumulative padded offsets,
// gguf_write_tensor_meta and gguf_write_tensor_data.
//
// Slice ownership:
//   - S1: flat KV list with caller-owned byte pool; a setter removes an
//     existing key first and appends; remove returns the old index or -1;
//     set_kv replays in source order; "general.alignment" accepts only a u32
//     power of two and the port returns -1 instead of aborting.
//   - S2: writer core: magic, version, u64 counts, KV encoding order, array
//     encoding, string encoding, zero padding to the alignment, only_meta.
//   - S3: tensor descriptors (name, n_dims, ne[], type, cumulative padded
//     offset), the pad between the descriptor block and the data section,
//     each tensor's data padded to the alignment, and the type size / block
//     size table needed for ggml_nbytes.
//
// KV record layout (parallel arrays, index 0..n-1):
//   key_off/key_len  key bytes in `pool`
//   type             gguf_type of a scalar, or element type of an array
//   arr              0 scalar, 1 array of values, 2 array of strings
//   v0               scalar: raw 64-bit value; otherwise pool offset
//   v1               array length (0 for scalars)
//   v2               byte length in pool (0 for numeric scalars)
//
// Tensor record layout (flat arrays, index i, stride 8):
//   TR_NAME_OFF/TR_NAME_LEN  name bytes in `tpool`
//   TR_TYPE                  ggml_type id
//   TR_DIMS                  n_dims derived from ne[] by ggml_n_dims
//   TR_DATA_OFF/TR_DATA_LEN  data bytes in `tpool` (-1 length = no data yet)
//   TR_OFFSET                cumulative offset in the data section
//   plus tne[i*4 + d] for ne[].
//
// State: st[0] n_kv, st[1] KV pool cursor; tst[0] n_tensors,
// tst[1] alignment, tst[2] tensor pool cursor.
//
// Formats and type ids follow the native enums: gguf_type
// (UINT8=0 ... STRING=8, ARRAY=9 ... FLOAT64=12) and ggml_type
// (F32=0, F16=1, Q4_0=2 ... I32=26 ... GGML_TYPE_COUNT=43). The type size and
// block size table is copied from the native ggml_type_traits; removed types
// have a block size of 0 and are refused.

const S_N: i64 = 0;
const S_POOL: i64 = 1;

const T_UINT32: i64 = 4;
const T_STRING: i64 = 8;

fn find(st: i64[:], pool: const u8[:], key_off: i64[:], key_len: i64[:], key: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < st[S_N] {
        let o: i64 = key_off[i];
        let n: i64 = key_len[i];
        if pool[o..o + n] == key {
            return i;
        }
        i += 1 as i64;
    }
    return -(1 as i64);
}

fn shift_left(st: i64[:], key_off: i64[:], key_len: i64[:], types: i64[:],
              arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:], at: i64) -> i64 {
    var i: i64 = at;
    while i + (1 as i64) < st[S_N] {
        key_off[i] = key_off[i + (1 as i64)];
        key_len[i] = key_len[i + (1 as i64)];
        types[i] = types[i + (1 as i64)];
        arr[i] = arr[i + (1 as i64)];
        v0[i] = v0[i + (1 as i64)];
        v1[i] = v1[i + (1 as i64)];
        v2[i] = v2[i + (1 as i64)];
        i += 1 as i64;
    }
    st[S_N] = st[S_N] - (1 as i64);
    return at;
}

fn drop(st: i64[:], pool: const u8[:], key_off: i64[:], key_len: i64[:],
        types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
        key: const u8[:]) -> i64 {
    let at: i64 = find(st, pool, key_off, key_len, key);
    if at < 0 as i64 {
        return -(1 as i64);
    }
    return shift_left(st, key_off, key_len, types, arr, v0, v1, v2, at);
}

fn append_key(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:], key: const u8[:]) -> i64 {
    let off: i64 = st[S_POOL];
    var i: i64 = 0;
    while i < key.len {
        pool[off + i] = key[i];
        i += 1 as i64;
    }
    st[S_POOL] = off + key.len;
    key_off[st[S_N]] = off;
    key_len[st[S_N]] = key.len;
    return off;
}

fn reserved_ok(key: const u8[:], t: i64, value: i64) -> bool {
    if key != "general.alignment" {
        return true;
    }
    if t != T_UINT32 {
        return false;
    }
    return value > 0 as i64 && (value & (value - (1 as i64))) == 0 as i64;
}

export fn s1_init(st: i64[:], key_off: i64[:], key_len: i64[:], types: i64[:],
                  arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:]) -> i64 {
    st[S_N] = 0 as i64;
    st[S_POOL] = 0 as i64;
    key_off[0] = 0 as i64;
    key_len[0] = 0 as i64;
    types[0] = 0 as i64;
    arr[0] = 0 as i64;
    v0[0] = 0 as i64;
    v1[0] = 0 as i64;
    v2[0] = 0 as i64;
    return 0 as i64;
}

// Scalar setter. `bits` is the raw value (u64 bits for u64/i64 and floats).
export fn s1_set_val(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                     types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                     key: const u8[:], t: i64, bits: i64, out: i64[:]) -> i64 {
    if !reserved_ok(key, t, bits) {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    let _drop: i64 = drop(st, pool, key_off, key_len, types, arr, v0, v1, v2, key);
    let off: i64 = append_key(st, pool, key_off, key_len, key);
    types[st[S_N]] = t;
    arr[st[S_N]] = 0 as i64;
    v0[st[S_N]] = bits;
    v1[st[S_N]] = 0 as i64;
    v2[st[S_N]] = 0 as i64;
    st[S_N] = st[S_N] + (1 as i64);
    out[0] = off;
    return 0 as i64;
}

// Scalar string setter (type 8); bytes are copied into the pool.
export fn s1_set_str(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                     types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                     key: const u8[:], val: const u8[:], out: i64[:]) -> i64 {
    if !reserved_ok(key, T_STRING, 0 as i64) {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    let _drop: i64 = drop(st, pool, key_off, key_len, types, arr, v0, v1, v2, key);
    let off: i64 = append_key(st, pool, key_off, key_len, key);
    var i: i64 = 0;
    while i < val.len {
        pool[st[S_POOL] + i] = val[i];
        i += 1 as i64;
    }
    types[st[S_N]] = T_STRING;
    arr[st[S_N]] = 0 as i64;
    v0[st[S_N]] = st[S_POOL];
    v1[st[S_N]] = val.len;
    v2[st[S_N]] = val.len;
    st[S_POOL] = st[S_POOL] + val.len;
    st[S_N] = st[S_N] + (1 as i64);
    out[0] = off;
    return 0 as i64;
}

// Numeric/bool array. `data` is already the element byte image; `n` elements.
export fn s1_set_arr(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                     types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                     key: const u8[:], atype: i64, data: const u8[:], n: i64, out: i64[:]) -> i64 {
    if !reserved_ok(key, atype, 0 as i64) {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    let _drop: i64 = drop(st, pool, key_off, key_len, types, arr, v0, v1, v2, key);
    let off: i64 = append_key(st, pool, key_off, key_len, key);
    var i: i64 = 0;
    while i < data.len {
        pool[st[S_POOL] + i] = data[i];
        i += 1 as i64;
    }
    types[st[S_N]] = atype;
    arr[st[S_N]] = 1 as i64;
    v0[st[S_N]] = st[S_POOL];
    v1[st[S_N]] = n;
    v2[st[S_N]] = data.len;
    st[S_POOL] = st[S_POOL] + data.len;
    st[S_N] = st[S_N] + (1 as i64);
    out[0] = off;
    return 0 as i64;
}

// String array: `data` is the NUL-separated concatenation of `n` strings.
export fn s1_set_arr_str(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
                         types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                         key: const u8[:], data: const u8[:], n: i64, out: i64[:]) -> i64 {
    if !reserved_ok(key, T_STRING, 0 as i64) {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    let _drop: i64 = drop(st, pool, key_off, key_len, types, arr, v0, v1, v2, key);
    let off: i64 = append_key(st, pool, key_off, key_len, key);
    var i: i64 = 0;
    while i < data.len {
        pool[st[S_POOL] + i] = data[i];
        i += 1 as i64;
    }
    types[st[S_N]] = T_STRING;
    arr[st[S_N]] = 2 as i64;
    v0[st[S_N]] = st[S_POOL];
    v1[st[S_N]] = n;
    v2[st[S_N]] = data.len;
    st[S_POOL] = st[S_POOL] + data.len;
    st[S_N] = st[S_N] + (1 as i64);
    out[0] = off;
    return 0 as i64;
}

export fn s1_remove(st: i64[:], pool: const u8[:], key_off: i64[:], key_len: i64[:],
                    types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                    key: const u8[:]) -> i64 {
    return drop(st, pool, key_off, key_len, types, arr, v0, v1, v2, key);
}

// Copy every KV from src to dst with native gguf_set_kv semantics: source order,
// duplicate keys in dst removed, reserved keys revalidated.
export fn s1_set_kv(dst_st: i64[:], dst_pool: u8[:], dst_key_off: i64[:], dst_key_len: i64[:],
                    dst_types: i64[:], dst_arr: i64[:], dst_v0: i64[:], dst_v1: i64[:], dst_v2: i64[:],
                    src_st: const i64[:], src_pool: const u8[:], src_key_off: i64[:], src_key_len: i64[:],
                    src_types: i64[:], src_arr: i64[:], src_v0: i64[:], src_v1: i64[:], src_v2: i64[:],
                    out: i64[:]) -> i64 {
    var k: i64 = 0;
    while k < src_st[S_N] {
        let key: const u8[:] = src_pool[src_key_off[k]..src_key_off[k] + src_key_len[k]];
        let t: i64 = src_types[k];
        if !reserved_ok(key, t, src_v0[k]) {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
        let _drop: i64 = drop(dst_st, dst_pool, dst_key_off, dst_key_len, dst_types,
                              dst_arr, dst_v0, dst_v1, dst_v2, key);
        let _off: i64 = append_key(dst_st, dst_pool, dst_key_off, dst_key_len, key);
        let di: i64 = dst_st[S_N];
        dst_types[di] = t;
        dst_arr[di] = src_arr[k];
        dst_v1[di] = src_v1[k];
        if src_arr[k] == 0 as i64 && t != T_STRING {
            dst_v0[di] = src_v0[k];
            dst_v2[di] = 0 as i64;
        } else {
            let off: i64 = dst_st[S_POOL];
            let m: i64 = src_v2[k];
            var j: i64 = 0;
            while j < m {
                dst_pool[off + j] = src_pool[src_v0[k] + j];
                j += 1 as i64;
            }
            dst_v0[di] = off;
            dst_v2[di] = m;
            dst_st[S_POOL] = off + m;
        }
        dst_st[S_N] = di + (1 as i64);
        k += 1 as i64;
    }
    out[0] = 0 as i64;
    return 0 as i64;
}

// ---- S2: little-endian byte helpers and the metadata section ---------------

const W_MAGIC_0: i64 = 71;
const W_MAGIC_1: i64 = 71;
const W_MAGIC_2: i64 = 85;
const W_MAGIC_3: i64 = 70;

fn w_u8(out: u8[:], p: i64, v: i64) -> i64 {
    out[p] = (v & 255 as i64) as u8;
    return p + (1 as i64);
}

fn w_u32(out: u8[:], p: i64, v: i64) -> i64 {
    var q = p;
    var i: i64 = 0;
    while i < 4 as i64 {
        out[q] = ((v >> (8 as i64 * i)) & 255 as i64) as u8;
        q = q + (1 as i64);
        i = i + (1 as i64);
    }
    return q;
}

fn w_u64(out: u8[:], p: i64, v: i64) -> i64 {
    var q = p;
    var i: i64 = 0;
    while i < 8 as i64 {
        out[q] = ((v >> (8 as i64 * i)) & 255 as i64) as u8;
        q = q + (1 as i64);
        i = i + (1 as i64);
    }
    return q;
}

fn w_bytes(out: u8[:], p: i64, src: const u8[:], off: i64, len: i64) -> i64 {
    var q = p;
    var i: i64 = 0;
    while i < len {
        out[q] = src[off + i];
        q = q + (1 as i64);
        i = i + (1 as i64);
    }
    return q;
}

fn w_str(out: u8[:], p: i64, src: const u8[:], off: i64, len: i64) -> i64 {
    var q = w_u64(out, p, len);
    return w_bytes(out, q, src, off, len);
}

fn w_bits(out: u8[:], p: i64, bits: i64, n: i64) -> i64 {
    var q = p;
    var i: i64 = 0;
    while i < n {
        out[q] = ((bits >> (8 as i64 * i)) & 255 as i64) as u8;
        q = q + (1 as i64);
        i = i + (1 as i64);
    }
    return q;
}

fn w_type_size(t: i64) -> i64 {
    if t == 0 as i64 || t == 1 as i64 || t == 7 as i64 {
        return 1 as i64;
    }
    if t == 2 as i64 || t == 3 as i64 {
        return 2 as i64;
    }
    if t == 4 as i64 || t == 5 as i64 || t == 6 as i64 {
        return 4 as i64;
    }
    return 8 as i64;
}

fn w_pad(out: u8[:], p: i64, alignment: i64) -> i64 {
    var q = p;
    while q % alignment != 0 as i64 {
        out[q] = 0 as u8;
        q = q + (1 as i64);
    }
    return q;
}

// Header plus every KV in list order. Returns the offset after the last KV.
fn w_meta(st: const i64[:], pool: const u8[:], key_off: const i64[:],
          key_len: const i64[:], types: const i64[:], arr: const i64[:],
          v0: const i64[:], v1: const i64[:], v2: const i64[:],
          out: u8[:], version: i64, n_tensors: i64) -> i64 {
    var p: i64 = 0;

    p = w_u8(out, p, W_MAGIC_0);
    p = w_u8(out, p, W_MAGIC_1);
    p = w_u8(out, p, W_MAGIC_2);
    p = w_u8(out, p, W_MAGIC_3);
    p = w_u32(out, p, version);
    p = w_u64(out, p, n_tensors);
    p = w_u64(out, p, st[S_N]);

    var i: i64 = 0;
    while i < st[S_N] {
        p = w_str(out, p, pool, key_off[i], key_len[i]);
        let t = types[i];

        if arr[i] != 0 as i64 {
            p = w_u32(out, p, 9 as i64);
            p = w_u32(out, p, t);
            p = w_u64(out, p, v1[i]);
            if arr[i] == 1 as i64 {
                p = w_bytes(out, p, pool, v0[i], v2[i]);
            } else {
                let lim = v0[i] + v2[i];
                var q = v0[i];
                var k: i64 = 0;
                while k < v1[i] {
                    var e = q;
                    while e < lim && pool[e] != 0 as u8 {
                        e = e + (1 as i64);
                    }
                    p = w_str(out, p, pool, q, e - q);
                    q = e + (1 as i64);
                    k = k + (1 as i64);
                }
            }
        } else {
            p = w_u32(out, p, t);
            if t == 8 as i64 {
                p = w_str(out, p, pool, v0[i], v1[i]);
            } else {
                p = w_bits(out, p, v0[i], w_type_size(t));
            }
        }
        i = i + (1 as i64);
    }
    return p;
}

// S2 entry point: metadata only, zero tensors, padded to the alignment.
export fn s2_write(st: i64[:], pool: const u8[:], key_off: const i64[:],
                   key_len: const i64[:], types: const i64[:], arr: const i64[:],
                   v0: const i64[:], v1: const i64[:], v2: const i64[:],
                   out: u8[:], version: i64, alignment: i64, only_meta: i64) -> i64 {
    var p: i64 = 0;

    p = w_u8(out, p, W_MAGIC_0);
    p = w_u8(out, p, W_MAGIC_1);
    p = w_u8(out, p, W_MAGIC_2);
    p = w_u8(out, p, W_MAGIC_3);
    p = w_u32(out, p, version);
    p = w_u64(out, p, 0 as i64);
    p = w_u64(out, p, st[S_N]);

    var i: i64 = 0;
    while i < st[S_N] {
        p = w_str(out, p, pool, key_off[i], key_len[i]);
        let t = types[i];

        if arr[i] != 0 as i64 {
            p = w_u32(out, p, 9 as i64);
            p = w_u32(out, p, t);
            p = w_u64(out, p, v1[i]);
            if arr[i] == 1 as i64 {
                p = w_bytes(out, p, pool, v0[i], v2[i]);
            } else {
                let lim = v0[i] + v2[i];
                var q = v0[i];
                var k: i64 = 0;
                while k < v1[i] {
                    var e = q;
                    while e < lim && pool[e] != 0 as u8 {
                        e = e + (1 as i64);
                    }
                    p = w_str(out, p, pool, q, e - q);
                    q = e + (1 as i64);
                    k = k + (1 as i64);
                }
            }
        } else {
            p = w_u32(out, p, t);
            if t == 8 as i64 {
                p = w_str(out, p, pool, v0[i], v1[i]);
            } else {
                p = w_bits(out, p, v0[i], w_type_size(t));
            }
        }
        i = i + (1 as i64);
    }

    let a = alignment;
    while p % a != 0 as i64 {
        out[p] = 0 as u8;
        p = p + (1 as i64);
    }
    return p;
}

// ---- S3: tensor descriptors, data section, type size / block size ---------

const TS_N: i64 = 0;
const TS_ALIGN: i64 = 1;
const TS_POOL: i64 = 2;

const TR_STRIDE: i64 = 8;
const TR_NAME_OFF: i64 = 0;
const TR_NAME_LEN: i64 = 1;
const TR_TYPE: i64 = 2;
const TR_DIMS: i64 = 3;
const TR_DATA_OFF: i64 = 4;
const TR_DATA_LEN: i64 = 5;
const TR_OFFSET: i64 = 6;

const GGML_MAX_NAME: i64 = 64;

// ggml_type_traits[type].blck_size, copied from ggml/src/ggml.c. Removed types
// (4, 5, 31-33, 36-38) have blck_size 0 and are refused by add/set_type.
fn t_blck_size(t: i64) -> i64 {
    if t == 0 as i64 || t == 1 as i64 || t == 24 as i64 || t == 25 as i64 ||
       t == 26 as i64 || t == 27 as i64 || t == 28 as i64 || t == 30 as i64 {
        return 1 as i64;
    }
    if t == 2 as i64 || t == 3 as i64 || t == 6 as i64 || t == 7 as i64 ||
       t == 8 as i64 || t == 9 as i64 || t == 20 as i64 || t == 39 as i64 {
        return 32 as i64;
    }
    if t == 40 as i64 || t == 42 as i64 {
        return 64 as i64;
    }
    if t == 41 as i64 || t == 142 as i64 {
        return 128 as i64;
    }
    if t >= 10 as i64 && t <= 23 as i64 {
        return 256 as i64;
    }
    if t == 29 as i64 || t == 34 as i64 || t == 35 as i64 {
        return 256 as i64;
    }
    return 0 as i64;
}

// ggml_type_traits[type].type_size, same source.
fn t_type_size(t: i64) -> i64 {
    if t == 0 as i64 { return 4 as i64; }    // f32
    if t == 1 as i64 { return 2 as i64; }    // f16
    if t == 2 as i64 { return 18 as i64; }   // q4_0
    if t == 3 as i64 { return 20 as i64; }   // q4_1
    if t == 6 as i64 { return 22 as i64; }   // q5_0
    if t == 7 as i64 { return 24 as i64; }   // q5_1
    if t == 8 as i64 { return 34 as i64; }   // q8_0
    if t == 9 as i64 { return 36 as i64; }   // q8_1
    if t == 10 as i64 { return 84 as i64; }  // q2_K
    if t == 11 as i64 { return 110 as i64; } // q3_K
    if t == 12 as i64 { return 144 as i64; } // q4_K
    if t == 13 as i64 { return 176 as i64; } // q5_K
    if t == 14 as i64 { return 210 as i64; } // q6_K
    if t == 15 as i64 { return 292 as i64; } // q8_K
    if t == 16 as i64 { return 66 as i64; }  // iq2_xxs
    if t == 17 as i64 { return 74 as i64; }  // iq2_xs
    if t == 18 as i64 { return 98 as i64; }  // iq3_xxs
    if t == 19 as i64 { return 50 as i64; }  // iq1_s
    if t == 20 as i64 { return 18 as i64; }  // iq4_nl
    if t == 21 as i64 { return 110 as i64; } // iq3_s
    if t == 22 as i64 { return 82 as i64; }  // iq2_s
    if t == 23 as i64 { return 136 as i64; } // iq4_xs
    if t == 24 as i64 { return 1 as i64; }   // i8
    if t == 25 as i64 { return 2 as i64; }   // i16
    if t == 26 as i64 { return 4 as i64; }   // i32
    if t == 27 as i64 { return 8 as i64; }   // i64
    if t == 28 as i64 { return 8 as i64; }   // f64
    if t == 29 as i64 { return 56 as i64; }  // iq1_m
    if t == 30 as i64 { return 2 as i64; }   // bf16
    if t == 34 as i64 { return 54 as i64; }  // tq1_0
    if t == 35 as i64 { return 66 as i64; }  // tq2_0
    if t == 39 as i64 { return 17 as i64; }  // mxfp4
    if t == 40 as i64 { return 36 as i64; }  // nvfp4
    if t == 41 as i64 { return 18 as i64; }  // q1_0
    if t == 42 as i64 { return 18 as i64; }  // q2_0
    if t == 142 as i64 { return 34 as i64; } // Prism PQ2_0
    return 0 as i64;
}

fn t_align_ok(a: i64) -> bool {
    if a <= 0 as i64 {
        return false;
    }
    return (a & (a - (1 as i64))) == 0 as i64;
}

fn t_pad(v: i64, a: i64) -> i64 {
    return ((v + a - (1 as i64)) / a) * a;
}

// ggml_n_dims: the highest dimension with more than one element, at least 1.
// Native writes this value, not a caller-declared count, so the wire value is
// derived from ne[] even when the API receives ndims explicitly.
fn t_dims(tne: const i64[:], i: i64) -> i64 {
    var d: i64 = 3 as i64;
    while d >= 1 as i64 {
        if tne[i * 4 as i64 + d] > 1 as i64 {
            return d + (1 as i64);
        }
        d -= 1 as i64;
    }
    return 1 as i64;
}

fn t_find(tst: const i64[:], tpool: const u8[:], trec: const i64[:], name: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < tst[TS_N] {
        let o: i64 = trec[i * TR_STRIDE + TR_NAME_OFF];
        let n: i64 = trec[i * TR_STRIDE + TR_NAME_LEN];
        if tpool[o..o + n] == name {
            return i;
        }
        i += 1 as i64;
    }
    return -(1 as i64);
}

// ggml_nbytes for a contiguous tensor, from the type table.
// Returns -1 for a type whose block size is 0.
fn t_nbytes(tne: const i64[:], i: i64, ty: i64) -> i64 {
    let blck: i64 = t_blck_size(ty);
    let tsize: i64 = t_type_size(ty);
    if blck <= 0 as i64 || tsize <= 0 as i64 {
        return -(1 as i64);
    }
    var d: i64 = 0;
    while d < 4 as i64 {
        if tne[i * 4 as i64 + d] <= 0 as i64 {
            return 0 as i64;
        }
        d += 1 as i64;
    }
    let row: i64 = tsize * (tne[i * 4 as i64] / blck);
    return row * tne[i * 4 as i64 + 1] * tne[i * 4 as i64 + 2] * tne[i * 4 as i64 + 3];
}

// Cumulative padded offsets: offset[0] = 0,
// offset[i] = offset[i-1] + PAD(nbytes[i-1], alignment).
fn t_recompute(tst: const i64[:], tne: const i64[:], trec: i64[:], from: i64) -> i64 {
    var i: i64 = from;
    if i < 0 as i64 {
        i = 0 as i64;
    }
    while i < tst[TS_N] {
        if i == 0 as i64 {
            trec[TR_OFFSET] = 0 as i64;
        } else {
            let p: i64 = i - (1 as i64);
            let nb: i64 = t_nbytes(tne, p, trec[p * TR_STRIDE + TR_TYPE]);
            if nb < 0 as i64 {
                return -(1 as i64);
            }
            trec[i * TR_STRIDE + TR_OFFSET] =
                trec[p * TR_STRIDE + TR_OFFSET] + t_pad(nb, tst[TS_ALIGN]);
        }
        i += 1 as i64;
    }
    return 0 as i64;
}

export fn s3_init_tensors(tst: i64[:], alignment: i64) -> i64 {
    if !t_align_ok(alignment) {
        return -(1 as i64);
    }
    tst[TS_N] = 0 as i64;
    tst[TS_ALIGN] = alignment;
    tst[TS_POOL] = 0 as i64;
    return 0 as i64;
}

// Add one tensor descriptor. `ne` carries GGML_MAX_DIMS entries (mirroring the
// native ggml_tensor); `ndims` is the caller's declared count and is validated,
// but the wire value follows ggml_n_dims over ne[] exactly like native.
// Returns the new index, or -1 for a duplicate name, an invalid type, a bad
// ndims, a name that the native reader would reject (empty or >=
// GGML_MAX_NAME), or a data image whose length is not nbytes.
export fn s3_add_tensor(tst: i64[:], tpool: u8[:], tne: i64[:], trec: i64[:],
                        name: const u8[:], ndims: i64, ne: const i64[:],
                        t: i64, data: const u8[:]) -> i64 {
    if ndims < 1 as i64 || ndims > 4 as i64 || ne.len < 4 as i64 {
        return -(1 as i64);
    }
    if name.len < 1 as i64 || name.len >= GGML_MAX_NAME {
        return -(1 as i64);
    }
    if t_blck_size(t) <= 0 as i64 {
        return -(1 as i64);
    }
    if t_find(tst, tpool, trec, name) >= 0 as i64 {
        return -(1 as i64);
    }
    let i: i64 = tst[TS_N];
    if (i + (1 as i64)) * TR_STRIDE > trec.len {
        return -(1 as i64);
    }
    let need: i64 = name.len + data.len;
    if tst[TS_POOL] + need > tpool.len {
        return -(1 as i64);
    }

    let noff: i64 = tst[TS_POOL];
    var k: i64 = 0;
    while k < name.len {
        tpool[noff + k] = name[k];
        k += 1 as i64;
    }
    tst[TS_POOL] = noff + name.len;
    trec[i * TR_STRIDE + TR_NAME_OFF] = noff;
    trec[i * TR_STRIDE + TR_NAME_LEN] = name.len;

    var d: i64 = 0;
    while d < 4 as i64 {
        tne[i * 4 as i64 + d] = ne[d];
        if tne[i * 4 as i64 + d] < 0 as i64 {
            return -(1 as i64);
        }
        d += 1 as i64;
    }
    trec[i * TR_STRIDE + TR_TYPE] = t;
    trec[i * TR_STRIDE + TR_DIMS] = t_dims(tne, i);

    let nb: i64 = t_nbytes(tne, i, t);
    if nb < 0 as i64 {
        return -(1 as i64);
    }
    if data.len > 0 as i64 && data.len != nb {
        return -(1 as i64);
    }

    if data.len > 0 as i64 {
        let doff: i64 = tst[TS_POOL];
        var j: i64 = 0;
        while j < data.len {
            tpool[doff + j] = data[j];
            j += 1 as i64;
        }
        trec[i * TR_STRIDE + TR_DATA_OFF] = doff;
        trec[i * TR_STRIDE + TR_DATA_LEN] = data.len;
        tst[TS_POOL] = doff + data.len;
    } else {
        trec[i * TR_STRIDE + TR_DATA_OFF] = 0 as i64;
        trec[i * TR_STRIDE + TR_DATA_LEN] = -(1 as i64);
    }

    tst[TS_N] = i + (1 as i64);
    let rc: i64 = t_recompute(tst, tne, trec, i);
    if rc != 0 as i64 {
        tst[TS_N] = i;
        return -(1 as i64);
    }
    return i;
}

// Change a tensor type and recompute every later offset, like the native
// function. Refuses an unknown tensor, a removed type and a row that does not
// divide by the new block size (native asserts in those cases).
export fn s3_set_tensor_type(tst: i64[:], tpool: const u8[:], tne: i64[:], trec: i64[:],
                             name: const u8[:], t: i64) -> i64 {
    let id: i64 = t_find(tst, tpool, trec, name);
    if id < 0 as i64 {
        return -(1 as i64);
    }
    let blck: i64 = t_blck_size(t);
    if blck <= 0 as i64 || t_type_size(t) <= 0 as i64 {
        return -(1 as i64);
    }
    if (tne[id * 4 as i64] % blck) != 0 as i64 {
        return -(1 as i64);
    }
    trec[id * TR_STRIDE + TR_TYPE] = t;
    return t_recompute(tst, tne, trec, id);
}

// Attach or replace the data image of a tensor. The bytes are copied into the
// tensor pool; the writer emits nbytes of the current type from this image and
// refuses when fewer bytes were supplied.
export fn s3_set_tensor_data(tst: i64[:], tpool: u8[:], tne: const i64[:], trec: i64[:],
                             name: const u8[:], data: const u8[:]) -> i64 {
    let id: i64 = t_find(tst, tpool, trec, name);
    if id < 0 as i64 {
        return -(1 as i64);
    }
    if tst[TS_POOL] + data.len > tpool.len {
        return -(1 as i64);
    }
    let off: i64 = tst[TS_POOL];
    var j: i64 = 0;
    while j < data.len {
        tpool[off + j] = data[j];
        j += 1 as i64;
    }
    trec[id * TR_STRIDE + TR_DATA_OFF] = off;
    trec[id * TR_STRIDE + TR_DATA_LEN] = data.len;
    tst[TS_POOL] = off + data.len;
    return 0 as i64;
}

export fn s3_find(tst: const i64[:], tpool: const u8[:], trec: const i64[:],
                  name: const u8[:]) -> i64 {
    return t_find(tst, tpool, trec, name);
}

// ggml_nbytes of tensor `i` with the port's own type table.
export fn s3_tensor_size(tne: const i64[:], trec: const i64[:], i: i64) -> i64 {
    return t_nbytes(tne, i, trec[i * TR_STRIDE + TR_TYPE]);
}

// Full writer: header, KV in list order, tensor descriptors, pad to the
// alignment, then (only_meta == 0) every tensor's data padded to the alignment.
// Returns bytes written, or -1 when a data image is missing/short or a type is
// unusable. The alignment comes from the tensor state so offsets and padding
// cannot disagree.
export fn s3_write(st: i64[:], pool: const u8[:], key_off: const i64[:],
                   key_len: const i64[:], types: const i64[:], arr: const i64[:],
                   v0: const i64[:], v1: const i64[:], v2: const i64[:],
                   tst: const i64[:], tpool: const u8[:], tne: const i64[:],
                   trec: const i64[:], out: u8[:], version: i64, only_meta: i64) -> i64 {
    var p: i64 = w_meta(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                        out, version, tst[TS_N]);

    var i: i64 = 0;
    while i < tst[TS_N] {
        p = w_str(out, p, tpool, trec[i * TR_STRIDE + TR_NAME_OFF],
                  trec[i * TR_STRIDE + TR_NAME_LEN]);
        p = w_u32(out, p, trec[i * TR_STRIDE + TR_DIMS]);
        var d: i64 = 0;
        while d < trec[i * TR_STRIDE + TR_DIMS] {
            p = w_u64(out, p, tne[i * 4 as i64 + d]);
            d += 1 as i64;
        }
        p = w_u32(out, p, trec[i * TR_STRIDE + TR_TYPE]);
        p = w_u64(out, p, trec[i * TR_STRIDE + TR_OFFSET]);
        i += 1 as i64;
    }

    let a: i64 = tst[TS_ALIGN];
    p = w_pad(out, p, a);

    if only_meta != 0 as i64 {
        return p;
    }

    i = 0;
    while i < tst[TS_N] {
        let ty: i64 = trec[i * TR_STRIDE + TR_TYPE];
        let nb: i64 = t_nbytes(tne, i, ty);
        let dl: i64 = trec[i * TR_STRIDE + TR_DATA_LEN];
        if nb < 0 as i64 || dl < nb {
            return -(1 as i64);
        }
        p = w_bytes(out, p, tpool, trec[i * TR_STRIDE + TR_DATA_OFF], nb);
        p = w_pad(out, p, a);
        i += 1 as i64;
    }
    return p;
}
// Engine GGUF v2/v3 reader. No native reads, strings, containers or allocations.
// Borrowed immutable bytes; caller-owned disjoint records and hash slots.
// KV stride8: name_off,len,wire_type,element_type,payload_off,count,bytes,hash.
// Tensor stride12: name_off,len,type,declared_dims,ne[4],rel,bytes,abs,hash.
// State8: magic919,version,n_kv,n_tensors,alignment,data_off,padded_data,meta_end.
// A failed parse clears magic; scratch outputs may be dirty and are unpublished.

fn gr_fits(data: const u8[:], at: i64, n: i64) -> bool {
    return at >= 0 && n >= 0 && at <= data.len && n <= data.len - at;
}

fn gr_bits(data: const u8[:], at: i64, n: i64) -> u64 {
    var result: u64 = 0; var i: i64 = 0;
    while i < n { result = result | ((data[at + i] as u64) << (i * 8)); i += 1; }
    return result;
}

fn gr_length(data: const u8[:], at: i64) -> i64 {
    if !gr_fits(data, at, 8) { return -1; }
    let length: u64 = gr_bits(data, at, 8);
    if length > 9223372036854775807 { return -1; }
    let n: i64 = length as i64;
    return if gr_fits(data, at + 8, n) { n } else { -1 };
}

fn gr_hash(data: const u8[:], at: i64, n: i64) -> u64 {
    var h: u64 = 14695981039346656037; var i: i64 = 0;
    while i < n { h = (h ^ (data[at + i] as u64)) * 1099511628211; i += 1; }
    return h;
}

fn gr_same(data: const u8[:], a: i64, na: i64, b: i64, nb: i64) -> bool {
    if na != nb || !gr_fits(data, a, na) || !gr_fits(data, b, nb) { return false; }
    var i: i64 = 0; while i < na { if data[a + i] != data[b + i] { return false; } i += 1; }
    return true;
}

fn gr_name_ok(data: const u8[:], at: i64, n: i64) -> bool {
    var i: i64 = 0; while i < n { if data[at + i] == 0 { return false; } i += 1; }
    return true;
}

fn gr_slots_ok(slots: i64[:], n: i64) -> bool {
    return slots.len > 0 && (slots.len & (slots.len - 1)) == 0 && n >= 0 && n <= slots.len / 2;
}

fn gr_insert(data: const u8[:], records: i64[:], stride: i64, slots: i64[:], index: i64) -> bool {
    let base: i64 = stride * index; let h: u64 = records[base + stride - 1] as! u64;
    var slot: i64 = (h & ((slots.len - 1) as u64)) as i64; var visited: i64 = 0;
    while visited < slots.len {
        let entry: i64 = slots[slot];
        if entry == 0 { slots[slot] = index + 1; return true; }
        if entry < 1 || entry > index { return false; }
        let prior: i64 = (entry - 1) * stride;
        if records[prior + stride - 1] == records[base + stride - 1] &&
           gr_same(data, records[prior], records[prior + 1], records[base], records[base + 1]) { return false; }
        slot = (slot + 1) & (slots.len - 1); visited += 1;
    }
    return false;
}

export fn gr_parse(data: const u8[:], file_bytes: i64, state: i64[:], kv: i64[:], tensors: i64[:],
                    kv_slots: i64[:], tensor_slots: i64[:]) -> i64 {
    if state.len < 8 { return -1; }
    state[0] = 0;
    if data.len > 67108864 || !gr_fits(data, 0, 24) || file_bytes < data.len ||
       gr_bits(data, 0, 4) != (ggh_const_value(4) as u64) { return -1; }
    let version: i64 = gr_bits(data, 4, 4) as i64;
    if version != 2 && version != 3 { return -1; }
    let nt_raw: u64 = gr_bits(data, 8, 8); let nk_raw: u64 = gr_bits(data, 16, 8);
    if nt_raw > 9223372036854775807 || nk_raw > 9223372036854775807 { return -2; }
    let nt: i64 = nt_raw as i64; let nk: i64 = nk_raw as i64;
    if nk > kv.len / 8 || nt > tensors.len / 12 || !gr_slots_ok(kv_slots, nk) || !gr_slots_ok(tensor_slots, nt) { return -2; }
    var i: i64 = 0; while i < kv_slots.len { kv_slots[i] = 0; i += 1; }
    i = 0; while i < tensor_slots.len { tensor_slots[i] = 0; i += 1; }
    var at: i64 = 24; var alignment: i64 = 32; i = 0;
    while i < nk {
        let n: i64 = gr_length(data, at);
        if n <= 0 || !gr_name_ok(data, at + 8, n) { return -3; }
        let base: i64 = i * 8; kv[base] = at + 8; kv[base + 1] = n;
        kv[base + 7] = gr_hash(data, at + 8, n) as! i64;
        if !gr_insert(data, kv, 8, kv_slots, i) { return -4; }
        at += 8 + n;
        if !gr_fits(data, at, 4) { return -3; }
        let wire: i64 = gr_bits(data, at, 4) as i64; at += 4;
        var element: i64 = -1; var count: i64 = 1; var payload: i64 = at; var bytes: i64 = 0;
        if wire == 8 {
            let length: i64 = gr_length(data, at); if length < 0 { return -3; }
            payload = at + 8; count = length; bytes = length; at += 8 + length;
        } else {
            if wire == 9 {
                if !gr_fits(data, at, 12) { return -3; }
                element = gr_bits(data, at, 4) as i64;
                let raw: u64 = gr_bits(data, at + 4, 8);
                if raw > 9223372036854775807 { return -3; }
                count = raw as i64; at += 12; payload = at;
                if element == 8 {
                    if count > (data.len - at) / 8 { return -3; }
                    var j: i64 = 0;
                    while j < count { let length: i64 = gr_length(data, at); if length < 0 { return -3; } at += 8 + length; j += 1; }
                    bytes = at - payload;
                } else {
                    let size: i64 = ggh_type_size(element);
                    if size <= 0 || count > (data.len - at) / size { return -5; }
                    bytes = size * count; at += bytes;
                }
            } else {
                bytes = ggh_type_size(wire);
                if bytes <= 0 || !gr_fits(data, at, bytes) { return -5; }
                at += bytes;
            }
        }
        kv[base + 2] = wire; kv[base + 3] = element; kv[base + 4] = payload;
        kv[base + 5] = count; kv[base + 6] = bytes;
        if wire == 7 || (wire == 9 && element == 7) {
            var b: i64 = 0; while b < bytes { if data[payload + b] > 1 { return -5; } b += 1; }
        }
        if data[kv[base] .. kv[base] + n] == "general.alignment" {
            if wire != 4 { return -5; }
            alignment = gr_bits(data, payload, 4) as i64;
            if ggh_alignment_valid(alignment) == 0 { return -6; }
        }
        i += 1;
    }
    var total: i64 = 0; i = 0;
    while i < nt {
        let n: i64 = gr_length(data, at);
        if n < 0 || n >= 64 || !gr_name_ok(data, at + 8, n) { return -3; }
        let base: i64 = i * 12; tensors[base] = at + 8; tensors[base + 1] = n;
        tensors[base + 11] = gr_hash(data, at + 8, n) as! i64;
        if !gr_insert(data, tensors, 12, tensor_slots, i) { return -4; }
        at += 8 + n;
        if !gr_fits(data, at, 4) { return -3; }
        let dims: i64 = gr_bits(data, at, 4) as i64; at += 4;
        if dims > 4 || !gr_fits(data, at, 8 * dims + 12) { return -3; }
        tensors[base + 3] = dims; var d: i64 = 0; var elements: i64 = 1;
        while d < 4 {
            var extent: i64 = 1;
            if d < dims {
                let raw: u64 = gr_bits(data, at, 8); if raw > 9223372036854775807 { return -6; }
                extent = raw as i64; at += 8;
            }
            if extent > 0 && elements > 9223372036854775807 / extent { return -6; }
            elements *= extent; tensors[base + 4 + d] = extent; d += 1;
        }
        let dtype: i64 = gr_bits(data, at, 4) as i64; at += 4;
        let block_size: i64 = t_blck_size(dtype); let size: i64 = t_type_size(dtype);
        if block_size <= 0 || size <= 0 || tensors[base + 4] % block_size != 0 { return -5; }
        var bytes: i64 = 0;
        if elements > 0 {
            if elements / block_size > 9223372036854775807 / size { return -6; }
            bytes = (elements / block_size) * size;
        }
        let relative: u64 = gr_bits(data, at, 8); at += 8;
        if relative > 9223372036854775807 || (relative as i64) != total || bytes > 9223372036854775807 - (alignment - 1) { return -6; }
        let padded: i64 = (bytes + alignment - 1) & (0 - alignment);
        if total > 9223372036854775807 - padded { return -6; }
        tensors[base + 2] = dtype; tensors[base + 8] = total; tensors[base + 9] = bytes;
        total += padded; i += 1;
    }
    let data_offset: i64 = ggh_data_offset(at, alignment, nt);
    if data_offset < 0 || data_offset > file_bytes || total > file_bytes - data_offset { return -7; }
    i = 0; while i < nt { tensors[i * 12 + 10] = data_offset + tensors[i * 12 + 8]; i += 1; }
    state[1] = version; state[2] = nk; state[3] = nt; state[4] = alignment;
    state[5] = data_offset; state[6] = total; state[7] = at; state[0] = 919;
    return 0;
}

fn gr_lookup(data: const u8[:], state: i64[:], records: i64[:], slots: i64[:],
              key: const u8[:], stride: i64, which: i64) -> i64 {
    if state.len < 8 || state[0] != 919 || !gr_slots_ok(slots, state[which]) || state[which] > records.len / stride { return -1; }
    let h: u64 = gr_hash(key, 0, key.len);
    var slot: i64 = (h & ((slots.len - 1) as u64)) as i64; var visited: i64 = 0;
    while visited < slots.len {
        let entry: i64 = slots[slot]; if entry == 0 { return -1; }
        if entry < 0 || entry > state[which] { return -1; }
        let base: i64 = (entry - 1) * stride;
        let at: i64 = records[base]; let n: i64 = records[base + 1];
        if (records[base + stride - 1] as! u64) == h && n == key.len && gr_fits(data, at, n) && data[at .. at + n] == key { return entry - 1; }
        slot = (slot + 1) & (slots.len - 1); visited += 1;
    }
    return -1;
}

export fn gr_find_kv(data: const u8[:], state: i64[:], kv: i64[:], slots: i64[:], key: const u8[:]) -> i64 {
    return gr_lookup(data, state, kv, slots, key, 8, 2);
}

// Same decision as gr_find_kv for callers inside the unit: the export must keep
// one concrete signature, so internal callers use this body directly.
fn gr_find_kv_impl(data: const u8[:], state: i64[:], kv: i64[:], slots: i64[:], key: const u8[:]) -> i64 {
    return gr_lookup(data, state, kv, slots, key, 8, 2);
}

export fn gr_find_tensor(data: const u8[:], state: i64[:], tensors: i64[:], slots: i64[:], key: const u8[:]) -> i64 {
    return gr_lookup(data, state, tensors, slots, key, 12, 3);
}

fn gr_int(data: const u8[:], at: i64, dtype: i64, out: i64[:]) -> i64 {
    let n: i64 = ggh_type_size(dtype);
    if out.len < 1 || n <= 0 || !gr_fits(data, at, n) || dtype == 6 || dtype == 12 { return -1; }
    let raw: u64 = gr_bits(data, at, n); var value: i64 = raw as! i64;
    if dtype == 10 && raw > 9223372036854775807 { return -1; }
    if dtype == 1 && value >= 128 { value -= 256; }
    if dtype == 3 && value >= 32768 { value -= 65536; }
    if dtype == 5 && value >= 2147483648 { value -= 4294967296; }
    if dtype == 7 { value = if raw == 0 { 0 } else { 1 }; }
    out[0] = value; return 0;
}

fn gr_meta_int_impl(data: const u8[:], state: i64[:], kv: i64[:], index: i64, out: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 || kv[index * 8 + 2] == 9 { return -1; }
    return gr_int(data, kv[index * 8 + 4], kv[index * 8 + 2], out);
}

// Raw bytes of one array element, as an integer, for any element type of eight bytes or fewer. Comparing
// elements whose type has no integer interpretation - FLOAT32 and FLOAT64 arrays - needs the bytes themselves:
// a float array read through gr_array_int would be compared as if its bits were an integer of another width
// (EXP-2026-11-19-1400 records that those arrays were not compared at all).
export fn gr_array_raw_bits(data: const u8[:], state: i64[:], kv: i64[:], index: i64, element: i64, out: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 || element < 0 ||
       out.len < 2 || kv[index * 8 + 2] != 9 || element >= kv[index * 8 + 5] { return -1; }
    let dtype: i64 = kv[index * 8 + 3];
    let size: i64 = ggh_type_size(dtype);
    if size <= 0 || size > 8 || kv[index * 8 + 4] < 0 ||
       element > (9223372036854775807 - kv[index * 8 + 4]) / size { return -1; }
    let at: i64 = kv[index * 8 + 4] + element * size;
    if !gr_fits(data, at, size) { return -1; }
    var v: i64 = 0;
    var i: i64 = 0;
    while i < size {
        v = v | ((data[at + i] as i64) << (8 * i));
        i = i + 1;
    }
    out[0] = v;
    out[1] = size;
    return 0;
}

fn gr_array_int_impl(data: const u8[:], state: i64[:], kv: i64[:], index: i64, element: i64, out: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 || element < 0 ||
       kv[index * 8 + 2] != 9 || element >= kv[index * 8 + 5] { return -1; }
    let dtype: i64 = kv[index * 8 + 3]; let size: i64 = ggh_type_size(dtype);
    if size <= 0 || kv[index * 8 + 4] < 0 || element > (9223372036854775807 - kv[index * 8 + 4]) / size { return -1; }
    return gr_int(data, kv[index * 8 + 4] + element * size, dtype, out);
}

fn gr_meta_f32_bits_impl(data: const u8[:], state: i64[:], kv: i64[:], index: i64, out: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 || out.len < 1 ||
       kv[index * 8 + 2] != 6 || !gr_fits(data, kv[index * 8 + 4], 4) { return -1; }
    out[0] = gr_bits(data, kv[index * 8 + 4], 4) as i64; return 0;
}

// Stable public slice ABI. Internal model consumers call the private bodies,
// allowing literal/view specialization without changing the C entry signatures.
export fn gr_meta_int(data: const u8[:], state: i64[:], kv: i64[:], index: i64, out: i64[:]) -> i64 {
    return gr_meta_int_impl(data, state, kv, index, out);
}

export fn gr_array_int(data: const u8[:], state: i64[:], kv: i64[:], index: i64, element: i64, out: i64[:]) -> i64 {
    return gr_array_int_impl(data, state, kv, index, element, out);
}

export fn gr_meta_f32_bits(data: const u8[:], state: i64[:], kv: i64[:], index: i64, out: i64[:]) -> i64 {
    return gr_meta_f32_bits_impl(data, state, kv, index, out);
}

export fn gr_string_array_index(data: const u8[:], state: i64[:], kv: i64[:], index: i64, pairs: i64[:]) -> i64 {
    return gr_string_array_index_impl(data, state, kv, index, pairs);
}

fn gr_string_array_index_impl(data: const u8[:], state: i64[:], kv: i64[:], index: i64, pairs: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       kv[index * 8 + 2] != 9 || kv[index * 8 + 3] != 8 { return -1; }
    let count: i64 = kv[index * 8 + 5]; if count < 0 || count > pairs.len / 2 { return -1; }
    var at: i64 = kv[index * 8 + 4]; var i: i64 = 0;
    while i < count { let n: i64 = gr_length(data, at); if n < 0 { return -1; } at += 8 + n; i += 1; }
    at = kv[index * 8 + 4]; i = 0;
    while i < count { let n: i64 = gr_length(data, at); pairs[i * 2] = at + 8; pairs[i * 2 + 1] = n; at += 8 + n; i += 1; }
    return count;
}

// Header-field readers for a host that keeps its own container but wants the
// Goose reader to answer the metadata questions. Index i is the key's position
// in the file, which is what both readers use.
export fn gr_kv_type(state: i64[:], kv: i64[:], index: i64) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 {
        return -1;
    }
    return kv[index * 8 + 2];
}

export fn gr_arr_type(state: i64[:], kv: i64[:], index: i64) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       kv[index * 8 + 2] != 9 {
        return -1;
    }
    return kv[index * 8 + 3];
}

export fn gr_arr_n(state: i64[:], kv: i64[:], index: i64) -> i64 {
    return gr_arr_n_impl(state, kv, index);
}

fn gr_arr_n_impl(state: i64[:], kv: i64[:], index: i64) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       kv[index * 8 + 2] != 9 {
        return -1;
    }
    return kv[index * 8 + 5];
}

export fn gr_n_kv(state: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 {
        return -1;
    }
    return state[2];
}

// Payload spans for a host that keeps reading values from its own container but
// wants the Goose reader to say where they are. Offsets are relative to the
// header window the reader parsed; a value outside it is refused.
// Descriptor of one tensor, as the loader reads it: [0] dtype, [1] dimension count, [2..5] extents,
// [6] element count, [7] byte count, [8] data offset. The records already hold all of this; nothing
// exposed it, so a host could find a tensor by name and not ask what it was (EXP-2026-11-19-1327).
export fn gr_tensor_info(state: i64[:], tensors: i64[:], index: i64, out: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[3] ||
       index >= tensors.len / 12 || out.len < 9 {
        return -1;
    }
    let base: i64 = index * 12;
    out[0] = tensors[base + 2];
    out[1] = tensors[base + 3];
    var d: i64 = 0;
    while d < 4 {
        out[2 + d] = tensors[base + 4 + d];
        d = d + 1;
    }
    out[6] = tensors[base + 8];
    out[7] = tensors[base + 9];
    out[8] = tensors[base + 10];
    return 0;
}

export fn gr_val_span(state: i64[:], kv: i64[:], index: i64, out: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 || out.len < 4 {
        return -1;
    }
    out[0] = kv[index * 8 + 2]; // wire type
    out[1] = kv[index * 8 + 4]; // payload offset
    out[2] = kv[index * 8 + 6]; // payload bytes
    out[3] = kv[index * 8 + 3]; // element type, -1 for scalars
    return 0;
}

// Span of element `i` of a string array, as (offset, length) into the header.
export fn gr_arr_str_span(data: const u8[:], state: i64[:], kv: i64[:], index: i64, element: i64, out: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       out.len < 2 || kv[index * 8 + 2] != 9 || kv[index * 8 + 3] != 8 || element < 0 ||
       element >= kv[index * 8 + 5] {
        return -1;
    }
    var at: i64 = kv[index * 8 + 4];
    var i: i64 = 0;
    while i < element {
        if !gr_fits(data, at, 8) {
            return -1;
        }
        let n: u64 = gr_bits(data, at, 8);
        if n > 9223372036854775807 {
            return -1;
        }
        at = at + 8 + (n as! i64);
        i = i + 1;
    }
    if !gr_fits(data, at, 8) {
        return -1;
    }
    let len: u64 = gr_bits(data, at, 8);
    if len > 9223372036854775807 || !gr_fits(data, at + 8, len as! i64) {
        return -1;
    }
    out[0] = at + 8;
    out[1] = len as! i64;
    return 0;
}

// Offsets and lengths of every element of a string array, taken from the file's own window in one pass.
// gr_arr_str_span answers the same question for one element but walks from the array start each call, which is
// quadratic over a vocabulary of 248 320 tokens; gr_arr_str_table copies the text, which a caller that indexes the
// file window cannot use (EXP-2026-11-19-1389, -1390).
export fn gr_arr_str_offsets(data: const u8[:], state: i64[:], kv: i64[:], index: i64,
                             offs: i64[:], lens: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       kv[index * 8 + 2] != 9 || kv[index * 8 + 3] != 8 {
        return -1;
    }
    let n: i64 = kv[index * 8 + 5];
    if offs.len < n || lens.len < n {
        return -2;
    }
    var at: i64 = kv[index * 8 + 4];
    var i: i64 = 0;
    while i < n {
        if !gr_fits(data, at, 8) {
            return -1;
        }
        let len: u64 = gr_bits(data, at, 8);
        if len > 9223372036854775807 || !gr_fits(data, at + 8, len as! i64) {
            return -1;
        }
        offs[i] = at + 8;
        lens[i] = len as! i64;
        at = at + 8 + (len as! i64);
        i = i + 1;
    }
    return n;
}

// Lay out a whole GGUF string array as NUL-terminated C strings in one pass.
// GGUF strings are length-prefixed and carry no terminator, so the terminator
// is written here, where the length is known; a consumer that treats the mapped
// bytes as a C string would otherwise run past the element into the next
// entry's length word. `offs[i]` receives the offset of element i inside
// `text`, which holds every entry back to back. Returns the element count, or
// -(needed + 1) when `text` is smaller than needed, or -1 on a bad span, or -2
// when `offs` is too small.
export fn gr_arr_str_table(data: const u8[:], state: i64[:], kv: i64[:], index: i64,
                           text: u8[:], offs: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       kv[index * 8 + 2] != 9 || kv[index * 8 + 3] != 8 {
        return -1;
    }
    let n: i64 = kv[index * 8 + 5];
    if offs.len < n {
        return -2;
    }
    var at: i64 = kv[index * 8 + 4];
    var total: i64 = 0;
    var i: i64 = 0;
    while i < n {
        if !gr_fits(data, at, 8) {
            return -1;
        }
        let len: u64 = gr_bits(data, at, 8);
        if len > 9223372036854775807 || !gr_fits(data, at + 8, len as! i64) {
            return -1;
        }
        offs[i] = total;
        total = total + (len as! i64) + 1;
        at = at + 8 + (len as! i64);
        i = i + 1;
    }
    if text.len < total {
        return -(total + 1);
    }
    at = kv[index * 8 + 4];
    i = 0;
    while i < n {
        let len: u64 = gr_bits(data, at, 8);
        let ln: i64 = len as! i64;
        let base: i64 = offs[i];
        var k: i64 = 0;
        while k < ln {
            text[base + k] = data[at + 8 + k];
            k = k + 1;
        }
        text[base + ln] = 0;
        at = at + 8 + ln;
        i = i + 1;
    }
    return n;
}
// Row dequantizers for the remaining block types (P7-04 S5b), from
// ggml/src/ggml-quants.c (llama.cpp-flint-cuda):
//   - dequantize_row_q1_0, dequantize_row_q2_0
//   - dequantize_row_q2_K, dequantize_row_q3_K, dequantize_row_q4_K,
//     dequantize_row_q5_K, dequantize_row_q6_K, dequantize_row_q8_K
//   - dequantize_row_tq1_0, dequantize_row_tq2_0
//   - dequantize_row_mxfp4, dequantize_row_nvfp4
//   - dequantize_row_iq4_nl, dequantize_row_iq4_xs, dequantize_row_iq3_xxs,
//     dequantize_row_iq3_s, dequantize_row_iq2_s, dequantize_row_iq1_m
// and the different-type copy primitives of ggml/src/ggml-cpu/ops.cpp:
//   - ggml_compute_forward_dup_from_q  (quantized -> f32, type traits to_float)
//   - ggml_compute_forward_dup_flt     (f16/bf16 <-> f32 conversions)
//   - ggml_compute_forward_dup_to_q    (f32 -> quantized, type traits from_float)
// for the destination types f16, bf16 and q8_0.
//
// Every scalar expression keeps the native shape and evaluation order: the
// same nibble/bit layout, the same 6-bit k-scale unpacking, the same
// (scale * quant - min) association, the same float d of q8_K, the same
// ternary base-3 decode of tq1_0, the same E8M0 / UE4M3 sub-block scales and
// the same grid-plus-sign decode of the IQ families. Floats are compared bit
// for bit by the gate; the only folded case is the sign of zero, which Goose
// does not carry (documented in README.md).
//
// Host primitives: float bit reinterpretation and the IQ grid tables. The
// grids are built by the native ggml_quantize_init constructors; the port
// owns the decode, not a hand copy of the tables.

extern "qd_bits" fn bits(v: f32) -> i64;
extern "qd_of_bits" fn of_bits(b: i64) -> f32;
extern "qd_iq2_grid" fn iq2_grid(g: i64, i: i64) -> i64;
extern "qd_iq3_grid" fn iq3_grid(g: i64, i: i64) -> i64;

// ---- ggml type ids --------------------------------------------------------

const T_F16: i64 = 1;
const T_Q8_0: i64 = 8;
const T_Q2_K: i64 = 10;
const T_Q3_K: i64 = 11;
const T_Q4_K: i64 = 12;
const T_Q5_K: i64 = 13;
const T_Q6_K: i64 = 14;
const T_Q8_K: i64 = 15;
const T_IQ3_XXS: i64 = 18;
const T_IQ4_NL: i64 = 20;
const T_IQ3_S: i64 = 21;
const T_IQ2_S: i64 = 22;
const T_IQ4_XS: i64 = 23;
const T_IQ1_M: i64 = 29;
const T_BF16: i64 = 30;
const T_TQ1_0: i64 = 34;
const T_TQ2_0: i64 = 35;
const T_MXFP4: i64 = 39;
const T_NVFP4: i64 = 40;
const T_Q1_0: i64 = 41;
const T_Q2_0: i64 = 42;
const T_PQ2_0: i64 = 142;

// ---- geometry -------------------------------------------------------------

const QK_K: i64 = 256;
const QK1_0: i64 = 128;
const QK2_0: i64 = 64;

fn blk_elems(t: i64) -> i64 {
    if t == T_Q1_0 { return 128; }
    if t == T_Q2_0 { return 64; }
    if t == T_PQ2_0 { return 128; }
    if t == T_MXFP4 { return 32; }
    if t == T_IQ4_NL { return 32; }
    if t == T_NVFP4 { return 64; }
    if t == T_Q2_K { return 256; }
    if t == T_Q3_K { return 256; }
    if t == T_Q4_K { return 256; }
    if t == T_Q5_K { return 256; }
    if t == T_Q6_K { return 256; }
    if t == T_Q8_K { return 256; }
    if t == T_TQ1_0 { return 256; }
    if t == T_TQ2_0 { return 256; }
    if t == T_IQ3_XXS { return 256; }
    if t == T_IQ3_S { return 256; }
    if t == T_IQ2_S { return 256; }
    if t == T_IQ4_XS { return 256; }
    if t == T_IQ1_M { return 256; }
    return 0;
}

fn blk_bytes(t: i64) -> i64 {
    if t == T_Q1_0 { return 18; }
    if t == T_Q2_0 { return 18; }
    if t == T_PQ2_0 { return 34; }
    if t == T_Q2_K { return 84; }
    if t == T_Q3_K { return 110; }
    if t == T_Q4_K { return 144; }
    if t == T_Q5_K { return 176; }
    if t == T_Q6_K { return 210; }
    if t == T_Q8_K { return 292; }
    if t == T_TQ1_0 { return 54; }
    if t == T_TQ2_0 { return 66; }
    if t == T_MXFP4 { return 17; }
    if t == T_NVFP4 { return 36; }
    if t == T_IQ4_NL { return 18; }
    if t == T_IQ4_XS { return 136; }
    if t == T_IQ3_XXS { return 98; }
    if t == T_IQ3_S { return 110; }
    if t == T_IQ2_S { return 82; }
    if t == T_IQ1_M { return 56; }
    return 0;
}

// ---- byte loads -----------------------------------------------------------

fn u8at(s: const u8[:], at: i64) -> i64 {
    return s[at] as i64;
}

fn u16at(s: const u8[:], at: i64) -> i64 {
    return (s[at] as i64) | ((s[at + 1] as i64) << 8);
}

fn u32at(s: const u8[:], at: i64) -> i64 {
    return (s[at] as i64) | ((s[at + 1] as i64) << 8) | ((s[at + 2] as i64) << 16) | ((s[at + 3] as i64) << 24);
}

fn i8of(v: i64) -> i64 {
    if v >= 128 { return v - 256; }
    return v;
}

// IEEE half -> float, the native ggml_compute_fp16_to_fp32 bit trick, so
// subnormals, signed zero and inf/NaN carry the same bits as the oracle.
fn half(h: i64) -> f32 {
    let w = (h << 16) & 0xffffffff;
    let sign = w & 0x80000000;
    let two_w = (w + w) & 0xffffffff;

    let norm = of_bits(((two_w >> 4) + 0x70000000) & 0xffffffff) * pow2_neg(112);
    let denorm = of_bits(((two_w >> 17) | 0x3f000000) & 0xffffffff) - 0.5;

    var bitsv = bits(denorm) & 0xffffffff;
    if two_w >= (1 << 27) {
        bitsv = bits(norm) & 0xffffffff;
    }
    return of_bits((sign | bitsv) & 0xffffffff);
}

fn pow2_neg(n: i64) -> f32 {
    var v: f32 = 1.0;
    var k = 0;
    while k < n {
        v = v * 0.5;
        k = k + 1;
    }
    return v;
}

// ---- small numeric tables -------------------------------------------------

// kvalues_mxfp4: E2M1 values doubled, 0..7 positive, 8..15 negative.
fn kv_mxfp4(i: i64) -> i64 {
    let m = i & 7;
    var v: i64 = m;
    if m == 5 { v = 6; }
    if m == 6 { v = 8; }
    if m == 7 { v = 12; }
    if i >= 8 {
        return 0 - v;
    }
    return v;
}

// kvalues_iq4nl, 16 non-linear values.
fn kv_iq4nl(i: i64) -> i64 {
    if i == 0 { return 0 - 127; }
    if i == 1 { return 0 - 104; }
    if i == 2 { return 0 - 83; }
    if i == 3 { return 0 - 65; }
    if i == 4 { return 0 - 49; }
    if i == 5 { return 0 - 35; }
    if i == 6 { return 0 - 22; }
    if i == 7 { return 0 - 10; }
    if i == 8 { return 1; }
    if i == 9 { return 13; }
    if i == 10 { return 25; }
    if i == 11 { return 38; }
    if i == 12 { return 53; }
    if i == 13 { return 69; }
    if i == 14 { return 89; }
    return 113;
}

// ksigns_iq2xs[i] for i in 0..127: bit j is the sign of element j (j = 0..6,
// element 7 carries the parity bit of the index).
fn ksign(i: i64) -> i64 {
    var v = i & 127;
    var k = 0;
    var p = 0;
    while k < 7 {
        p = p ^ ((i >> k) & 1);
        k = k + 1;
    }
    if p != 0 {
        v = v | 128;
    }
    return v;
}

fn sgn(bit_set: bool) -> f32 {
    if bit_set {
        return 0.0 - 1.0;
    }
    return 1.0;
}

// e8m0_to_fp32 / 2, the MXFP4 block scale.
fn e8m0_half(e: i64) -> f32 {
    var b: i64 = 0;
    if e < 2 {
        b = 2097152 << e;
    } else {
        b = (e - 1) << 23;
    }
    return of_bits(b);
}

// ue4m3 (unsigned E4M3, bias 7) scaled by 0.5, the NVFP4 sub-block scale.
fn ue4m3(x: i64) -> f32 {
    if x == 0 || x == 127 {
        return 0.0;
    }
    let e = (x >> 3) & 15;
    let m = x & 7;
    var raw: f32 = 0.0;
    if e == 0 {
        raw = m as f32;
        var k = 0;
        while k < 9 {
            raw = raw * 0.5;
            k = k + 1;
        }
    } else {
        raw = 1.0 + (m as f32) / 8.0;
        var k = e - 7;
        while k > 0 {
            raw = raw * 2.0;
            k = k - 1;
        }
        while k < 0 {
            raw = raw * 0.5;
            k = k + 1;
        }
    }
    return raw * 0.5;
}

fn pow3(i: i64) -> i64 {
    if i == 0 { return 1; }
    if i == 1 { return 3; }
    if i == 2 { return 9; }
    if i == 3 { return 27; }
    if i == 4 { return 81; }
    return 243;
}

fn grid_byte(gv: i64, j: i64) -> i64 {
    return (gv >> (8 * j)) & 255;
}

// 6-bit k-scale unpacking, sc in bits 0..7 and m in bits 16..23.
fn scale_min_k4(src: const u8[:], base: i64, j: i64) -> i64 {
    if j < 4 {
        let sc = u8at(src, base + j) & 63;
        let m = u8at(src, base + j + 4) & 63;
        return sc | (m << 16);
    }
    let sc = (u8at(src, base + j + 4) & 15) | ((u8at(src, base + j - 4) >> 6) << 4);
    let m = (u8at(src, base + j + 4) >> 4) | ((u8at(src, base + j) >> 6) << 4);
    return sc | (m << 16);
}

// unpacked q3_K scale is in bits 0..7, signed.
fn scal8(s0: i64, s1: i64, s2: i64, s3: i64, is: i64) -> i64 {
    let w = is / 4;
    let sh = (is & 3) * 8;
    var v = s0;
    if w == 1 { v = s1; }
    if w == 2 { v = s2; }
    if w == 3 { v = s3; }
    return i8of((v >> sh) & 255);
}

// ---- per-type decoders ----------------------------------------------------

fn dq_q1_0(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK1_0;
    var i: i64 = 0;
    while i < nb {
        let o = i * 18;
        let d = half(u16at(src, o));
        let nd = 0.0 - d;
        var j: i64 = 0;
        while j < QK1_0 {
            let b = u8at(src, o + 2 + j / 8);
            let bit = (b >> (j & 7)) & 1;
            if bit != 0 {
                out[i * QK1_0 + j] = d;
            } else {
                out[i * QK1_0 + j] = nd;
            }
            j = j + 1;
        }
        i = i + 1;
    }
}

fn dq_q2_0(src: const u8[:], n: i64, out: f32[:], elements: i64, bytes: i64) {
    let nb = n / elements;
    var i: i64 = 0;
    while i < nb {
        let o = i * bytes;
        let d = half(u16at(src, o));
        var j: i64 = 0;
        while j < elements {
            let q = (u8at(src, o + 2 + j / 4) >> ((j & 3) * 2)) & 3;
            out[i * elements + j] = ((q - 1) as f32) * d;
            j = j + 1;
        }
        i = i + 1;
    }
}

fn dq_q2_K(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 84;
        let d = half(u16at(src, o + 80));
        let mn = half(u16at(src, o + 82));
        let scales = o;
        let qs = o + 16;
        var is = 0;
        var y = i * QK_K;
        var nn: i64 = 0;
        while nn < QK_K {
            let qb = qs + (nn / 128) * 32;
            var j: i64 = 0;
            while j < 4 {
                let shift = j * 2;
                let sc0 = u8at(src, scales + is);
                is = is + 1;
                let dl0 = d * ((sc0 & 15) as f32);
                let ml0 = mn * ((sc0 >> 4) as f32);
                var l: i64 = 0;
                while l < 16 {
                    out[y + l] = dl0 * (((u8at(src, qb + l) >> shift) & 3) as f32) - ml0;
                    l = l + 1;
                }
                y = y + 16;

                let sc1 = u8at(src, scales + is);
                is = is + 1;
                let dl1 = d * ((sc1 & 15) as f32);
                let ml1 = mn * ((sc1 >> 4) as f32);
                l = 0;
                while l < 16 {
                    out[y + l] = dl1 * (((u8at(src, qb + 16 + l) >> shift) & 3) as f32) - ml1;
                    l = l + 1;
                }
                y = y + 16;
                j = j + 1;
            }
            nn = nn + 128;
        }
        i = i + 1;
    }
}

fn dq_q3_K(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 110;
        let d_all = half(u16at(src, o + 108));

        let a0 = u32at(src, o + 96);
        let a1 = u32at(src, o + 100);
        let tmp = u32at(src, o + 104);
        let s0 = (a0 & 0x0f0f0f0f) | (((tmp >> 0) & 0x03030303) << 4);
        let s1 = (a1 & 0x0f0f0f0f) | (((tmp >> 2) & 0x03030303) << 4);
        let s2 = ((a0 >> 4) & 0x0f0f0f0f) | (((tmp >> 4) & 0x03030303) << 4);
        let s3 = ((a1 >> 4) & 0x0f0f0f0f) | (((tmp >> 6) & 0x03030303) << 4);

        var y = i * QK_K;
        var is = 0;
        var nn: i64 = 0;
        while nn < QK_K {
            let qb = o + 32 + (nn / 128) * 32;
            let hb = o;
            var j: i64 = 0;
            while j < 4 {
                let mbit = 1 << ((nn / 128) * 4 + j);
                let dl0 = d_all * ((scal8(s0, s1, s2, s3, is) - 32) as f32);
                is = is + 1;
                var l: i64 = 0;
                while l < 16 {
                    let q = (u8at(src, qb + l) >> (j * 2)) & 3;
                    var hv = 0;
                    if (u8at(src, hb + l) & mbit) == 0 { hv = 4; }
                    out[y + l] = dl0 * ((q - hv) as f32);
                    l = l + 1;
                }
                y = y + 16;

                let dl1 = d_all * ((scal8(s0, s1, s2, s3, is) - 32) as f32);
                is = is + 1;
                l = 0;
                while l < 16 {
                    let q = (u8at(src, qb + 16 + l) >> (j * 2)) & 3;
                    var hv = 0;
                    if (u8at(src, hb + 16 + l) & mbit) == 0 { hv = 4; }
                    out[y + l] = dl1 * ((q - hv) as f32);
                    l = l + 1;
                }
                y = y + 16;
                j = j + 1;
            }
            nn = nn + 128;
        }
        i = i + 1;
    }
}

fn dq_q4_K(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 144;
        let d = half(u16at(src, o));
        let mn = half(u16at(src, o + 2));
        let scales = o + 4;
        let qs = o + 16;
        var is = 0;
        var y = i * QK_K;
        var j: i64 = 0;
        while j < QK_K {
            let p0 = scale_min_k4(src, scales, is + 0);
            let p1 = scale_min_k4(src, scales, is + 1);
            let d1 = d * ((p0 & 255) as f32);
            let m1 = mn * (((p0 >> 16) & 255) as f32);
            let d2 = d * ((p1 & 255) as f32);
            let m2 = mn * (((p1 >> 16) & 255) as f32);
            let qb = qs + j / 2;
            var l: i64 = 0;
            while l < 32 {
                out[y + l] = d1 * ((u8at(src, qb + l) & 15) as f32) - m1;
                l = l + 1;
            }
            l = 0;
            while l < 32 {
                out[y + 32 + l] = d2 * ((u8at(src, qb + l) >> 4) as f32) - m2;
                l = l + 1;
            }
            y = y + 64;
            is = is + 2;
            j = j + 64;
        }
        i = i + 1;
    }
}

fn dq_q5_K(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 176;
        let d = half(u16at(src, o));
        let mn = half(u16at(src, o + 2));
        let scales = o + 4;
        let qh = o + 16;
        let qs = o + 48;
        var is = 0;
        var y = i * QK_K;
        var j: i64 = 0;
        while j < QK_K {
            let p0 = scale_min_k4(src, scales, is + 0);
            let p1 = scale_min_k4(src, scales, is + 1);
            let d1 = d * ((p0 & 255) as f32);
            let m1 = mn * (((p0 >> 16) & 255) as f32);
            let d2 = d * ((p1 & 255) as f32);
            let m2 = mn * (((p1 >> 16) & 255) as f32);
            let u1 = 1 << (j / 64 * 2);
            let u2 = 2 << (j / 64 * 2);
            let qb = qs + j / 2;
            var l: i64 = 0;
            while l < 32 {
                var extra = 0;
                if (u8at(src, qh + l) & u1) != 0 { extra = 16; }
                out[y + l] = d1 * (((u8at(src, qb + l) & 15) + extra) as f32) - m1;
                l = l + 1;
            }
            l = 0;
            while l < 32 {
                var extra = 0;
                if (u8at(src, qh + l) & u2) != 0 { extra = 16; }
                out[y + 32 + l] = d2 * (((u8at(src, qb + l) >> 4) + extra) as f32) - m2;
                l = l + 1;
            }
            y = y + 64;
            is = is + 2;
            j = j + 64;
        }
        i = i + 1;
    }
}

fn dq_q6_K(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 210;
        let d = half(u16at(src, o + 208));
        var y = i * QK_K;
        var nn: i64 = 0;
        while nn < QK_K {
            let ql = o + (nn / 128) * 64;
            let qh = o + 128 + (nn / 128) * 32;
            let sc = o + 192 + (nn / 128) * 8;
            var l: i64 = 0;
            while l < 32 {
                let is = l / 16;
                let q1 = (((u8at(src, ql + l) & 15) | (((u8at(src, qh + l) >> 0) & 3) << 4)) - 32) as f32;
                let q2 = (((u8at(src, ql + l + 32) & 15) | (((u8at(src, qh + l) >> 2) & 3) << 4)) - 32) as f32;
                let q3 = (((u8at(src, ql + l) >> 4) | (((u8at(src, qh + l) >> 4) & 3) << 4)) - 32) as f32;
                let q4 = (((u8at(src, ql + l + 32) >> 4) | (((u8at(src, qh + l) >> 6) & 3) << 4)) - 32) as f32;
                out[y + l] = d * (i8of(u8at(src, sc + is)) as f32) * q1;
                out[y + l + 32] = d * (i8of(u8at(src, sc + is + 2)) as f32) * q2;
                out[y + l + 64] = d * (i8of(u8at(src, sc + is + 4)) as f32) * q3;
                out[y + l + 96] = d * (i8of(u8at(src, sc + is + 6)) as f32) * q4;
                l = l + 1;
            }
            y = y + 128;
            nn = nn + 128;
        }
        i = i + 1;
    }
}

fn dq_q8_K(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 292;
        let d = of_bits(u32at(src, o));
        var j: i64 = 0;
        while j < QK_K {
            out[i * QK_K + j] = d * (i8of(u8at(src, o + 4 + j)) as f32);
            j = j + 1;
        }
        i = i + 1;
    }
}

fn dq_tq1_0(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 54;
        let d = half(u16at(src, o + 52));
        var y = i * QK_K;
        // qs is 48 bytes: one 32-byte group of 5 base-3 planes, then one
        // 16-byte group (sizeof(qs) - sizeof(qs) % 32 == 32).
        var j: i64 = 0;
        while j < 32 {
            var nn: i64 = 0;
            while nn < 5 {
                var m: i64 = 0;
                while m < 32 {
                    let q = (u8at(src, o + j + m) * pow3(nn)) & 255;
                    let xi = (q * 3) >> 8;
                    out[y] = ((xi - 1) as f32) * d;
                    y = y + 1;
                    m = m + 1;
                }
                nn = nn + 1;
            }
            j = j + 32;
        }
        j = 32;
        while j < 48 {
            var nn: i64 = 0;
            while nn < 5 {
                var m: i64 = 0;
                while m < 16 {
                    let q = (u8at(src, o + j + m) * pow3(nn)) & 255;
                    let xi = (q * 3) >> 8;
                    out[y] = ((xi - 1) as f32) * d;
                    y = y + 1;
                    m = m + 1;
                }
                nn = nn + 1;
            }
            j = j + 16;
        }
        var nn: i64 = 0;
        while nn < 4 {
            var m: i64 = 0;
            while m < 4 {
                let q = (u8at(src, o + 48 + m) * pow3(nn)) & 255;
                let xi = (q * 3) >> 8;
                out[y] = ((xi - 1) as f32) * d;
                y = y + 1;
                m = m + 1;
            }
            nn = nn + 1;
        }
        i = i + 1;
    }
}

fn dq_tq2_0(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 66;
        let d = half(u16at(src, o + 64));
        var y = i * QK_K;
        var j: i64 = 0;
        while j < 64 {
            var l: i64 = 0;
            while l < 4 {
                var m: i64 = 0;
                while m < 32 {
                    let q = (u8at(src, o + j + m) >> (l * 2)) & 3;
                    out[y] = ((q - 1) as f32) * d;
                    y = y + 1;
                    m = m + 1;
                }
                l = l + 1;
            }
            j = j + 32;
        }
        i = i + 1;
    }
}

fn dq_mxfp4(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / 32;
    var i: i64 = 0;
    while i < nb {
        let o = i * 17;
        let d = e8m0_half(u8at(src, o));
        var j: i64 = 0;
        while j < 16 {
            let x0 = kv_mxfp4(u8at(src, o + 1 + j) & 15);
            let x1 = kv_mxfp4(u8at(src, o + 1 + j) >> 4);
            out[i * 32 + j] = (x0 as f32) * d;
            out[i * 32 + j + 16] = (x1 as f32) * d;
            j = j + 1;
        }
        i = i + 1;
    }
}

fn dq_nvfp4(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / 64;
    var i: i64 = 0;
    while i < nb {
        let o = i * 36;
        var s: i64 = 0;
        while s < 4 {
            let d = ue4m3(u8at(src, o + s));
            var j: i64 = 0;
            while j < 8 {
                let qb = u8at(src, o + 4 + s * 8 + j);
                let x0 = kv_mxfp4(qb & 15);
                let x1 = kv_mxfp4(qb >> 4);
                out[i * 64 + s * 16 + j] = (x0 as f32) * d;
                out[i * 64 + s * 16 + j + 8] = (x1 as f32) * d;
                j = j + 1;
            }
            s = s + 1;
        }
        i = i + 1;
    }
}

fn dq_iq4_nl(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / 32;
    var i: i64 = 0;
    while i < nb {
        let o = i * 18;
        let d = half(u16at(src, o));
        var j: i64 = 0;
        while j < 16 {
            let q = u8at(src, o + 2 + j);
            out[i * 32 + j] = d * (kv_iq4nl(q & 15) as f32);
            out[i * 32 + j + 16] = d * (kv_iq4nl(q >> 4) as f32);
            j = j + 1;
        }
        i = i + 1;
    }
}

fn dq_iq4_xs(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 136;
        let d = half(u16at(src, o));
        let sh = u16at(src, o + 2);
        var ib: i64 = 0;
        while ib < 8 {
            let ls = ((u8at(src, o + 4 + ib / 2) >> (4 * (ib & 1))) & 15) | (((sh >> (2 * ib)) & 3) << 4);
            let dl = d * ((ls - 32) as f32);
            var j: i64 = 0;
            while j < 16 {
                let qb = u8at(src, o + 8 + ib * 16 + j);
                out[i * QK_K + ib * 32 + j] = dl * (kv_iq4nl(qb & 15) as f32);
                out[i * QK_K + ib * 32 + j + 16] = dl * (kv_iq4nl(qb >> 4) as f32);
                j = j + 1;
            }
            ib = ib + 1;
        }
        i = i + 1;
    }
}

fn dq_iq3_xxs(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 98;
        let d = half(u16at(src, o));
        var ib32: i64 = 0;
        while ib32 < 8 {
            // grid codes: 8 bytes per 32 elements; scales and signs: one word
            // per 32 elements, stored after QK_K/4 code bytes.
            let a1 = u32at(src, o + 2 + 64 + 4 * ib32);
            let db = d * ((0.5 + ((a1 >> 28) as f32)) * 0.5);
            var l: i64 = 0;
            while l < 4 {
                let g1 = iq3_grid(0, u8at(src, o + 2 + 8 * ib32 + 2 * l));
                let g2 = iq3_grid(0, u8at(src, o + 2 + 8 * ib32 + 2 * l + 1));
                let signs = ksign((a1 >> (7 * l)) & 127);
                var j: i64 = 0;
                while j < 4 {
                    let base = i * QK_K + ib32 * 32 + l * 8;
                    out[base + j] = (db * (grid_byte(g1, j) as f32)) * sgn((signs & (1 << j)) != 0);
                    out[base + j + 4] = (db * (grid_byte(g2, j) as f32)) * sgn((signs & (1 << (j + 4))) != 0);
                    j = j + 1;
                }
                l = l + 1;
            }
            ib32 = ib32 + 1;
        }
        i = i + 1;
    }
}

fn dq_iq3_s(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 110;
        let d = half(u16at(src, o));
        let qs = o + 2;
        let qh = o + 66;
        let sg = o + 74;
        let sc = o + 106;
        var step: i64 = 0;
        while step < 4 {
            let sbyte = u8at(src, sc + step);
            let db1 = d * ((1 + 2 * (sbyte & 15)) as f32);
            let db2 = d * ((1 + 2 * (sbyte >> 4)) as f32);
            var half_at: i64 = 0;
            while half_at < 2 {
                var db = db1;
                if half_at == 1 { db = db2; }
                let qb = qs + step * 16 + half_at * 8;
                let hb = qh + step * 2 + half_at;
                let sb = sg + step * 8 + half_at * 4;
                var l: i64 = 0;
                while l < 4 {
                    let g1 = iq3_grid(1, u8at(src, qb + 2 * l) | ((u8at(src, hb) << (8 - 2 * l)) & 256));
                    let g2 = iq3_grid(1, u8at(src, qb + 2 * l + 1) | ((u8at(src, hb) << (7 - 2 * l)) & 256));
                    let sbyte2 = u8at(src, sb + l);
                    var j: i64 = 0;
                    while j < 4 {
                        let base = i * QK_K + step * 64 + half_at * 32 + l * 8;
                        out[base + j] = (db * (grid_byte(g1, j) as f32)) * sgn((sbyte2 & (1 << j)) != 0);
                        out[base + j + 4] = (db * (grid_byte(g2, j) as f32)) * sgn((sbyte2 & (1 << (j + 4))) != 0);
                        j = j + 1;
                    }
                    l = l + 1;
                }
                half_at = half_at + 1;
            }
            step = step + 1;
        }
        i = i + 1;
    }
}

fn dq_iq2_s(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 82;
        let d = half(u16at(src, o));
        let qs = o + 2;
        let qh = o + 66;
        let sg = o + 2 + 32;
        let sc = o + 74;
        var ib32: i64 = 0;
        while ib32 < 8 {
            let sbyte = u8at(src, sc + ib32);
            let db0 = d * ((0.5 + ((sbyte & 15) as f32)) * 0.25);
            let db1 = d * ((0.5 + ((sbyte >> 4) as f32)) * 0.25);
            var l: i64 = 0;
            while l < 4 {
                var db = db0;
                if l >= 2 { db = db1; }
                let gv = iq2_grid(3, u8at(src, qs + ib32 * 4 + l) | ((u8at(src, qh + ib32) << (8 - 2 * l)) & 0x300));
                let sbyte2 = u8at(src, sg + ib32 * 4 + l);
                var j: i64 = 0;
                while j < 8 {
                    let g = grid_byte(gv, j) as f32;
                    out[i * QK_K + ib32 * 32 + l * 8 + j] = (db * g) * sgn((sbyte2 & (1 << j)) != 0);
                    j = j + 1;
                }
                l = l + 1;
            }
            ib32 = ib32 + 1;
        }
        i = i + 1;
    }
}

fn dq_iq1_m(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 56;
        let qs = o;
        let qh = o + 32;
        let sc = o + 48;
        let s0 = u16at(src, sc);
        let s1 = u16at(src, sc + 2);
        let s2 = u16at(src, sc + 4);
        let s3 = u16at(src, sc + 6);
        let su = (s0 >> 12) | ((s1 >> 8) & 240) | ((s2 >> 4) & 3840) | (s3 & 61440);
        let d = half(su);
        var ib: i64 = 0;
        while ib < 8 {
            let sb = u16at(src, sc + (ib / 2) * 2);
            let dl1 = d * ((2 * ((sb >> (6 * (ib & 1) + 0)) & 7) + 1) as f32);
            let dl2 = d * ((2 * ((sb >> (6 * (ib & 1) + 3)) & 7) + 1) as f32);
            let qb = qs + ib * 4;
            let hb = qh + ib * 2;
            let h0 = u8at(src, hb);
            let h1 = u8at(src, hb + 1);
            let i0 = u8at(src, qb + 0) | ((h0 << 8) & 0x700);
            let i1 = u8at(src, qb + 1) | ((h0 << 4) & 0x700);
            let i2 = u8at(src, qb + 2) | ((h1 << 8) & 0x700);
            let i3 = u8at(src, qb + 3) | ((h1 << 4) & 0x700);
            var j: i64 = 0;
            while j < 32 {
                var gv = iq2_grid(2, i0);
                var dl = dl1;
                var dbit = (h0 & 8) != 0;
                if j >= 8 {
                    gv = iq2_grid(2, i1);
                    dbit = (h0 & 128) != 0;
                }
                if j >= 16 {
                    gv = iq2_grid(2, i2);
                    dl = dl2;
                    dbit = (h1 & 8) != 0;
                }
                if j >= 24 {
                    gv = iq2_grid(2, i3);
                    dl = dl2;
                    dbit = (h1 & 128) != 0;
                }
                var delta: f32 = 0.125;
                if dbit { delta = 0.0 - 0.125; }
                let gs = i8of(grid_byte(gv, j & 7));
                out[i * QK_K + ib * 32 + j] = dl * ((gs as f32) + delta);
                j = j + 1;
            }
            ib = ib + 1;
        }
        i = i + 1;
    }
}

// ---- dispatcher -----------------------------------------------------------

fn impl(t: i64, src: const u8[:], n: i64, out: f32[:]) -> i64 {
    let be = blk_elems(t);
    if be == 0 {
        return 0 - 1;
    }
    if n % be != 0 {
        return 0 - 1;
    }
    let bb = blk_bytes(t);
    if src.len < (n / be) * bb {
        return 0 - 1;
    }
    if out.len < n {
        return 0 - 1;
    }

    if t == T_Q1_0 { dq_q1_0(src, n, out); }
    if t == T_Q2_0 { dq_q2_0(src, n, out, QK2_0, 18); }
    if t == T_PQ2_0 { dq_q2_0(src, n, out, 128, 34); }
    if t == T_Q2_K { dq_q2_K(src, n, out); }
    if t == T_Q3_K { dq_q3_K(src, n, out); }
    if t == T_Q4_K { dq_q4_K(src, n, out); }
    if t == T_Q5_K { dq_q5_K(src, n, out); }
    if t == T_Q6_K { dq_q6_K(src, n, out); }
    if t == T_Q8_K { dq_q8_K(src, n, out); }
    if t == T_TQ1_0 { dq_tq1_0(src, n, out); }
    if t == T_TQ2_0 { dq_tq2_0(src, n, out); }
    if t == T_MXFP4 { dq_mxfp4(src, n, out); }
    if t == T_NVFP4 { dq_nvfp4(src, n, out); }
    if t == T_IQ4_NL { dq_iq4_nl(src, n, out); }
    if t == T_IQ4_XS { dq_iq4_xs(src, n, out); }
    if t == T_IQ3_XXS { dq_iq3_xxs(src, n, out); }
    if t == T_IQ3_S { dq_iq3_s(src, n, out); }
    if t == T_IQ2_S { dq_iq2_s(src, n, out); }
    if t == T_IQ1_M { dq_iq1_m(src, n, out); }
    return n;
}

export fn qd_dequant(t: i64, src: const u8[:], n: i64, out: f32[:]) -> i64 {
    return impl(t, src, n, out);
}

export fn qd_dequant_block_size(t: i64) -> i64 {
    return blk_bytes(t);
}

export fn qd_dequant_elems(t: i64) -> i64 {
    return blk_elems(t);
}

// ---- different-type copy --------------------------------------------------

// quantized -> f32, the to_float leg of ggml_compute_forward_dup_from_q.
export fn qd_copy_to_f32(t: i64, src: const u8[:], n: i64, out: f32[:]) -> i64 {
    if t == T_F16 {
        if src.len < 2 * n || out.len < n { return 0 - 1; }
        var i: i64 = 0;
        while i < n {
            out[i] = half(u16at(src, 2 * i));
            i = i + 1;
        }
        return n;
    }
    if t == T_BF16 {
        if src.len < 2 * n || out.len < n { return 0 - 1; }
        var i: i64 = 0;
        while i < n {
            out[i] = of_bits(u16at(src, 2 * i) << 16);
            i = i + 1;
        }
        return n;
    }
    return impl(t, src, n, out);
}

// f32 -> f16 bits, the exact native bit-trick rounding (round to nearest even,
// overflow to infinity, NaN to 0x7E00).
fn f32_to_f16(f: f32) -> i64 {
    var af = f;
    if af < 0.0 {
        af = 0.0 - af;
    }
    var si: f32 = 1.0;
    var k = 0;
    while k < 112 {
        si = si * 2.0;
        k = k + 1;
    }
    var sz: f32 = 1.0;
    k = 0;
    while k < 110 {
        sz = sz * 0.5;
        k = k + 1;
    }
    var base = (af * si) * sz;

    let w = bits(f) & 0xffffffff;
    let shl1_w = (w + w) & 0xffffffff;
    let sign = w & 0x80000000;
    var bias = shl1_w & 0xff000000;
    if bias < 0x71000000 {
        bias = 0x71000000;
    }

    base = of_bits((bias >> 1) + 0x07800000) + base;
    let bb = bits(base) & 0xffffffff;
    let exp_bits = (bb >> 13) & 0x7c00;
    let mant_bits = bb & 0x0fff;
    let nonsign = exp_bits + mant_bits;
    var outv = nonsign;
    if shl1_w > 0xff000000 {
        outv = 0x7e00;
    }
    return ((sign >> 16) | outv) & 0xffff;
}

// f32 -> bf16 bits, round to nearest even (ggml_compute_fp32_to_bf16).
fn f32_to_bf16(f: f32) -> i64 {
    let u = bits(f) & 0xffffffff;
    if (u & 0x7fffffff) > 0x7f800000 {
        return ((u >> 16) | 64) & 0xffff;
    }
    let r = (u + (0x7fff + ((u >> 16) & 1))) & 0xffffffff;
    return (r >> 16) & 0xffff;
}

fn store_u16(dst: u8[:], at: i64, v: i64) {
    dst[at] = (v & 255) as u8;
    dst[at + 1] = ((v >> 8) & 255) as u8;
}

// round to nearest even, as _mm256_round_ps(x, _MM_ROUND_NEAREST).
fn rne(x: f32) -> i64 {
    var a = x;
    var neg = 0;
    if a < 0.0 {
        a = 0.0 - a;
        neg = 1;
    }
    let t = a as i64;
    let frac = a - (t as f32);
    var r = t;
    if frac > 0.5 {
        r = t + 1;
    } else if frac == 0.5 {
        if (t & 1) != 0 {
            r = t + 1;
        }
    }
    if neg != 0 {
        return 0 - r;
    }
    return r;
}

// f32 -> quantized, the from_float leg of ggml_compute_forward_dup_to_q.
// Only f16, bf16 and q8_0 are in scope; q8_0 follows the AVX2 from_float:
// d = amax/127, id = 127/amax, round to nearest even.
export fn qd_copy_from_f32(t: i64, src: const f32[:], n: i64, dst: u8[:]) -> i64 {
    if t == T_F16 {
        if dst.len < 2 * n || src.len < n { return 0 - 1; }
        var i: i64 = 0;
        while i < n {
            store_u16(dst, 2 * i, f32_to_f16(src[i]));
            i = i + 1;
        }
        return n;
    }
    if t == T_BF16 {
        if dst.len < 2 * n || src.len < n { return 0 - 1; }
        var i: i64 = 0;
        while i < n {
            store_u16(dst, 2 * i, f32_to_bf16(src[i]));
            i = i + 1;
        }
        return n;
    }
    if t == T_Q8_0 {
        if n % 32 != 0 || src.len < n || dst.len < (n / 32) * 34 { return 0 - 1; }
        let nb = n / 32;
        var b: i64 = 0;
        while b < nb {
            var amax: f32 = 0.0;
            var j: i64 = 0;
            while j < 32 {
                var v = src[b * 32 + j];
                if v < 0.0 {
                    v = 0.0 - v;
                }
                if v > amax {
                    amax = v;
                }
                j = j + 1;
            }
            let d = amax / 127.0;
            var id: f32 = 0.0;
            if amax != 0.0 {
                id = 127.0 / amax;
            }
            store_u16(dst, b * 34, f32_to_f16(d));
            j = 0;
            while j < 32 {
                let q = rne(src[b * 32 + j] * id);
                dst[b * 34 + 2 + j] = (q & 255) as u8;
                j = j + 1;
            }
            b = b + 1;
        }
        return n;
    }
    return 0 - 1;
}
fn mr_name(layer: i64, suffix: const u8[:], out: u8[:]) -> i64 {
    if layer < 0 || layer > 127 || out.len < suffix.len + 8 { return -1; }
    out[0] = 98; out[1] = 108; out[2] = 107; out[3] = 46;
    var div: i64 = if layer >= 100 { 100 } else { if layer >= 10 { 10 } else { 1 } };
    var at: i64 = 4;
    while div > 0 { out[at] = (48 + (layer / div) % 10) as u8; div /= 10; at += 1; }
    out[at] = 46; at += 1; var i: i64 = 0;
    while i < suffix.len { out[at] = suffix[i]; at += 1; i += 1; }
    return at;
}

fn mr_branch_name(kind: i64, i: i64) -> const u8[:] {
    if i == 0 { return "attn_norm.weight"; }
    if kind == 0 {
        if i == 1 { return "attn_q.weight"; } if i == 2 { return "attn_k.weight"; }
        if i == 3 { return "attn_v.weight"; } if i == 4 { return "attn_output.weight"; }
        if i == 5 { return "attn_q_norm.weight"; } if i == 6 { return "attn_k_norm.weight"; }
        return "post_attention_norm.weight";
    }
    if i == 1 { return "attn_qkv.weight"; } if i == 2 { return "attn_gate.weight"; }
    if i == 3 { return "ssm_beta.weight"; } if i == 4 { return "ssm_alpha.weight"; }
    if i == 5 { return "ssm_dt.bias"; } if i == 6 { return "ssm_a"; }
    if i == 7 { return "ssm_conv1d.weight"; } if i == 8 { return "ssm_norm.weight"; }
    if i == 9 { return "ssm_out.weight"; } return "post_attention_norm.weight";
}

// Bind the initial dense Bonsai profile to validated GGUF tensor indices.
// The base trunk and the MTP tail have different matrix encodings.
fn bonsai_tensor(data: const u8[:], state: i64[:], tensors: i64[:], slots: i64[:],
                 name: const u8[:], dtype: i64, columns: i64, rows: i64) -> i64 {
    let index = gr_find_tensor(data, state, tensors, slots, name);
    if index < 0 { return -1; }
    let at = index * 12;
    if tensors[at + 2] != dtype || tensors[at + 4] != columns ||
       tensors[at + 5] != rows || tensors[at + 6] != 1 || tensors[at + 7] != 1 { return -1; }
    return index;
}

fn bonsai_branch_dims(full: bool, i: i64, matrix: i64, dims: i64[:]) {
    dims[0] = 0; dims[1] = 5120; dims[2] = 1;
    if full {
        if i >= 1 && i <= 4 {
            dims[0] = matrix;
            dims[2] = if i == 1 { 12288 } else { 1024 };
            if i == 4 { dims[1] = 6144; dims[2] = 5120; }
        }
        if i == 5 || i == 6 { dims[1] = 256; }
    } else {
        if i == 1 || i == 2 {
            dims[0] = matrix;
            dims[2] = if i == 1 { 10240 } else { 6144 };
        }
        if i == 3 || i == 4 { dims[2] = 48; }
        if i == 5 || i == 6 { dims[1] = 48; }
        if i == 7 { dims[1] = 4; dims[2] = 10240; }
        if i == 8 { dims[1] = 128; }
        if i == 9 { dims[0] = matrix; dims[1] = 6144; dims[2] = 5120; }
    }
}

fn bonsai_integer(data: const u8[:], state: i64[:], kv: i64[:], slots: i64[:],
                  key: const u8[:], expected: i64) -> bool {
    let index = gr_find_kv(data, state, kv, slots, key);
    var value: i64[1] = [0];
    return index >= 0 && gr_meta_int(data, state, kv, index, value[..]) == 0 && value[0] == expected;
}

export fn bonsai_bind_layout(data: const u8[:], state: i64[:], kv: i64[:], tensors: i64[:],
    kv_slots: i64[:], tensor_slots: i64[:], globals: i64[:], layers: i64[:], draft: i64[:]) -> i64 {
    if globals.len < 3 || layers.len < 65 * 15 || draft.len < 4 { return -1; }
    let arch = gr_find_kv(data, state, kv, kv_slots, "general.architecture");
    if arch < 0 || kv[arch * 8 + 2] != 8 || kv[arch * 8 + 5] != 6 { return -1; }
    let arch_at = kv[arch * 8 + 4];
    if !gr_fits(data, arch_at, 6) || data[arch_at .. arch_at+6] != "qwen35" { return -1; }
    if !bonsai_integer(data, state, kv, kv_slots, "qwen35.block_count", 65) ||
       !bonsai_integer(data, state, kv, kv_slots, "qwen35.nextn_predict_layers", 1) ||
       !bonsai_integer(data, state, kv, kv_slots, "qwen35.full_attention_interval", 4) ||
       !bonsai_integer(data, state, kv, kv_slots, "prism.hadamard.block_size", 1024) ||
       !bonsai_integer(data, state, kv, kv_slots, "prism.hadamard.version", 1) { return -1; }

    globals[0] = bonsai_tensor(data, state, tensors, tensor_slots, "token_embd.weight", 142, 5120, 248320);
    globals[1] = bonsai_tensor(data, state, tensors, tensor_slots, "output_norm.weight", 0, 5120, 1);
    globals[2] = bonsai_tensor(data, state, tensors, tensor_slots, "output.weight", 142, 5120, 248320);
    for i in 3 { if globals[i] < 0 { return -2; } }

    var name: u8[64] = [0; 64];
    var dims: i64[3] = [0; 3];
    for layer in 65 {
        let full = layer % 4 == 3 || layer == 64;
        let matrix: i64 = if layer == 64 { 2 } else { 142 };
        let at = layer * 15;
        layers[at] = if full { 0 } else { 1 };
        for i in 11 { layers[at + 1 + i] = -1; }
        for i in (if full { 8 } else { 11 }) {
            let length = mr_name(layer, mr_branch_name(layers[at], i), name[..]);
            if length < 0 { return -2; }
            bonsai_branch_dims(full, i, matrix, dims[..]);
            let index = bonsai_tensor(data, state, tensors, tensor_slots, name[0..length], dims[0], dims[1], dims[2]);
            if index < 0 { return -2; }
            layers[at + 1 + i] = index;
        }
        for i in 3 {
            let suffix = if i == 0 { "ffn_gate.weight" } else { if i == 1 { "ffn_up.weight" } else { "ffn_down.weight" } };
            let length = mr_name(layer, suffix, name[..]);
            if length < 0 { return -2; }
            let index = bonsai_tensor(data, state, tensors, tensor_slots, name[0..length],
                matrix, if i == 2 { 17408 } else { 5120 }, if i == 2 { 5120 } else { 17408 });
            if index < 0 { return -2; }
            layers[at + 12 + i] = index;
        }
    }

    draft[0] = bonsai_tensor(data, state, tensors, tensor_slots, "blk.64.nextn.eh_proj.weight", 2, 10240, 5120);
    draft[1] = bonsai_tensor(data, state, tensors, tensor_slots, "blk.64.nextn.enorm.weight", 0, 5120, 1);
    draft[2] = bonsai_tensor(data, state, tensors, tensor_slots, "blk.64.nextn.hnorm.weight", 0, 5120, 1);
    draft[3] = bonsai_tensor(data, state, tensors, tensor_slots, "blk.64.nextn.shared_head_norm.weight", 0, 5120, 1);
    for i in 4 { if draft[i] < 0 { return -2; } }
    return 64;
}
// Prism v1 stores weight rows in a signed, block-normalized Hadamard basis.
export fn bonsai_signed_fwht(source: const f32[:], signs: const i32[:], output: f32[:],
                            inverse: bool, head_width: i64, key_heads: i64, repeat: i64) -> i64 {
    let width = source.len;
    let tile_width: i64 = 1024;
    if width == 0 || width % tile_width != 0 || signs.len != width || output.len != width { return -1; }
    let grouped = repeat > 1;
    if grouped && (inverse || head_width <= 0 || key_heads <= 0 || width % head_width != 0 ||
       width / head_width % key_heads != 0 || width / head_width / key_heads != repeat) { return -1; }
    for i in width {
        if signs[i] != 1 && signs[i] != -1 { return -1; }
    }
    for i in width {
        var at = i;
        if grouped {
            let head = i / head_width;
            at = (head % repeat * key_heads + head / repeat) * head_width + i % head_width;
        }
        output[i] = if inverse { source[at] } else { source[at] * (signs[i] as f32) };
    }
    for tile in width / tile_width {
        let base = tile * tile_width;
        var stride: i64 = 1;
        while stride < tile_width {
            var first = base;
            while first < base + tile_width {
                for j in stride {
                    let at = first + j;
                    let a = output[at];
                    let b = output[at + stride];
                    output[at] = a + b;
                    output[at + stride] = a - b;
                }
                first += 2 * stride;
            }
            stride *= 2;
        }
    }
    for i in width {
        let value = output[i] * 0.03125;
        output[i] = if inverse { value * (signs[i] as f32) } else { value };
    }
    return width;
}
fn bt_eog_text(gguf: const u8[:], off: i64, len: i64) -> bool {
    if len <= 0 || off < 0 || off + len > gguf.len {
        return false;
    }
    let t: const u8[:] = gguf[off .. off + len];
    return t == "<|eot_id|>" || t == "<|eot|>" || t == "<|im_end|>" || t == "<|end|>"
        || t == "<|return|>" || t == "<|call|>" || t == "<|flush|>" || t == "<|calls|>"
        || t == "<end_of_turn>" || t == "<|endoftext|>" || t == "</s>"
        || t == "<|eom_id|>" || t == "<EOT>" || t == "_<EOT>" || t == "[EOT]"
        || t == "[EOS]" || t == "<|end_of_text|>" || t == "<end_of_utterance>"
        || t == "<eos>" || t == "<turn|>" || t == "<|tool_response>"
        || t == "＜end▁of▁sentence｜>"
        || t == "[e~[" || t == "<|ifm|im_end|>";
}

// 1 prefix, 2 suffix, 3 middle, 4 pad, 5 repo, 6 separator, 0 none.
fn bt_fim_class(gguf: const u8[:], off: i64, len: i64) -> i64 {
    if len <= 0 || off < 0 || off + len > gguf.len {
        return 0;
    }
    let t: const u8[:] = gguf[off .. off + len];
    if (t == "<|fim_prefix|>" || t == "<fim-prefix>" || t == "<fim_prefix>" || t == "<PRE>"
        || t == "▁<PRE>" || t == "<|code_prefix|>" || t == "<|prefix|>"
        || t == "<|plamo:fim_prefix|>") {
        return 1;
    }
    if (t == "<|fim_suffix|>" || t == "<fim-suffix>" || t == "<fim_suffix>"
        || t == "＜fim▁hole｜" || t == "<SUF>" || t == "▁<SUF>"
        || t == "<|code_suffix|>" || t == "<|suffix|>" || t == "<|plamo:fim_suffix|>") {
        return 2;
    }
    if (t == "<|fim_middle|>" || t == "<fim-middle>" || t == "<fim_middle>"
        || t == "＜fim▁end｜" || t == "<MID>" || t == "▁<MID>"
        || t == "<|code_middle|>" || t == "<|middle|>" || t == "<|plamo:fim_middle|>") {
        return 3;
    }
    if (t == "<|fim_pad|>" || t == "<fim-pad>" || t == "<fim_pad>" || t == "<PAD>" || t == "[PAD]") {
        return 4;
    }
    if (t == "<|fim_repo|>" || t == "<|repo_name|>" || t == "<fim-repo>" || t == "<REPO>"
        || t == "<reponame>") {
        return 5;
    }
    if (t == "<|file_sep|>" || t == "<|plamo:file_separator|>") {
        return 6;
    }
    return 0;
}

export fn bt_eog_ids(gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:],
                     eos_id: i64, eot_id: i64, eom_id: i64, out: i64[:]) -> i64 {
    return bt_eog_ids_impl(gguf, tok_off, tok_len, eos_id, eot_id, eom_id, out);
}

fn bt_eog_ids_impl(gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:],
                   eos_id: i64, eot_id: i64, eom_id: i64, out: i64[:]) -> i64 {
    var n: i64 = 0;
    var i: i64 = 0;
    while i < tok_off.len {
        var hit: bool = bt_eog_text(gguf, tok_off[i], tok_len[i]);
        if !hit {
            let c: i64 = bt_fim_class(gguf, tok_off[i], tok_len[i]);
            hit = c == 4 || c == 5 || c == 6;
        }
        if i == eos_id || i == eot_id || i == eom_id {
            hit = true;
        }
        if hit {
            if n >= out.len {
                return 0 - (n + 1);
            }
            out[n] = i;
            n = n + 1;
        }
        i = i + 1;
    }
    return n;
}

// The validated weight slots follow bonsai_bind_layout. Activations and state
// are CUDA addresses, and only the scalar device driver ABI crosses this unit.
extern "bonsai_launch" fn b_launch(context: u64, op: i64, a: i64, b: i64, c: i64,
    d: i64, e: i64, f: i64, g: i64, h: i64) -> i64;

fn b_norm(c: u64, x: i64, w: i64, gate: i64, y: i64, n: i64, rows: i64, mode: i64) -> i64 {
    return b_launch(c, 0, x, w, gate, y, n, rows, mode, 0);
}
fn b_rotate(c: u64, x: i64, sign: i64, y: i64, n: i64, inverse: i64, grouped: i64) -> i64 {
    return b_launch(c, 1, x, sign, y, n, inverse, grouped, 0, 0);
}
fn b_mat(c: u64, w: i64, x: i64, y: i64, n: i64, rows: i64) -> i64 {
    return b_launch(c, 2, w, x, y, n, rows, 0, 0, 0);
}
fn b_add(c: u64, x: i64, y: i64, out: i64) -> i64 {
    return b_launch(c, 3, x, y, out, 0, 0, 0, 0, 0);
}

fn b_gdn(c: u64, w: const i64[:], b: i64[:], s: i64[:]) -> i64 {
    // Scratch 1 is primal normalized input and 2 is its Hadamard transform.
    // Slots 3..12 are shared by the two branch types. Slot 23 owns the
    // reusable next-state buffer, which exchanges ownership after launch.
    if b_mat(c, w[2], b[2], b[3], 5120, 10240) != 0 ||
       b_mat(c, w[3], b[2], b[4], 5120, 6144) != 0 ||
       b_launch(c, 4, b[1], w[5], w[4], w[6], w[7], b[5], b[6], 0) != 0 ||
       b_launch(c, 5, b[3], w[8], s[0], b[7], 0, 0, 0, 0) != 0 ||
       b_norm(c, b[7], 0, 0, b[8], 128, 32, 1) != 0 ||
       b_launch(c, 6, b[8], b[8] + 8192, b[7] + 16384, b[5], b[6], s[1], b[9], b[23]) != 0 {
        return -1;
    }
    let old = s[1];
    s[1] = b[23]; b[23] = old;
    if b_norm(c, b[9], w[9], b[4], b[10], 128, 48, 2) != 0 ||
       b_rotate(c, b[10], b[21], b[11], 6144, 0, 1) != 0 ||
       b_mat(c, w[10], b[11], b[12], 6144, 5120) != 0 { return -1; }
    return 0;
}

fn b_attention(c: u64, w: const i64[:], b: i64[:], s: const i64[:], position: i64) -> i64 {
    if b_mat(c, w[2], b[2], b[3], 5120, 12288) != 0 ||
       b_mat(c, w[3], b[2], b[4], 5120, 1024) != 0 ||
       b_mat(c, w[4], b[2], b[5], 5120, 1024) != 0 ||
       b_launch(c, 7, b[3], b[6], b[7], 0, 0, 0, 0, 0) != 0 ||
       b_norm(c, b[6], w[6], 0, b[8], 256, 24, 0) != 0 ||
       b_norm(c, b[4], w[7], 0, b[9], 256, 4, 0) != 0 ||
       b_launch(c, 8, b[8], b[10], 24, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 8, b[9], b[11], 4, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 9, b[11], s[0] + position * 576, 0, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 9, b[5], s[1] + position * 576, 0, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 10, b[10], s[0], s[1], b[8], position + 1, 0, 0, 0) != 0 ||
       b_launch(c, 11, b[8], b[7], b[6], 0, 0, 0, 0, 0) != 0 ||
       b_rotate(c, b[6], b[21], b[11], 6144, 0, 0) != 0 ||
       b_mat(c, w[5], b[11], b[12], 6144, 5120) != 0 { return -1; }
    return 0;
}

extern "bonsai_ffn_tail_launch" fn launch_ffn_tail(c: u64, gate: i64, up: i64,
    signs: i64, packed_owner: i64) -> i64;

fn b_ffn_finish(c: u64, b: i64[:]) -> i64 {
    let fused = launch_ffn_tail(c, b[14], b[15], b[22], b[17]);
    if fused > 0 { return 0; }
    if fused < 0 || b_launch(c, 13, b[14], b[15], b[16], 0, 0, 0, 0, 0) != 0 { return -1; }
    return b_rotate(c, b[16], b[22], b[17], 17408, 0, 0);
}

export fn bonsai_graph(c: u64, weights: const i64[:], states: i64[:], globals: const i64[:],
    b: i64[:], token: i64, position: i64, want_logits: i64) -> i64 {
    if weights.len != 64 * 15 || states.len != 64 * 2 || globals.len != 3 || b.len != 24 ||
       token < 0 || token >= 248320 || position < 0 { return -1; }
    if b_launch(c, 12, globals[0], b[1], token, 0, 0, 0, 0, 0) != 0 ||
       b_rotate(c, b[1], b[20], b[0], 5120, 1, 0) != 0 { return -1; }
    for layer in 64 {
        let w = weights[layer * 15 .. layer * 15 + 15];
        let s = states[layer * 2 .. layer * 2 + 2];
        if b_norm(c, b[0], w[1], 0, b[1], 5120, 1, 0) != 0 ||
           b_rotate(c, b[1], b[20], b[2], 5120, 0, 0) != 0 { return -2 - layer; }
        let branch = if w[0] == 1 { b_gdn(c, w, b, s) } else { b_attention(c, w, b, s, position) };
        let post = if w[0] == 1 { w[11] } else { w[8] };
        if branch != 0 ||
           b_add(c, b[0], b[12], b[13]) != 0 ||
           b_norm(c, b[13], post, 0, b[1], 5120, 1, 0) != 0 ||
           b_rotate(c, b[1], b[20], b[2], 5120, 0, 0) != 0 ||
           b_mat(c, w[12], b[2], b[14], 5120, 17408) != 0 ||
           b_mat(c, w[13], b[2], b[15], 5120, 17408) != 0 ||
           b_ffn_finish(c, b) != 0 ||
           b_mat(c, w[14], b[17], b[18], 17408, 5120) != 0 ||
           b_add(c, b[13], b[18], b[0]) != 0 { return -2 - layer; }
    }
    if want_logits != 0 &&
       (b_norm(c, b[0], globals[1], 0, b[1], 5120, 1, 0) != 0 ||
        b_rotate(c, b[1], b[20], b[2], 5120, 0, 0) != 0 ||
        b_mat(c, globals[2], b[2], b[19], 5120, 248320) != 0) { return -66; }
    return 0;
}
