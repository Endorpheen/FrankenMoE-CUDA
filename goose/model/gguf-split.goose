// GGUF split and merge in Goose (P7-02 S4), from
// tools/gguf-split/gguf-split.cpp: split_params_parse, split_str_to_n_bytes,
// split_strategy::should_split, the per-shard metadata, shard naming,
// gguf_split and gguf_merge.
//
// The file also carries the accepted S1 metadata context and the S3 tensor
// writer, because split and merge emit complete GGUF files; the canonical tree
// keeps every file self-contained. The S1/S2/S3 block below is the accepted
// text from goose/model/gguf-tensors.goose (EXP596, EXP601).
//
// S4 additions, all with the gsp_ prefix for the canon:
//   - gsp_parse: split_params_parse (flags, conflicts, defaults, positions);
//   - gsp_str_to_n_bytes: "128M" / "4G" with the native refusal paths;
//   - gsp_should_split / gsp_split_layout: the strategy decisions without the
//     contexts, including the "one of splits has 0 tensors" refusal;
//   - gsp_split_write: every shard into one caller buffer (metadata + data);
//   - gsp_split_name / gsp_merge_prefix: llama_split_path / llama_split_prefix;
//   - gsp_read_kv / gsp_read_tensors: the reader half that split and merge need
//     (the writer in S1-S3 had no reader counterpart);
//   - gsp_kv_u16 / gsp_kv_u32 / gsp_kv_i32: scalar peeks used for validation;
//   - gsp_merge: gguf_merge over in-memory shards.
//
// Formats and type ids follow the native enums: gguf_type
// (UINT8=0 ... STRING=8, ARRAY=9 ... FLOAT64=12) and ggml_type
// (F32=0, F16=1, Q4_0=2 ... I32=26 ... GGML_TYPE_COUNT=43). The type size and
// block size table is copied from the native ggml_type_traits.
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
// ---- S4: split and merge ---------------------------------------------------
//
// From tools/gguf-split/gguf-split.cpp. The port owns the decisions: the
// size/tensor split strategy, the per-shard metadata, shard naming, the merge
// metadata replay, tensor order restoration and the refusal paths. File IO is
// a buffer parameter: every function takes the whole input file and writes
// shards into a caller buffer, so the gate can run in memory.
//
// Native anchors: split_str_to_n_bytes (74), split_params_parse_ex (92),
// split_params_parse (183), should_split (287), the split_strategy constructor
// (216), write (315), gguf_split (369) and gguf_merge (407).
//
// Shard metadata order, byte for byte like native:
//   shard 0: every input KV except split.*, then split.no, split.count,
//            split.tensors.count; later shards carry only the three split keys.
//   merge:   the first shard's list with split.count dropped and re-appended as
//            0, so the merged file keeps split.no and split.tensors.count and
//            cannot be merged a second time.
//
// Output alignment is the native tool's, not the input file's: gguf_init_empty
// fixes 32 and the tool never changes it, so split and merge pad to 32 even
// when the input declares general.alignment 64. The port mirrors that (the
// native output is internally inconsistent in such a case; the gate records it
// rather than fixing it silently).

const GSP_ALIGN: i64 = 32;

const GSP_KV_UINT16: i64 = 2;
const GSP_KV_INT32: i64 = 5;

// ---- little-endian reads ---------------------------------------------------

fn g_u32(b: const u8[:], p: i64) -> i64 {
    return (b[p] as i64) | ((b[p + 1 as i64] as i64) << 8 as i64) |
           ((b[p + 2 as i64] as i64) << 16 as i64) | ((b[p + 3 as i64] as i64) << 24 as i64);
}

fn g_u64(b: const u8[:], p: i64) -> i64 {
    var v: i64 = 0;
    var i: i64 = 0;
    while i < 8 as i64 {
        v = v | ((b[p + i] as i64) << (8 as i64 * i));
        i += 1 as i64;
    }
    return v;
}

fn g_fits(len: i64, p: i64, n: i64) -> bool {
    if p < 0 as i64 || n < 0 as i64 {
        return false;
    }
    return p + n <= len;
}

// Validation shared by every reader entry point: magic, version 2 or 3.
// Returns the offset of the first KV record, or -1.
fn g_head(b: const u8[:]) -> i64 {
    if !g_fits(b.len, 0 as i64, 24 as i64) {
        return -(1 as i64);
    }
    if b[0] != 71 as u8 || b[1] != 71 as u8 || b[2] != 85 as u8 || b[3] != 70 as u8 {
        return -(1 as i64);
    }
    let v: i64 = g_u32(b, 4 as i64);
    if v < 2 as i64 || v > 3 as i64 {
        return -(1 as i64);
    }
    return 24 as i64;
}

// Skip one KV value. `p` is the position after the type field.
recursive fn g_skip_val(b: const u8[:], p: i64, t: i64) -> i64 {
    if t == 8 as i64 {
        if !g_fits(b.len, p, 8 as i64) {
            return -(1 as i64);
        }
        let n: i64 = g_u64(b, p);
        if !g_fits(b.len, p + 8 as i64, n) {
            return -(1 as i64);
        }
        return p + 8 as i64 + n;
    }
    if t == 9 as i64 {
        if !g_fits(b.len, p, 12 as i64) {
            return -(1 as i64);
        }
        let at: i64 = g_u32(b, p);
        let n: i64 = g_u64(b, p + 4 as i64);
        var q: i64 = p + 12 as i64;
        if at == 8 as i64 {
            var i: i64 = 0;
            while i < n {
                q = g_skip_val(b, q, 8 as i64);
                if q < 0 as i64 {
                    return -(1 as i64);
                }
                i += 1 as i64;
            }
            return q;
        }
        if at < 0 as i64 || at > 12 as i64 || at == 9 as i64 {
            return -(1 as i64);
        }
        let esz: i64 = w_type_size(at);
        if esz <= 0 as i64 || n < 0 as i64 || !g_fits(b.len, q, n * esz) {
            return -(1 as i64);
        }
        return q + n * esz;
    }
    if t < 0 as i64 || t > 12 as i64 {
        return -(1 as i64);
    }
    let sz: i64 = w_type_size(t);
    if sz <= 0 as i64 || !g_fits(b.len, p, sz) {
        return -(1 as i64);
    }
    return p + sz;
}

// Offset just after the last KV record, or -1.
fn g_kv_end(b: const u8[:]) -> i64 {
    let p0: i64 = g_head(b);
    if p0 < 0 as i64 {
        return -(1 as i64);
    }
    let n_kv: i64 = g_u64(b, 16 as i64);
    if n_kv < 0 as i64 {
        return -(1 as i64);
    }
    var p: i64 = p0;
    var k: i64 = 0;
    while k < n_kv {
        if !g_fits(b.len, p, 8 as i64) {
            return -(1 as i64);
        }
        let kl: i64 = g_u64(b, p);
        if !g_fits(b.len, p + 8 as i64, kl) {
            return -(1 as i64);
        }
        let tpos: i64 = p + 8 as i64 + kl;
        if !g_fits(b.len, tpos, 4 as i64) {
            return -(1 as i64);
        }
        let t: i64 = g_u32(b, tpos);
        p = g_skip_val(b, tpos + 4 as i64, t);
        if p < 0 as i64 {
            return -(1 as i64);
        }
        k += 1 as i64;
    }
    return p;
}

// Scalar search without materializing the KV list. Signed types are sign
// extended. Returns the value, or -1 when the key is absent or has other type.
fn g_peek(b: const u8[:], key: const u8[:], want: i64) -> i64 {
    let p0: i64 = g_head(b);
    if p0 < 0 as i64 {
        return -(1 as i64);
    }
    let n_kv: i64 = g_u64(b, 16 as i64);
    if n_kv < 0 as i64 {
        return -(1 as i64);
    }
    var p: i64 = p0;
    var k: i64 = 0;
    while k < n_kv {
        if !g_fits(b.len, p, 8 as i64) {
            return -(1 as i64);
        }
        let kl: i64 = g_u64(b, p);
        if !g_fits(b.len, p + 8 as i64, kl) {
            return -(1 as i64);
        }
        let kp: i64 = p + 8 as i64;
        let tpos: i64 = kp + kl;
        if !g_fits(b.len, tpos, 4 as i64) {
            return -(1 as i64);
        }
        let t: i64 = g_u32(b, tpos);
        if kl == key.len && b[kp..kp + kl] == key {
            if t != want {
                return -(1 as i64);
            }
            let sz: i64 = w_type_size(t);
            let vp: i64 = tpos + 4 as i64;
            if sz <= 0 as i64 || !g_fits(b.len, vp, sz) {
                return -(1 as i64);
            }
            var v: i64 = 0;
            var i: i64 = 0;
            while i < sz {
                v = v | ((b[vp + i] as i64) << (8 as i64 * i));
                i += 1 as i64;
            }
            let signed: bool = t == 1 as i64 || t == 3 as i64 || t == 5 as i64 || t == 11 as i64;
            if signed && sz < 8 as i64 && (v & (1 as i64 << (8 as i64 * sz - 1 as i64))) != 0 as i64 {
                v = v - (1 as i64 << (8 as i64 * sz));
            }
            return v;
        }
        p = g_skip_val(b, tpos + 4 as i64, t);
        if p < 0 as i64 {
            return -(1 as i64);
        }
        k += 1 as i64;
    }
    return -(1 as i64);
}

export fn gsp_kv_u16(inp: const u8[:], key: const u8[:]) -> i64 {
    return g_peek(inp, key, GSP_KV_UINT16);
}

export fn gsp_kv_u32(inp: const u8[:], key: const u8[:]) -> i64 {
    return g_peek(inp, key, 4 as i64);
}

export fn gsp_kv_i32(inp: const u8[:], key: const u8[:]) -> i64 {
    return g_peek(inp, key, GSP_KV_INT32);
}

// ---- GGUF reader used by split and merge -----------------------------------

// Parse the header and every KV into the S1 flat arrays. Strings and array
// payloads go to `pool`; the record layout is the S1 one, so the caller can
// replay or modify the list with the setters (merge does exactly that).
// info[0] version, info[1] alignment, info[2] n_tensors, info[3] n_kv,
// info[4] offset of the tensor section.
fn g_read_kv(inp: const u8[:],
                      st: i64[:], pool: u8[:], ko: i64[:], kl: i64[:],
                      ty: i64[:], ar: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                      info: i64[:]) -> i64 {
    let len: i64 = inp.len;
    let p0: i64 = g_head(inp);
    if p0 < 0 as i64 {
        return -(1 as i64);
    }
    let version: i64 = g_u32(inp, 4 as i64);
    let n_tensors: i64 = g_u64(inp, 8 as i64);
    let n_kv: i64 = g_u64(inp, 16 as i64);
    if n_tensors < 0 as i64 || n_kv < 0 as i64 {
        return -(1 as i64);
    }
    let init: i64 = s1_init(st, ko, kl, ty, ar, v0, v1, v2);
    if init != 0 as i64 {
        return -(1 as i64);
    }
    var p: i64 = p0;
    var k: i64 = 0;
    while k < n_kv {
        if st[S_N] >= ko.len || st[S_N] >= kl.len || st[S_N] >= ty.len ||
           st[S_N] >= ar.len || st[S_N] >= v0.len || st[S_N] >= v1.len || st[S_N] >= v2.len {
            return -(1 as i64);
        }
        if !g_fits(len, p, 8 as i64) {
            return -(1 as i64);
        }
        let klen: i64 = g_u64(inp, p);
        if klen < 1 as i64 || !g_fits(len, p + 8 as i64, klen) {
            return -(1 as i64);
        }
        let koff: i64 = st[S_POOL];
        if koff + klen > pool.len {
            return -(1 as i64);
        }
        var z: i64 = 0;
        while z < klen {
            pool[koff + z] = inp[p + 8 as i64 + z];
            z += 1 as i64;
        }
        st[S_POOL] = koff + klen;
        let idx: i64 = st[S_N];
        ko[idx] = koff;
        kl[idx] = klen;
        let key: const u8[:] = pool[koff..koff + klen];
        if find(st, pool, ko, kl, key) >= 0 as i64 {
            return -(1 as i64);
        }
        p = p + 8 as i64 + klen;
        if !g_fits(len, p, 4 as i64) {
            return -(1 as i64);
        }
        let t: i64 = g_u32(inp, p);
        p = p + 4 as i64;
        if t == 9 as i64 {
            if !g_fits(len, p, 12 as i64) {
                return -(1 as i64);
            }
            let at: i64 = g_u32(inp, p);
            let cnt: i64 = g_u64(inp, p + 4 as i64);
            p = p + 12 as i64;
            if cnt < 0 as i64 {
                return -(1 as i64);
            }
            let poff: i64 = st[S_POOL];
            if at == 8 as i64 {
                var w: i64 = poff;
                var e: i64 = 0;
                while e < cnt {
                    if !g_fits(len, p, 8 as i64) {
                        return -(1 as i64);
                    }
                    let sl: i64 = g_u64(inp, p);
                    if !g_fits(len, p + 8 as i64, sl) {
                        return -(1 as i64);
                    }
                    if w + sl > pool.len {
                        return -(1 as i64);
                    }
                    var y: i64 = 0;
                    while y < sl {
                        pool[w + y] = inp[p + 8 as i64 + y];
                        y += 1 as i64;
                    }
                    w = w + sl;
                    if e + 1 as i64 < cnt {
                        if w >= pool.len {
                            return -(1 as i64);
                        }
                        pool[w] = 0 as u8;
                        w = w + 1 as i64;
                    }
                    p = p + 8 as i64 + sl;
                    e += 1 as i64;
                }
                ty[idx] = at;
                ar[idx] = 2 as i64;
                v0[idx] = poff;
                v1[idx] = cnt;
                v2[idx] = w - poff;
                st[S_POOL] = w;
            } else {
                if at < 0 as i64 || at > 12 as i64 || at == 9 as i64 {
                    return -(1 as i64);
                }
                let esz: i64 = w_type_size(at);
                if esz <= 0 as i64 {
                    return -(1 as i64);
                }
                let bytes: i64 = cnt * esz;
                if !g_fits(len, p, bytes) {
                    return -(1 as i64);
                }
                if poff + bytes > pool.len {
                    return -(1 as i64);
                }
                var y2: i64 = 0;
                while y2 < bytes {
                    pool[poff + y2] = inp[p + y2];
                    y2 += 1 as i64;
                }
                ty[idx] = at;
                ar[idx] = 1 as i64;
                v0[idx] = poff;
                v1[idx] = cnt;
                v2[idx] = bytes;
                st[S_POOL] = poff + bytes;
                p = p + bytes;
            }
        } else if t == 8 as i64 {
            if !g_fits(len, p, 8 as i64) {
                return -(1 as i64);
            }
            let sl: i64 = g_u64(inp, p);
            if !g_fits(len, p + 8 as i64, sl) {
                return -(1 as i64);
            }
            let poff: i64 = st[S_POOL];
            if poff + sl > pool.len {
                return -(1 as i64);
            }
            var y3: i64 = 0;
            while y3 < sl {
                pool[poff + y3] = inp[p + 8 as i64 + y3];
                y3 += 1 as i64;
            }
            ty[idx] = 8 as i64;
            ar[idx] = 0 as i64;
            v0[idx] = poff;
            v1[idx] = sl;
            v2[idx] = sl;
            st[S_POOL] = poff + sl;
            p = p + 8 as i64 + sl;
        } else {
            if t < 0 as i64 || t > 12 as i64 {
                return -(1 as i64);
            }
            let sz: i64 = w_type_size(t);
            if sz <= 0 as i64 || !g_fits(len, p, sz) {
                return -(1 as i64);
            }
            var v: i64 = 0;
            var i: i64 = 0;
            while i < sz {
                v = v | ((inp[p + i] as i64) << (8 as i64 * i));
                i += 1 as i64;
            }
            ty[idx] = t;
            ar[idx] = 0 as i64;
            v0[idx] = v;
            v1[idx] = 0 as i64;
            v2[idx] = 0 as i64;
            p = p + sz;
        }
        st[S_N] = idx + 1 as i64;
        k += 1 as i64;
    }

    var alignment: i64 = GSP_ALIGN;
    let aidx: i64 = find(st, pool, ko, kl, "general.alignment");
    if aidx >= 0 as i64 {
        if ty[aidx] != 4 as i64 || ar[aidx] != 0 as i64 || v0[aidx] <= 0 as i64 {
            return -(1 as i64);
        }
        if (v0[aidx] & (v0[aidx] - 1 as i64)) != 0 as i64 {
            return -(1 as i64);
        }
        alignment = v0[aidx];
    }
    info[0] = version;
    info[1] = alignment;
    info[2] = n_tensors;
    info[3] = n_kv;
    info[4] = p;
    return 0 as i64;
}

// Parse the tensor descriptors that follow the KV section. `append` == 0
// resets the tensor state; 1 continues a list (merge). TR_DATA_OFF becomes the
// absolute source offset in `inp`, TR_OFFSET the cumulative padded offset.
// `alignment` is the source file's alignment; offsets are recomputed with the
// tensor state's alignment.
// info[0] data section start, info[1] first index written.
fn g_read_tensors(inp: const u8[:], meta_pos: i64, n_tensors: i64, alignment: i64,
                           append: i64, tst: i64[:], tpool: u8[:], tne: i64[:], trec: i64[:],
                           info: i64[:]) -> i64 {
    if n_tensors < 0 as i64 || meta_pos < 0 as i64 || alignment <= 0 as i64 {
        return -(1 as i64);
    }
    if append == 0 as i64 {
        tst[TS_N] = 0 as i64;
        tst[TS_POOL] = 0 as i64;
        tst[TS_ALIGN] = alignment;
    } else if tst[TS_ALIGN] != alignment {
        return -(1 as i64);
    }
    let base: i64 = tst[TS_N];
    var p: i64 = meta_pos;
    var k: i64 = 0;
    while k < n_tensors {
        let idx: i64 = tst[TS_N];
        if (idx + 1 as i64) * TR_STRIDE > trec.len || (idx + 1 as i64) * 4 as i64 > tne.len {
            return -(1 as i64);
        }
        if !g_fits(inp.len, p, 8 as i64) {
            return -(1 as i64);
        }
        let nl: i64 = g_u64(inp, p);
        p = p + 8 as i64;
        if nl < 1 as i64 || nl >= GGML_MAX_NAME || !g_fits(inp.len, p, nl) {
            return -(1 as i64);
        }
        let noff: i64 = tst[TS_POOL];
        if noff + nl > tpool.len {
            return -(1 as i64);
        }
        var z: i64 = 0;
        while z < nl {
            tpool[noff + z] = inp[p + z];
            z += 1 as i64;
        }
        let name: const u8[:] = tpool[noff..noff + nl];
        if t_find(tst, tpool, trec, name) >= 0 as i64 {
            return -(1 as i64);
        }
        tst[TS_POOL] = noff + nl;
        trec[idx * TR_STRIDE + TR_NAME_OFF] = noff;
        trec[idx * TR_STRIDE + TR_NAME_LEN] = nl;
        p = p + nl;

        if !g_fits(inp.len, p, 4 as i64) {
            return -(1 as i64);
        }
        let nd: i64 = g_u32(inp, p);
        p = p + 4 as i64;
        if nd < 1 as i64 || nd > 4 as i64 {
            return -(1 as i64);
        }
        var d: i64 = 0;
        while d < 4 as i64 {
            tne[idx * 4 as i64 + d] = 1 as i64;
            d += 1 as i64;
        }
        d = 0;
        while d < nd {
            if !g_fits(inp.len, p, 8 as i64) {
                return -(1 as i64);
            }
            let ne: i64 = g_u64(inp, p);
            if ne <= 0 as i64 {
                return -(1 as i64);
            }
            tne[idx * 4 as i64 + d] = ne;
            p = p + 8 as i64;
            d += 1 as i64;
        }
        if !g_fits(inp.len, p, 12 as i64) {
            return -(1 as i64);
        }
        let t: i64 = g_u32(inp, p);
        p = p + 4 as i64;
        if t_blck_size(t) <= 0 as i64 || t_type_size(t) <= 0 as i64 {
            return -(1 as i64);
        }
        let off: i64 = g_u64(inp, p);
        p = p + 8 as i64;
        if off < 0 as i64 {
            return -(1 as i64);
        }
        trec[idx * TR_STRIDE + TR_TYPE] = t;
        trec[idx * TR_STRIDE + TR_DIMS] = t_dims(tne, idx);
        trec[idx * TR_STRIDE + TR_OFFSET] = off;
        trec[idx * TR_STRIDE + TR_DATA_OFF] = off;
        let nb: i64 = t_nbytes(tne, idx, t);
        if nb < 0 as i64 {
            return -(1 as i64);
        }
        trec[idx * TR_STRIDE + TR_DATA_LEN] = nb;
        tst[TS_N] = idx + 1 as i64;
        k += 1 as i64;
    }

    var data_start: i64 = p;
    if n_tensors > 0 as i64 {
        data_start = ((p + alignment - 1 as i64) / alignment) * alignment;
    }
    var i: i64 = base;
    while i < tst[TS_N] {
        let o: i64 = trec[i * TR_STRIDE + TR_OFFSET];
        let dl: i64 = trec[i * TR_STRIDE + TR_DATA_LEN];
        if !g_fits(inp.len, data_start + o, dl) {
            return -(1 as i64);
        }
        trec[i * TR_STRIDE + TR_DATA_OFF] = data_start + o;
        i += 1 as i64;
    }
    if t_recompute(tst, tne, trec, 0 as i64) != 0 as i64 {
        return -(1 as i64);
    }
    info[0] = data_start;
    info[1] = base;
    return 0 as i64;
}

export fn gsp_read_kv(inp: const u8[:],
                      st: i64[:], pool: u8[:], ko: i64[:], kl: i64[:],
                      ty: i64[:], ar: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                      info: i64[:]) -> i64 {
    return g_read_kv(inp, st, pool, ko, kl, ty, ar, v0, v1, v2, info);
}

export fn gsp_read_tensors(inp: const u8[:], meta_pos: i64, n_tensors: i64, alignment: i64,
                           append: i64, tst: i64[:], tpool: u8[:], tne: i64[:], trec: i64[:],
                           info: i64[:]) -> i64 {
    return g_read_tensors(inp, meta_pos, n_tensors, alignment, append, tst, tpool, tne, trec, info);
}

// ---- split parameters ------------------------------------------------------

// sscanf("%d") behaviour: optional space, optional sign, digit prefix; junk
// after the digits is ignored.
fn g_atoi(b: const u8[:], o: i64, l: i64) -> i64 {
    var i: i64 = 0;
    while i < l {
        let c: i64 = b[o + i] as i64;
        if c != 32 as i64 && c != 9 as i64 {
            break;
        }
        i += 1 as i64;
    }
    var neg: i64 = 0;
    if i < l {
        if b[o + i] == 45 as u8 {
            neg = 1;
            i += 1 as i64;
        } else if b[o + i] == 43 as u8 {
            i += 1 as i64;
        }
    }
    var v: i64 = 0;
    while i < l {
        let c: i64 = b[o + i] as i64;
        if c < 48 as i64 || c > 57 as i64 {
            break;
        }
        v = v * 10 as i64 + (c - 48 as i64);
        i += 1 as i64;
    }
    if neg != 0 {
        return -v;
    }
    return v;
}

fn g_str_to_n_bytes(s: const u8[:]) -> i64 {
    if s.len < 1 as i64 {
        return -(1 as i64);
    }
    let last: i64 = s[s.len - 1 as i64] as i64;
    var mult: i64 = 0;
    if last == 77 as i64 {
        mult = 1000000 as i64;
    } else if last == 71 as i64 {
        mult = 1000000000 as i64;
    } else {
        return -(1 as i64);
    }
    let n: i64 = g_atoi(s, 0 as i64, s.len);
    if n <= 0 as i64 {
        return -(1 as i64);
    }
    return n * mult;
}

// split_str_to_n_bytes: "128M" and "4G"; the port returns -1 instead of
// throwing on a bad unit or a non-positive size.
export fn gsp_str_to_n_bytes(s: const u8[:], out: i64[:]) -> i64 {
    let v: i64 = g_str_to_n_bytes(s);
    if v < 0 as i64 {
        out[0] = v;
        return -(1 as i64);
    }
    out[0] = v;
    return 0 as i64;
}

// Compare argv[i] with `lit`, mapping '_' to '-' like the native prefix
// normalization.
fn g_arg_is(blob: const u8[:], aoff: const i64[:], alen: const i64[:], i: i64,
            lit: const u8[:]) -> bool {
    if i < 0 as i64 || i >= alen.len || i >= aoff.len {
        return false;
    }
    let l: i64 = alen[i];
    if l != lit.len {
        return false;
    }
    let o: i64 = aoff[i];
    if !g_fits(blob.len, o, l) {
        return false;
    }
    var k: i64 = 0;
    while k < l {
        var c: u8 = blob[o + k];
        if c == 95 as u8 {
            c = 45 as u8;
        }
        if c != lit[k] {
            return false;
        }
        k += 1 as i64;
    }
    return true;
}

// split_params_parse_ex / split_params_parse. Params:
//   0 operation (0 none, 1 split, 2 merge)
//   1 mode (0 none, 1 tensor, 2 size)
//   2 n_split_tensors (default 128)
//   3 n_bytes_split
//   4 no_tensor_first_split, 5 dry_run, 6 delete_splits
//   7 input argument index, 8 output argument index
//   9 1 for --help, 2 for --version (native exits 0 there)
// Returns 0 on success, -1 on every refusal (native prints usage and exits 1).
export fn gsp_parse(n_args: i64, blob: const u8[:], aoff: const i64[:], alen: const i64[:],
                    params: i64[:]) -> i64 {
    if n_args < 1 as i64 || n_args > aoff.len || n_args > alen.len {
        return -(1 as i64);
    }
    if params.len < 10 as i64 {
        return -(1 as i64);
    }
    var z: i64 = 0;
    while z < 10 as i64 {
        params[z] = 0 as i64;
        z += 1 as i64;
    }
    params[2] = 128 as i64;
    params[7] = -(1 as i64);
    params[8] = -(1 as i64);

    var i: i64 = 1;
    var bad: i64 = 0;
    while i < n_args {
        if alen[i] < 2 as i64 {
            break;
        }
        let ao: i64 = aoff[i];
        if !g_fits(blob.len, ao, 2 as i64) {
            return -(1 as i64);
        }
        if blob[ao] != 45 as u8 || blob[ao + 1 as i64] != 45 as u8 {
            break;
        }
        if g_arg_is(blob, aoff, alen, i, "--help") {
            params[9] = 1 as i64;
            return 0 as i64;
        } else if g_arg_is(blob, aoff, alen, i, "--version") {
            params[9] = 2 as i64;
            return 0 as i64;
        } else if g_arg_is(blob, aoff, alen, i, "--dry-run") {
            params[5] = 1 as i64;
        } else if g_arg_is(blob, aoff, alen, i, "--no-tensor-first-split") {
            params[4] = 1 as i64;
        } else if g_arg_is(blob, aoff, alen, i, "--merge") {
            if params[0] != 0 as i64 && params[0] != 2 as i64 {
                return -(1 as i64);
            }
            params[0] = 2 as i64;
        } else if g_arg_is(blob, aoff, alen, i, "--split") {
            if params[0] != 0 as i64 && params[0] != 1 as i64 {
                return -(1 as i64);
            }
            params[0] = 1 as i64;
        } else if g_arg_is(blob, aoff, alen, i, "--split-max-tensors") {
            i += 1 as i64;
            if i >= n_args {
                bad = 1 as i64;
                break;
            }
            if params[1] != 0 as i64 && params[1] != 1 as i64 {
                return -(1 as i64);
            }
            params[1] = 1 as i64;
            params[2] = g_atoi(blob, aoff[i], alen[i]);
        } else if g_arg_is(blob, aoff, alen, i, "--split-max-size") {
            i += 1 as i64;
            if i >= n_args {
                bad = 1 as i64;
                break;
            }
            if params[1] != 0 as i64 && params[1] != 2 as i64 {
                return -(1 as i64);
            }
            params[1] = 2 as i64;
            if !g_fits(blob.len, aoff[i], alen[i]) {
                bad = 1 as i64;
                break;
            }
            let nb: i64 = g_str_to_n_bytes(blob[aoff[i]..aoff[i] + alen[i]]);
            if nb < 0 as i64 {
                return -(1 as i64);
            }
            params[3] = nb;
        } else if g_arg_is(blob, aoff, alen, i, "--delete-splits") {
            params[6] = 1 as i64;
        } else {
            return -(1 as i64);
        }
        i += 1 as i64;
    }

    if params[0] == 0 as i64 {
        params[0] = 1 as i64;
    }
    if params[1] == 0 as i64 {
        params[1] = 1 as i64;
    }
    if bad != 0 as i64 {
        return -(1 as i64);
    }
    if n_args - i != 2 as i64 {
        return -(1 as i64);
    }
    params[7] = i;
    params[8] = i + 1 as i64;
    return 0 as i64;
}

// ---- split strategy --------------------------------------------------------

// should_split: 1 split, 0 keep, -1 invalid mode or an unusable tensor limit.
export fn gsp_should_split(mode: i64, i_tensor: i64, n_tensors: i64, next_size: i64,
                           n_split_tensors: i64, n_bytes_split: i64) -> i64 {
    if mode == 2 as i64 {
        if next_size > n_bytes_split {
            return 1 as i64;
        }
        return 0 as i64;
    }
    if mode == 1 as i64 {
        if n_split_tensors < 1 as i64 {
            return -(1 as i64);
        }
        if i_tensor > 0 as i64 && i_tensor < n_tensors &&
           i_tensor % n_split_tensors == 0 as i64 {
            return 1 as i64;
        }
        return 0 as i64;
    }
    return -(1 as i64);
}

fn g_pad(v: i64) -> i64 {
    return ((v + GSP_ALIGN - 1 as i64) / GSP_ALIGN) * GSP_ALIGN;
}

// The split_strategy constructor's decisions without the contexts. `nb[i]` is
// the tensor's nbytes. t0[s]/t1[s] receive the tensor range of shard s.
// Returns the shard count or -1 when native would exit (a shard with no
// tensors, a bad limit).
export fn gsp_split_layout(n_tensors: i64, mode: i64, n_split_tensors: i64,
                           n_bytes_split: i64, no_tensor_first: i64,
                           nb: const i64[:], t0: i64[:], t1: i64[:]) -> i64 {
    if n_tensors < 0 as i64 || n_tensors > nb.len {
        return -(1 as i64);
    }
    if mode == 2 as i64 {
        if n_bytes_split <= 0 as i64 {
            return -(1 as i64);
        }
    } else if mode == 1 as i64 {
        if n_split_tensors < 1 as i64 {
            return -(1 as i64);
        }
    } else {
        return -(1 as i64);
    }
    if t0.len < 1 as i64 || t1.len < 1 as i64 {
        return -(1 as i64);
    }
    var n_split: i64 = 1;
    t0[0] = 0 as i64;
    var cur: i64 = 0;
    if no_tensor_first != 0 as i64 {
        n_split = 2;
        t0[1] = 0 as i64;
    }
    var last: i64 = n_split - 1 as i64;
    var i: i64 = 0;
    while i < n_tensors {
        let nbi: i64 = nb[i];
        if nbi < 0 as i64 {
            return -(1 as i64);
        }
        let padded: i64 = g_pad(nbi);
        let next: i64 = cur + padded;
        let s: i64 = gsp_should_split(mode, i, n_tensors, next, n_split_tensors, n_bytes_split);
        if s < 0 as i64 {
            return -(1 as i64);
        }
        if s > 0 as i64 {
            let cnt: i64 = i - t0[last];
            if cnt == 0 as i64 {
                if !(last == 0 as i64 && no_tensor_first != 0 as i64) {
                    return -(1 as i64);
                }
            }
            if last + 1 as i64 >= t0.len || last + 1 as i64 >= t1.len {
                return -(1 as i64);
            }
            n_split += 1 as i64;
            last += 1 as i64;
            t0[last] = i;
            cur = padded;
        } else {
            cur = next;
        }
        i += 1 as i64;
    }
    if n_split > t0.len || n_split > t1.len {
        return -(1 as i64);
    }
    var s2: i64 = 0;
    while s2 < n_split {
        if s2 + 1 as i64 < n_split {
            t1[s2] = t0[s2 + 1 as i64];
        } else {
            t1[s2] = n_tensors;
        }
        if t1[s2] < t0[s2] {
            return -(1 as i64);
        }
        s2 += 1 as i64;
    }
    return n_split;
}

// ---- shard naming ----------------------------------------------------------

fn g_digits(v: i64) -> i64 {
    if v < 0 as i64 {
        return 1 as i64;
    }
    var d: i64 = 1;
    var t: i64 = v;
    while t >= 10 as i64 {
        t = t / 10 as i64;
        d += 1 as i64;
    }
    return d;
}

// Zero-padded decimal with a minimum width; -1 when it does not fit.
fn g_put_dec(out: u8[:], p: i64, v: i64, width: i64) -> i64 {
    if v < 0 as i64 {
        return -(1 as i64);
    }
    var w: i64 = g_digits(v);
    if width > w {
        w = width;
    }
    if !g_fits(out.len, p, w) {
        return -(1 as i64);
    }
    var i: i64 = w - 1 as i64;
    var t: i64 = v;
    while i >= 0 as i64 {
        out[p + i] = (48 as i64 + (t % 10 as i64)) as u8;
        t = t / 10 as i64;
        i -= 1 as i64;
    }
    return p + w;
}

// llama_split_path: "<prefix>-<split_no+1 %05d>-of-<count %05d>.gguf". Native
// silently truncates when the buffer is too small; the port refuses (-1).
export fn gsp_split_name(prefix: const u8[:], i_split: i64, n_split: i64, out: u8[:]) -> i64 {
    if i_split < 0 as i64 || n_split < 1 as i64 || i_split >= n_split {
        return -(1 as i64);
    }
    var q: i64 = 0;
    var i: i64 = 0;
    while i < prefix.len {
        if q >= out.len {
            return -(1 as i64);
        }
        out[q] = prefix[i];
        q += 1 as i64;
        i += 1 as i64;
    }
    if q >= out.len {
        return -(1 as i64);
    }
    out[q] = 45 as u8;
    q += 1 as i64;
    q = g_put_dec(out, q, i_split + 1 as i64, 5 as i64);
    if q < 0 as i64 {
        return -(1 as i64);
    }
    if !g_fits(out.len, q, 4 as i64) {
        return -(1 as i64);
    }
    out[q] = 45 as u8;
    out[q + 1 as i64] = 111 as u8;
    out[q + 2 as i64] = 102 as u8;
    out[q + 3 as i64] = 45 as u8;
    q = q + 4 as i64;
    q = g_put_dec(out, q, n_split, 5 as i64);
    if q < 0 as i64 {
        return -(1 as i64);
    }
    if !g_fits(out.len, q, 5 as i64) {
        return -(1 as i64);
    }
    out[q] = 46 as u8;
    out[q + 1 as i64] = 103 as u8;
    out[q + 2 as i64] = 103 as u8;
    out[q + 3 as i64] = 117 as u8;
    out[q + 4 as i64] = 102 as u8;
    return q + 5 as i64;
}

// llama_split_prefix: accept "<prefix>-<no %05d>-of-<count %05d>.gguf" and
// return the prefix length, writing the prefix into `out`. -1 otherwise.
export fn gsp_merge_prefix(path: const u8[:], i_split: i64, n_split: i64, out: u8[:]) -> i64 {
    if i_split < 0 as i64 || n_split < 1 as i64 || i_split >= n_split {
        return -(1 as i64);
    }
    if out.len < 64 as i64 {
        return -(1 as i64);
    }
    var q: i64 = 0;
    out[q] = 45 as u8;
    q += 1 as i64;
    q = g_put_dec(out, q, i_split + 1 as i64, 5 as i64);
    if q < 0 as i64 {
        return -(1 as i64);
    }
    let word: const u8[:] = "-of-";
    var i: i64 = 0;
    while i < 4 as i64 {
        out[q + i] = word[i];
        i += 1 as i64;
    }
    q = q + 4 as i64;
    q = g_put_dec(out, q, n_split, 5 as i64);
    if q < 0 as i64 {
        return -(1 as i64);
    }
    let tail: const u8[:] = ".gguf";
    i = 0;
    while i < 5 as i64 {
        out[q + i] = tail[i];
        i += 1 as i64;
    }
    let slen: i64 = q + 5 as i64;
    if path.len <= slen {
        return -(1 as i64);
    }
    let poff: i64 = path.len - slen;
    var k: i64 = 0;
    while k < slen {
        if path[poff + k] != out[k] {
            return -(1 as i64);
        }
        k += 1 as i64;
    }
    var m: i64 = 0;
    while m < poff {
        out[m] = path[m];
        m += 1 as i64;
    }
    return poff;
}

// ---- metadata emission -----------------------------------------------------

fn g_is_split_key(k: const u8[:]) -> bool {
    if k == "split.no" {
        return true;
    }
    if k == "split.count" {
        return true;
    }
    if k == "split.tensors.count" {
        return true;
    }
    return false;
}

// Upper bound for one KV record, used as the buffer guard.
fn g_kv_need(klen: i64, a: i64, t: i64, cnt: i64, bytes: i64) -> i64 {
    if a == 0 as i64 {
        if t == 8 as i64 {
            return 8 as i64 + klen + 4 as i64 + 8 as i64 + bytes;
        }
        return 8 as i64 + klen + 4 as i64 + w_type_size(t);
    }
    if a == 1 as i64 {
        return 8 as i64 + klen + 4 as i64 + 4 as i64 + 8 as i64 + bytes;
    }
    return 8 as i64 + klen + 4 as i64 + 4 as i64 + 8 as i64 + bytes + 8 as i64 * cnt;
}

fn g_w_kv(out: u8[:], p: i64, pool: const u8[:], ko: const i64[:], kl: const i64[:],
          types: const i64[:], arr: const i64[:], v0: const i64[:], v1: const i64[:],
          v2: const i64[:], i: i64) -> i64 {
    var q: i64 = w_str(out, p, pool, ko[i], kl[i]);
    let t: i64 = types[i];
    if arr[i] != 0 as i64 {
        q = w_u32(out, q, 9 as i64);
        q = w_u32(out, q, t);
        q = w_u64(out, q, v1[i]);
        if arr[i] == 1 as i64 {
            q = w_bytes(out, q, pool, v0[i], v2[i]);
        } else {
            let lim: i64 = v0[i] + v2[i];
            var a: i64 = v0[i];
            var e: i64 = 0;
            while e < v1[i] {
                var b: i64 = a;
                while b < lim && pool[b] != 0 as u8 {
                    b += 1 as i64;
                }
                q = w_str(out, q, pool, a, b - a);
                a = b + 1 as i64;
                e += 1 as i64;
            }
        }
    } else {
        q = w_u32(out, q, t);
        if t == 8 as i64 {
            q = w_str(out, q, pool, v0[i], v1[i]);
        } else {
            q = w_bits(out, q, v0[i], w_type_size(t));
        }
    }
    return q;
}

// Native appends split.count as a placeholder, then split.tensors.count, and
// only at the end replaces split.count with the real number. The replacement
// moves the key to the end, so the wire order is no, tensors.count, count.
fn g_w_split_keys(out: u8[:], p: i64, i_split: i64, n_split: i64, n_tensors: i64) -> i64 {
    var q: i64 = w_str(out, p, "split.no", 0 as i64, 8 as i64);
    q = w_u32(out, q, GSP_KV_UINT16);
    q = w_bits(out, q, i_split, 2 as i64);
    q = w_str(out, q, "split.tensors.count", 0 as i64, 19 as i64);
    q = w_u32(out, q, GSP_KV_INT32);
    q = w_bits(out, q, n_tensors, 4 as i64);
    q = w_str(out, q, "split.count", 0 as i64, 11 as i64);
    q = w_u32(out, q, GSP_KV_UINT16);
    q = w_bits(out, q, n_split, 2 as i64);
    return q;
}

// ---- split -----------------------------------------------------------------

// split_strategy::write into one buffer: every shard's metadata and data in
// order, offsets in sh_off/sh_len. Returns the total bytes written or -1.
export fn gsp_split_write(inp: const u8[:],
                          st: const i64[:], pool: const u8[:], ko: const i64[:],
                          kl: const i64[:], ty: const i64[:], ar: const i64[:],
                          v0: const i64[:], v1: const i64[:], v2: const i64[:],
                          tst: const i64[:], tpool: const u8[:], tne: const i64[:],
                          trec: const i64[:], t0: const i64[:], t1: const i64[:],
                          n_split: i64, out: u8[:], sh_off: i64[:], sh_len: i64[:]) -> i64 {
    if n_split < 1 as i64 || n_split > t0.len || n_split > t1.len ||
       n_split > sh_off.len || n_split > sh_len.len {
        return -(1 as i64);
    }
    var q: i64 = 0;
    var s: i64 = 0;
    while s < n_split {
        let start: i64 = q;
        let nt: i64 = t1[s] - t0[s];
        if nt < 0 as i64 {
            return -(1 as i64);
        }
        var nk: i64 = 3;
        if s == 0 as i64 {
            var k: i64 = 0;
            while k < st[S_N] {
                let key: const u8[:] = pool[ko[k]..ko[k] + kl[k]];
                if !g_is_split_key(key) {
                    nk += 1 as i64;
                }
                k += 1 as i64;
            }
        }
        if out.len - q < 32 as i64 {
            return -(1 as i64);
        }
        q = w_u8(out, q, W_MAGIC_0);
        q = w_u8(out, q, W_MAGIC_1);
        q = w_u8(out, q, W_MAGIC_2);
        q = w_u8(out, q, W_MAGIC_3);
        q = w_u32(out, q, 3 as i64);
        q = w_u64(out, q, nt);
        q = w_u64(out, q, nk);

        if s == 0 as i64 {
            var k: i64 = 0;
            while k < st[S_N] {
                let key: const u8[:] = pool[ko[k]..ko[k] + kl[k]];
                if !g_is_split_key(key) {
                    let need: i64 = g_kv_need(kl[k], ar[k], ty[k], v1[k], v2[k]);
                    if out.len - q < need {
                        return -(1 as i64);
                    }
                    q = g_w_kv(out, q, pool, ko, kl, ty, ar, v0, v1, v2, k);
                }
                k += 1 as i64;
            }
        }
        if out.len - q < 128 as i64 {
            return -(1 as i64);
        }
        q = g_w_split_keys(out, q, s, n_split, tst[TS_N]);

        var cur: i64 = 0;
        var i: i64 = t0[s];
        while i < t1[s] {
            let tyi: i64 = trec[i * TR_STRIDE + TR_TYPE];
            let nb: i64 = t_nbytes(tne, i, tyi);
            if nb < 0 as i64 {
                return -(1 as i64);
            }
            let nd: i64 = trec[i * TR_STRIDE + TR_DIMS];
            let need: i64 = 8 as i64 + trec[i * TR_STRIDE + TR_NAME_LEN] + 4 as i64 +
                            8 as i64 * nd + 12 as i64;
            if out.len - q < need {
                return -(1 as i64);
            }
            q = w_str(out, q, tpool, trec[i * TR_STRIDE + TR_NAME_OFF],
                      trec[i * TR_STRIDE + TR_NAME_LEN]);
            q = w_u32(out, q, nd);
            var d: i64 = 0;
            while d < nd {
                q = w_u64(out, q, tne[i * 4 as i64 + d]);
                d += 1 as i64;
            }
            q = w_u32(out, q, tyi);
            q = w_u64(out, q, cur);
            cur = cur + g_pad(nb);
            i += 1 as i64;
        }
        q = w_pad(out, q, GSP_ALIGN);

        i = t0[s];
        while i < t1[s] {
            let nb: i64 = t_nbytes(tne, i, trec[i * TR_STRIDE + TR_TYPE]);
            let padded: i64 = g_pad(nb);
            let src: i64 = trec[i * TR_STRIDE + TR_DATA_OFF];
            if nb < 0 as i64 || !g_fits(inp.len, src, nb) {
                return -(1 as i64);
            }
            if out.len - q < padded {
                return -(1 as i64);
            }
            q = w_bytes(out, q, inp, src, nb);
            q = w_pad(out, q, GSP_ALIGN);
            i += 1 as i64;
        }
        sh_off[s] = start;
        sh_len[s] = q - start;
        s += 1 as i64;
    }
    return q;
}

// gguf_set_val_u16(ctx, key, 0) with the setter's remove-then-append order.
// Kept private: an exported const-slice caller cannot be specialized from Goose
// code, and merge is its only user.
fn g_set_zero(st: i64[:], pool: u8[:], ko: i64[:], kl: i64[:], ty: i64[:], ar: i64[:],
              v0: i64[:], v1: i64[:], v2: i64[:], key: const u8[:]) -> i64 {
    if find(st, pool, ko, kl, key) < 0 as i64 {
        return -(1 as i64);
    }
    let _at: i64 = drop(st, pool, ko, kl, ty, ar, v0, v1, v2, key);
    let _off: i64 = append_key(st, pool, ko, kl, key);
    ty[st[S_N]] = GSP_KV_UINT16;
    ar[st[S_N]] = 0 as i64;
    v0[st[S_N]] = 0 as i64;
    v1[st[S_N]] = 0 as i64;
    v2[st[S_N]] = 0 as i64;
    st[S_N] = st[S_N] + 1 as i64;
    return 0 as i64;
}

// ---- merge -----------------------------------------------------------------

// gguf_merge over in-memory shards: buf holds the files end to end, off/len
// locate them. `have_output` mirrors the native refusal of an existing output
// file. The first shard's KV list is replayed with split.count set to 0, the
// tensors follow shard order with offsets recomputed at 32-byte alignment.
// Returns the merged size or -1.
export fn gsp_merge(buf: const u8[:], off: const i64[:], len: const i64[:], n_shards: i64,
                    have_output: i64, out: u8[:],
                    st: i64[:], pool: u8[:], ko: i64[:], kl: i64[:], ty: i64[:],
                    ar: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
                    tst: i64[:], tpool: u8[:], tne: i64[:], trec: i64[:],
                    info: i64[:]) -> i64 {
    if have_output != 0 as i64 || n_shards < 1 as i64 {
        return -(1 as i64);
    }
    if off.len < n_shards || len.len < n_shards {
        return -(1 as i64);
    }
    var s: i64 = 0;
    while s < n_shards {
        if off[s] < 0 as i64 || len[s] < 0 as i64 || off[s] + len[s] > buf.len {
            return -(1 as i64);
        }
        s += 1 as i64;
    }

    let b0: const u8[:] = buf[off[0]..off[0] + len[0]];
    if g_read_kv(b0, st, pool, ko, kl, ty, ar, v0, v1, v2, info) != 0 as i64 {
        return -(1 as i64);
    }
    let n_split: i64 = g_peek(b0, "split.count", GSP_KV_UINT16);
    if n_split < 1 as i64 || n_split != n_shards {
        return -(1 as i64);
    }
    if g_peek(b0, "split.no", GSP_KV_UINT16) != 0 as i64 {
        return -(1 as i64);
    }
    let tc: i64 = g_peek(b0, "split.tensors.count", GSP_KV_INT32);
    if tc < 0 as i64 {
        return -(1 as i64);
    }
    let nt0: i64 = info[2];
    let align0: i64 = info[1];
    let meta0: i64 = info[4];

    s = 1;
    while s < n_shards {
        let b: const u8[:] = buf[off[s]..off[s] + len[s]];
        if g_peek(b, "split.no", GSP_KV_UINT16) != s {
            return -(1 as i64);
        }
        if g_peek(b, "split.count", GSP_KV_UINT16) != n_split {
            return -(1 as i64);
        }
        if g_peek(b, "split.tensors.count", GSP_KV_INT32) != tc {
            return -(1 as i64);
        }
        s += 1 as i64;
    }

    // native: gguf_set_val_u16(ctx_gguf, split.count, 0) before gguf_set_kv,
    // which drops the key and appends it, so it ends up last.
    if g_set_zero(st, pool, ko, kl, ty, ar, v0, v1, v2, "split.count") != 0 as i64 {
        return -(1 as i64);
    }

    if g_read_tensors(b0, meta0, nt0, align0, 0 as i64, tst, tpool, tne, trec, info) != 0 as i64 {
        return -(1 as i64);
    }
    s = 1;
    while s < n_shards {
        let b: const u8[:] = buf[off[s]..off[s] + len[s]];
        let nt: i64 = g_u64(b, 8 as i64);
        let mp: i64 = g_kv_end(b);
        let at: i64 = g_peek(b, "general.alignment", 4 as i64);
        var al: i64 = GSP_ALIGN;
        if at > 0 as i64 {
            al = at;
        }
        if nt < 0 as i64 || mp < 0 as i64 {
            return -(1 as i64);
        }
        if g_read_tensors(b, mp, nt, al, 1 as i64, tst, tpool, tne, trec, info) != 0 as i64 {
            return -(1 as i64);
        }
        let base: i64 = info[1];
        var i: i64 = base;
        while i < tst[TS_N] {
            trec[i * TR_STRIDE + TR_DATA_OFF] =
                trec[i * TR_STRIDE + TR_DATA_OFF] + off[s];
            i += 1 as i64;
        }
        s += 1 as i64;
    }
    if tst[TS_N] != tc {
        return -(1 as i64);
    }
    tst[TS_ALIGN] = GSP_ALIGN;
    if t_recompute(tst, tne, trec, 0 as i64) != 0 as i64 {
        return -(1 as i64);
    }

    var q: i64 = 0;
    if out.len < 32 as i64 {
        return -(1 as i64);
    }
    q = w_u8(out, q, W_MAGIC_0);
    q = w_u8(out, q, W_MAGIC_1);
    q = w_u8(out, q, W_MAGIC_2);
    q = w_u8(out, q, W_MAGIC_3);
    q = w_u32(out, q, 3 as i64);
    q = w_u64(out, q, tst[TS_N]);
    q = w_u64(out, q, st[S_N]);
    var k: i64 = 0;
    while k < st[S_N] {
        let need: i64 = g_kv_need(kl[k], ar[k], ty[k], v1[k], v2[k]);
        if out.len - q < need {
            return -(1 as i64);
        }
        q = g_w_kv(out, q, pool, ko, kl, ty, ar, v0, v1, v2, k);
        k += 1 as i64;
    }
    var i2: i64 = 0;
    while i2 < tst[TS_N] {
        let nd: i64 = trec[i2 * TR_STRIDE + TR_DIMS];
        let need: i64 = 8 as i64 + trec[i2 * TR_STRIDE + TR_NAME_LEN] + 4 as i64 +
                        8 as i64 * nd + 12 as i64;
        if out.len - q < need {
            return -(1 as i64);
        }
        q = w_str(out, q, tpool, trec[i2 * TR_STRIDE + TR_NAME_OFF],
                  trec[i2 * TR_STRIDE + TR_NAME_LEN]);
        q = w_u32(out, q, nd);
        var d2: i64 = 0;
        while d2 < nd {
            q = w_u64(out, q, tne[i2 * 4 as i64 + d2]);
            d2 += 1 as i64;
        }
        q = w_u32(out, q, trec[i2 * TR_STRIDE + TR_TYPE]);
        q = w_u64(out, q, trec[i2 * TR_STRIDE + TR_OFFSET]);
        i2 += 1 as i64;
    }
    q = w_pad(out, q, GSP_ALIGN);
    i2 = 0;
    while i2 < tst[TS_N] {
        let nb: i64 = t_nbytes(tne, i2, trec[i2 * TR_STRIDE + TR_TYPE]);
        let padded: i64 = g_pad(nb);
        let src: i64 = trec[i2 * TR_STRIDE + TR_DATA_OFF];
        if nb < 0 as i64 || !g_fits(buf.len, src, nb) {
            return -(1 as i64);
        }
        if out.len - q < padded {
            return -(1 as i64);
        }
        q = w_bytes(out, q, buf, src, nb);
        q = w_pad(out, q, GSP_ALIGN);
        i2 += 1 as i64;
    }
    info[0] = n_shards;
    info[1] = tst[TS_N];
    return q;
}
