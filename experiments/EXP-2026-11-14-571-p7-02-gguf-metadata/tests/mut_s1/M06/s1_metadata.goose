// GGUF metadata context in Goose (P7-02 S1), from ggml/src/gguf.cpp:
// gguf_init_empty, gguf_find_key, gguf_remove_key, gguf_set_val_*,
// gguf_set_arr_data, gguf_set_arr_str, gguf_set_kv and the reserved-key rule.
//
// The KV list is flat: one record per entry, keys and string/array bytes live in
// a caller-owned byte pool. This slice owns the list semantics, not the writer:
//
//   - setting an existing key removes it first, so the new value moves to the
//     end of the list (native gguf_remove_key + emplace_back);
//   - remove returns the old index, or -1 when the key is absent;
//   - set_kv walks the source in order and calls the same setter semantics, so
//     every copied key moves to the end and destination duplicates are removed;
//   - "general.alignment" accepts only a u32 power of two. Native aborts on the
//     other cases; this port returns -1 so a fixture can assert the refusal
//     without killing the host.
//
// Record layout (parallel arrays, index 0..n-1):
//   key_off/key_len  key bytes in `pool`
//   type             gguf_type of a scalar, or element type of an array
//   arr              0 scalar, 1 array of values, 2 array of strings
//   v0               scalar: raw 64-bit value; otherwise pool offset
//   v1               array length (0 for scalars)
//   v2               byte length in pool (0 for numeric scalars)
//
// State: st[0] n, st[1] pool cursor.
//
// Formats and type ids follow the native enum reused by canon/gguf-parser.goose
// (UINT8=0, INT8=1, ..., STRING=8, ARRAY=9, ..., FLOAT64=12).

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
    types[st[S_N]] = 0 as i64;
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
