import std;
// ---- composed from goose/misc/gguf-headers.goose ----
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

// ---- composed from goose/model/gguf-tensors.goose ----
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
    if t == 41 as i64 {
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

// ---- composed from goose/model/gguf-reader.goose ----
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

// Lay out a whole GGUF string array as NUL-terminated C strings in one pass.
// GGUF strings are length-prefixed and carry no terminator, so the terminator
// is written here, where the length is known; a consumer that treats the mapped
// bytes as a C string would otherwise run into the next entry.
// `offs[i]` receives the offset of element i inside `text`, which holds every
// entry back to back. Returns the element count, or `-(needed + 1)` when `text`
// is smaller than needed, or -1 on a bad span, or -2 when `offs` is too small.
export fn gr_arr_str_table(data: const u8[:], state: i64[:], kv: i64[:], index: i64, text: u8[:], offs: i64[:]) -> i64 {
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

