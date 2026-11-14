// LoRA adapter manifest and tensor pairing in Goose (P6-09 S1), from
// llama_adapter_lora_init_impl() in src/llama-adapter.cpp:151-427 and
// llama_adapter_lora_weight::get_scale in src/llama-adapter.h:48-61
// (flint tree, base commit a640bac9).
//
// Owned decisions, no file and no weights:
//   - metadata gate in native order: general.type == "adapter",
//     general.architecture matches the base model arch, adapter.type == "lora",
//     adapter.lora.alpha (0.0 when absent), adapter.alora.invocation_tokens
//     (array of u32, optional);
//   - tensor naming: ".lora_a" / ".lora_b" suffix pairing under a base name with
//     every occurrence of the suffix removed, "_norm.weight" skipped, any other
//     suffix is a hard error on the first tensor in GGUF order;
//   - the pair table is a sorted map: a second ".lora_a" for the same base name
//     replaces the first, exactly like ab_map[name].a = cur;
//   - per-pair validation in sorted name order: missing component, absent from
//     the base model, a Hadamard-folded target, shape. token_embd.weight uses
//     the flipped rule (model ne0 == b.ne1, model ne1 == a.ne1), every other
//     name the straight rule (model ne0 == a.ne0, model ne1 == b.ne1) plus
//     a.ne1 == b.ne0 for the transpose check;
//   - data bounds: offs = data section base + tensor offset, offs + nbytes must
//     stay inside the file and must not wrap;
//   - scale: alpha == 0 means the caller scale, else scale * alpha / rank with
//     rank = b.ne[0].
//
// Flat layouts (no structs):
//   metadata is a caller-decoded GGUF view, one record per key:
//     kv_off/kv_len   key bytes in `pool`
//     kv_type         gguf_type of the value: 4 u32, 6 f32, 8 string, 9 array
//     kv_v0           u32 scalar value, string offset in `pool`, or array start
//                     index in `arr`
//     kv_v1           string length, or array element count
//     kv_v2           array element type
//     kv_f32[i]       value of a f32 record, meaningful only when type == 6
//   tensors: t_off/t_len (name bytes in `tpool`), t_ne0, t_ne1, t_off_data,
//     t_nbytes. Base-model tensors: m_off/m_len (name bytes in `mpool`),
//     m_ne0, m_ne1, m_had (1 = prism.hadamard folded).
//   pairs: PAIR_SLOTS i64 per pair = name offset in the `names` arena, name
//     length, a tensor index (-1 absent), b tensor index (-1 absent).
//
// Return codes:
//    0 ok
//   -1 GGUF could not be opened/parsed (caller's file layer)
//   -2 general.type is not "adapter"
//   -3 model arch and LoRA arch mismatch
//   -4 adapter.type is not "lora"
//   -5 adapter.alora.invocation_tokens is not an array
//   -6 adapter.alora.invocation_tokens is not an array of u32
//   -7 tensor has an unexpected suffix
//   -8 pair is missing one component
//   -9 tensor does not exist in the base model
//  -10 tensor targets a Hadamard-folded weight
//  -11 tensor shape does not match the base model
//  -12 lora_a tensor is not transposed
//  -13 tensor data is outside the file
//  -14 bad argument or caller capacity (port-only guard)
//  -15 metadata value has the wrong type (native GGML_ASSERT, port-only guard)

const T_UINT32: i64 = 4;
const T_F32: i64 = 6;
const T_STRING: i64 = 8;
const T_ARRAY: i64 = 9;

const E_OK: i64 = 0;
const E_GENERAL_TYPE: i64 = -2;
const E_ARCH: i64 = -3;
const E_ADAPTER_TYPE: i64 = -4;
const E_ALORA_TYPE: i64 = -5;
const E_ALORA_ELEM: i64 = -6;
const E_SUFFIX: i64 = -7;
const E_PAIR: i64 = -8;
const E_MISSING: i64 = -9;
const E_HADAMARD: i64 = -10;
const E_SHAPE: i64 = -11;
const E_TRANSPOSED: i64 = -12;
const E_BOUNDS: i64 = -13;
const E_ARG: i64 = -14;
const E_KV_TYPE: i64 = -15;

const KEY_GENERAL_TYPE: u8[:] = "general.type";
const KEY_GENERAL_ARCH: u8[:] = "general.architecture";
const KEY_ADAPTER_TYPE: u8[:] = "adapter.type";
const KEY_ALPHA: u8[:] = "adapter.lora.alpha";
const KEY_ALORA: u8[:] = "adapter.alora.invocation_tokens";

const VAL_ADAPTER: u8[:] = "adapter";
const VAL_LORA: u8[:] = "lora";

const SUF_A: u8[:] = ".lora_a";
const SUF_B: u8[:] = ".lora_b";
const SUF_NORM: u8[:] = "_norm.weight";
const NAME_TOKEN_EMBD: u8[:] = "token_embd.weight";

const PAIR_SLOTS: i64 = 4;

// lora_pair diagnostics: n_pairs, skipped norm tensors, code, error name offset,
// error name length, arena cursor.
const PST_N: i64 = 0;
const PST_NORM: i64 = 1;
const PST_CODE: i64 = 2;
const PST_EOFF: i64 = 3;
const PST_ELEN: i64 = 4;
const PST_CUR: i64 = 5;
const PST_WORDS: i64 = 6;

fn bytes_eq(a: const u8[:], ao: i64, al: i64, key: const u8[:]) -> i64 {
    if al != key.len {
        return 0;
    }
    if ao < 0 || ao + al > a.len {
        return 0;
    }
    if a[ao..ao + al] == key {
        return 1;
    }
    return 0;
}

// memcmp over two regions of the same pool.
fn bytes_cmp(p: const u8[:], ao: i64, al: i64, bo: i64, bl: i64) -> i64 {
    var i: i64 = 0;
    while i < al && i < bl {
        let ca = p[ao + i];
        let cb = p[bo + i];
        if ca < cb {
            return -1;
        }
        if ca > cb {
            return 1;
        }
        i = i + 1;
    }
    if al < bl {
        return -1;
    }
    if al > bl {
        return 1;
    }
    return 0;
}

// Name equality across the pair arena and the base-model name pool.
fn name_eq(n: const u8[:], noff: i64, nlen: i64, m: const u8[:], moff: i64, mlen: i64) -> i64 {
    if nlen != mlen {
        return 0;
    }
    if noff < 0 || noff + nlen > n.len || moff < 0 || moff + mlen > m.len {
        return 0;
    }
    var i: i64 = 0;
    while i < nlen {
        if n[noff + i] != m[moff + i] {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

fn ends_with(p: const u8[:], off: i64, len: i64, suf: const u8[:]) -> i64 {
    if len < suf.len || off < 0 || off + len > p.len {
        return 0;
    }
    let o = off + len - suf.len;
    if p[o..o + suf.len] == suf {
        return 1;
    }
    return 0;
}

// 1 = .lora_a, 2 = .lora_b, 3 = _norm.weight, 0 = unexpected.
fn suffix_kind(p: const u8[:], off: i64, len: i64) -> i64 {
    if ends_with(p, off, len, SUF_A) != 0 {
        return 1;
    }
    if ends_with(p, off, len, SUF_B) != 0 {
        return 2;
    }
    if ends_with(p, off, len, SUF_NORM) != 0 {
        return 3;
    }
    return 0;
}

// The suffix bytes a kind-1 or kind-2 tensor is stripped of.
fn suffix_of(kind: i64) -> const u8[:] {
    if kind == 2 {
        return SUF_B;
    }
    return SUF_A;
}

fn kv_find(
    n_kv: i64,
    kv_off: const i64[:],
    kv_len: const i64[:],
    pool: const u8[:],
    key: const u8[:]) -> i64 {

    var i: i64 = 0;
    while i < n_kv {
        if bytes_eq(pool, kv_off[i], kv_len[i], key) != 0 {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

// String metadata lookup: the record index, -1 when absent, E_KV_TYPE when the
// value is not a string (native gguf_get_val_str aborts on that input).
fn kv_str_id(
    n_kv: i64,
    kv_off: const i64[:],
    kv_len: const i64[:],
    kv_type: const i64[:],
    pool: const u8[:],
    key: const u8[:]) -> i64 {

    let id = kv_find(n_kv, kv_off, kv_len, pool, key);
    if id < 0 {
        return -1;
    }
    if kv_type[id] != T_STRING {
        return E_KV_TYPE;
    }
    return id;
}

// Metadata gate in native order. Writes out_f[0] = alpha, st[0] = number of
// aLoRA tokens, st[1] = 1 when adapter.lora.alpha is present, st[2] = code.
export fn lora_manifest(
    n_kv: i64,
    kv_off: const i64[:],
    kv_len: const i64[:],
    kv_type: const i64[:],
    kv_v0: const i64[:],
    kv_v1: const i64[:],
    kv_v2: const i64[:],
    kv_f32: const f32[:],
    arr: const i64[:],
    pool: const u8[:],
    model_arch: const u8[:],
    st: i64[:],
    out_f: f32[:],
    toks: i64[:]) -> i64 {

    if st.len < 3 || out_f.len < 1 {
        return E_ARG;
    }
    // The caller-supplied view must be consistent before any index is taken.
    if n_kv < 0 || n_kv > kv_off.len || n_kv > kv_len.len || n_kv > kv_type.len ||
        n_kv > kv_v0.len || n_kv > kv_v1.len || n_kv > kv_v2.len || n_kv > kv_f32.len {
        return E_ARG;
    }
    st[0] = 0;
    st[1] = 0;
    st[2] = E_OK;
    out_f[0] = 0.0;

    // general.type must be the string "adapter".
    let gt = kv_str_id(n_kv, kv_off, kv_len, kv_type, pool, KEY_GENERAL_TYPE);
    if gt == E_KV_TYPE {
        st[2] = E_KV_TYPE;
        return E_KV_TYPE;
    }
    if gt < 0 {
        st[2] = E_GENERAL_TYPE;
        return E_GENERAL_TYPE;
    }
    if bytes_eq(pool, kv_v0[gt], kv_v1[gt], VAL_ADAPTER) == 0 {
        st[2] = E_GENERAL_TYPE;
        return E_GENERAL_TYPE;
    }

    // general.architecture must parse to the model arch. The port does not own
    // the arch table, so it compares the canonical arch name byte for byte.
    let ga = kv_str_id(n_kv, kv_off, kv_len, kv_type, pool, KEY_GENERAL_ARCH);
    if ga == E_KV_TYPE {
        st[2] = E_KV_TYPE;
        return E_KV_TYPE;
    }
    if ga < 0 {
        st[2] = E_ARCH;
        return E_ARCH;
    }
    if bytes_eq(pool, kv_v0[ga], kv_v1[ga], model_arch) == 0 {
        st[2] = E_ARCH;
        return E_ARCH;
    }

    // adapter.type must be "lora".
    let at = kv_str_id(n_kv, kv_off, kv_len, kv_type, pool, KEY_ADAPTER_TYPE);
    if at == E_KV_TYPE {
        st[2] = E_KV_TYPE;
        return E_KV_TYPE;
    }
    if at < 0 {
        st[2] = E_ADAPTER_TYPE;
        return E_ADAPTER_TYPE;
    }
    if bytes_eq(pool, kv_v0[at], kv_v1[at], VAL_LORA) == 0 {
        st[2] = E_ADAPTER_TYPE;
        return E_ADAPTER_TYPE;
    }

    // adapter.lora.alpha: absent means 0.0.
    let id = kv_find(n_kv, kv_off, kv_len, pool, KEY_ALPHA);
    if id >= 0 {
        if kv_type[id] != T_F32 {
            st[2] = E_KV_TYPE;
            return E_KV_TYPE;
        }
        out_f[0] = kv_f32[id];
        st[1] = 1;
    }

    // adapter.alora.invocation_tokens: optional array of u32.
    let ia = kv_find(n_kv, kv_off, kv_len, pool, KEY_ALORA);
    if ia >= 0 {
        if kv_type[ia] != T_ARRAY {
            st[2] = E_ALORA_TYPE;
            return E_ALORA_TYPE;
        }
        if kv_v2[ia] != T_UINT32 {
            st[2] = E_ALORA_ELEM;
            return E_ALORA_ELEM;
        }
        let n = kv_v1[ia];
        if n < 0 || n > toks.len {
            st[2] = E_ARG;
            return E_ARG;
        }
        let base = kv_v0[ia];
        if n > 0 && (base < 0 || base + n > arr.len) {
            st[2] = E_ARG;
            return E_ARG;
        }
        var k: i64 = 0;
        while k < n {
            // llama_token is int32, so a u32 id above 2^31-1 lands as a negative
            // token; the port reproduces the sign extension.
            var v = arr[base + k] % 4294967296;
            if v < 0 {
                v = v + 4294967296;
            }
            if v >= 2147483648 {
                v = v - 4294967296;
            }
            toks[k] = v;
            k = k + 1;
        }
        st[0] = n;
    }
    return E_OK;
}

// Copy the tensor name into the arena with every occurrence of `suf` removed,
// which is what replace_all(name, suffix, "") produces. Returns the copied
// length, or -1 when the arena is too small.
fn strip_copy(
    src: const u8[:],
    soff: i64,
    slen: i64,
    suf: const u8[:],
    dst: u8[:],
    doff: i64) -> i64 {

    var i: i64 = 0;
    var w: i64 = 0;
    while i < slen {
        var hit = 0;
        if i + suf.len <= slen {
            if src[soff + i..soff + i + suf.len] == suf {
                hit = 1;
            }
        }
        if hit != 0 {
            i = i + suf.len;
        } else {
            if doff + w >= dst.len {
                return -1;
            }
            dst[doff + w] = src[soff + i];
            w = w + 1;
            i = i + 1;
        }
    }
    return w;
}

// Index of the pair whose name equals [noff, noff+nlen) in the arena, or -1.
fn pair_find(names: const u8[:], pairs: const i64[:], n: i64, noff: i64, nlen: i64) -> i64 {
    var i: i64 = 0;
    while i < n {
        let off = pairs[PAIR_SLOTS * i];
        let len = pairs[PAIR_SLOTS * i + 1];
        if bytes_cmp(names, off, len, noff, nlen) == 0 {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

// Open a hole at `at` by shifting the tail right.
fn pair_open_slot(pairs: i64[:], n: i64, at: i64) -> i64 {
    var i: i64 = n;
    while i > at {
        pairs[PAIR_SLOTS * i + 0] = pairs[PAIR_SLOTS * (i - 1) + 0];
        pairs[PAIR_SLOTS * i + 1] = pairs[PAIR_SLOTS * (i - 1) + 1];
        pairs[PAIR_SLOTS * i + 2] = pairs[PAIR_SLOTS * (i - 1) + 2];
        pairs[PAIR_SLOTS * i + 3] = pairs[PAIR_SLOTS * (i - 1) + 3];
        i = i - 1;
    }
    return at;
}

// Bundle .lora_a and .lora_b into sorted pairs and skip every _norm.weight
// tensor. The first unexpected suffix in GGUF order is the error.
export fn lora_pair(
    n_tensors: i64,
    t_off: const i64[:],
    t_len: const i64[:],
    tpool: const u8[:],
    names: u8[:],
    pairs: i64[:],
    pst: i64[:]) -> i64 {

    if pst.len < PST_WORDS || pairs.len < PAIR_SLOTS {
        return E_ARG;
    }
    if n_tensors < 0 || n_tensors > t_off.len || n_tensors > t_len.len {
        return E_ARG;
    }
    pst[PST_N] = 0;
    pst[PST_NORM] = 0;
    pst[PST_CODE] = E_OK;
    pst[PST_EOFF] = 0;
    pst[PST_ELEN] = 0;
    pst[PST_CUR] = 0;

    var cur: i64 = 0;
    var n: i64 = 0;

    var i: i64 = 0;
    while i < n_tensors {
        let off = t_off[i];
        let len = t_len[i];
        let kind = suffix_kind(tpool, off, len);

        if kind == 3 {
            pst[PST_NORM] = pst[PST_NORM] + 1;
            i = i + 1;
            continue;
        }
        if kind == 0 {
            pst[PST_CODE] = E_SUFFIX;
            pst[PST_EOFF] = off;
            pst[PST_ELEN] = len;
            return E_SUFFIX;
        }

        let suf = suffix_of(kind);

        let save = cur;
        let w = strip_copy(tpool, off, len, suf, names, cur);
        if w < 0 {
            pst[PST_CODE] = E_ARG;
            return E_ARG;
        }

        let found = pair_find(names, pairs, n, save, w);
        if found >= 0 {
            // Duplicate base name: the later tensor wins, as in ab_map[name].a.
            cur = save;
            if kind == 1 {
                pairs[PAIR_SLOTS * found + 2] = i;
            } else {
                pairs[PAIR_SLOTS * found + 3] = i;
            }
            i = i + 1;
            continue;
        }
        if n >= pairs.len / PAIR_SLOTS {
            pst[PST_CODE] = E_ARG;
            return E_ARG;
        }

        // First existing name greater than the new one keeps the table sorted.
        var pos: i64 = n;
        var scan: i64 = 0;
        while scan < n {
            let eoff = pairs[PAIR_SLOTS * scan];
            let elen = pairs[PAIR_SLOTS * scan + 1];
            if bytes_cmp(names, eoff, elen, save, w) > 0 {
                pos = scan;
                scan = n;
            } else {
                scan = scan + 1;
            }
        }
        pair_open_slot(pairs, n, pos);
        pairs[PAIR_SLOTS * pos + 0] = save;
        pairs[PAIR_SLOTS * pos + 1] = w;
        pairs[PAIR_SLOTS * pos + 2] = -1;
        pairs[PAIR_SLOTS * pos + 3] = -1;
        if kind == 1 {
            pairs[PAIR_SLOTS * pos + 2] = i;
        } else {
            pairs[PAIR_SLOTS * pos + 3] = i;
        }
        n = n + 1;
        cur = save + w;
        i = i + 1;
    }
    pst[PST_N] = n;
    pst[PST_CUR] = cur;
    return E_OK;
}

// First base-model tensor named [noff, noff+nlen), or -1.
fn model_index(
    names: const u8[:],
    name_off: i64,
    name_len: i64,
    n_m: i64,
    m_off: const i64[:],
    m_len: const i64[:],
    mpool: const u8[:]) -> i64 {

    var i: i64 = 0;
    while i < n_m {
        if name_eq(names, name_off, name_len, mpool, m_off[i], m_len[i]) != 0 {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

// Shape rules for one pair.
fn lora_validate_pair(
    names: const u8[:],
    name_off: i64,
    name_len: i64,
    a_ne0: i64,
    a_ne1: i64,
    b_ne0: i64,
    b_ne1: i64,
    n_m: i64,
    m_off: const i64[:],
    m_len: const i64[:],
    m_ne0: const i64[:],
    m_ne1: const i64[:],
    m_had: const i64[:],
    mpool: const u8[:]) -> i64 {

    let hit = model_index(names, name_off, name_len, n_m, m_off, m_len, mpool);
    if hit < 0 {
        return E_MISSING;
    }
    if m_had[hit] != 0 {
        return E_HADAMARD;
    }

    if ends_with(names, name_off, name_len, NAME_TOKEN_EMBD) != 0 {
        // llm_build_inp_embd reads the transposed layout for the token embedding.
        if m_ne0[hit] != b_ne1 || m_ne1[hit] != a_ne1 {
            return E_SHAPE;
        }
        return E_OK;
    }
    if m_ne0[hit] != a_ne0 || m_ne1[hit] != b_ne1 {
        return E_SHAPE;
    }
    if a_ne1 != b_ne0 {
        return E_TRANSPOSED;
    }
    return E_OK;
}

// Pair validation in sorted name order: missing component, then the per-pair
// rule above. st[0] = error name offset, st[1] = error name length,
// st[2] = code, st[3] = offending tensor index.
export fn lora_validate_pairs(
    n_pairs: i64,
    pairs: const i64[:],
    names: const u8[:],
    t_ne0: const i64[:],
    t_ne1: const i64[:],
    n_m: i64,
    m_off: const i64[:],
    m_len: const i64[:],
    m_ne0: const i64[:],
    m_ne1: const i64[:],
    m_had: const i64[:],
    mpool: const u8[:],
    st: i64[:]) -> i64 {

    if st.len < 4 || n_pairs < 0 || PAIR_SLOTS * n_pairs > pairs.len {
        return E_ARG;
    }
    st[0] = 0;
    st[1] = 0;
    st[2] = E_OK;
    st[3] = -1;

    var p: i64 = 0;
    while p < n_pairs {
        let noff = pairs[PAIR_SLOTS * p + 0];
        let nlen = pairs[PAIR_SLOTS * p + 1];
        let a = pairs[PAIR_SLOTS * p + 2];
        let b = pairs[PAIR_SLOTS * p + 3];

        st[0] = noff;
        st[1] = nlen;
        if a < 0 || b < 0 {
            st[2] = E_PAIR;
            if a < 0 {
                st[3] = b;
            } else {
                st[3] = a;
            }
            return E_PAIR;
        }

        let rc = lora_validate_pair(
            names, noff, nlen,
            t_ne0[a], t_ne1[a], t_ne0[b], t_ne1[b],
            n_m, m_off, m_len, m_ne0, m_ne1, m_had, mpool);

        if rc != E_OK {
            st[2] = rc;
            st[3] = a;
            return rc;
        }
        p = p + 1;
    }
    return E_OK;
}

// Data bounds for every pair, a then b, after the geometry checks.
// Native: offs = data_offset + tensor_offset and offs + nbytes must stay inside
// the file and must not wrap.
export fn lora_bounds(
    n_pairs: i64,
    pairs: const i64[:],
    t_off_data: const i64[:],
    t_nbytes: const i64[:],
    data_base: i64,
    file_size: i64,
    st: i64[:]) -> i64 {

    if st.len < 2 || n_pairs < 0 || PAIR_SLOTS * n_pairs > pairs.len {
        return E_ARG;
    }
    st[0] = -1;
    st[1] = E_OK;

    var p: i64 = 0;
    while p < n_pairs {
        var side: i64 = 2;
        while side <= 3 {
            let t = pairs[PAIR_SLOTS * p + side];
            if t >= 0 {
                let offs = data_base + t_off_data[t];
                let size = t_nbytes[t];
                if offs + size < offs || offs + size > file_size {
                    st[0] = t;
                    st[1] = E_BOUNDS;
                    return E_BOUNDS;
                }
            }
            side = side + 1;
        }
        p = p + 1;
    }
    return E_OK;
}

// llama_adapter_lora_weight::get_scale: alpha == 0 keeps the adapter scale,
// otherwise the rank of b.ne[0] divides scale * alpha.
export fn lora_scale(alpha: f32, adapter_scale: f32, rank: i64) -> f32 {
    var scale = adapter_scale;
    if alpha != 0.0 {
        let r: f32 = rank;
        scale = adapter_scale * alpha / r;
    }
    return scale;
}
