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
    let p: i64 = w_meta(st, pool, key_off, key_len, types, arr, v0, v1, v2, out, version, 0 as i64);
    return w_pad(out, p, alignment);
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
