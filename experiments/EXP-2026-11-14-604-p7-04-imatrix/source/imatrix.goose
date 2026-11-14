// Importance-matrix loading and layer remapping (P7-04 S2).
//
// Native counterparts:
//   common/imatrix-loader.cpp       common_imatrix_load_legacy, common_imatrix_load
//   src/llama-quant.cpp             remap_layer, remap_imatrix
//   tools/quantize/quantize.cpp     load_imatrix, prepare_imatrix
//
// The port makes every decision over caller-owned byte buffers; it never opens
// a file. A caller that needs the native entry points serialises the same bytes
// the native code would have read and passes them here:
//
//   im_load      common_imatrix_load: try GGUF, fall back to the legacy format
//   im_gguf      the GGUF half alone
//   im_legacy    common_imatrix_load_legacy alone
//   im_normalize load_imatrix: the metadata rule and the per-entry scaling
//   im_prepare   prepare_imatrix: the include/exclude filters and m_last_call
//   rl_run       remap_layer over a tensor sequence, with mapped/next_id state
//   ri_run       remap_imatrix for one tensor name
//
// Result model. The native loader returns bool and leaves a partial map behind
// on failure; the port returns a status and leaves the outputs unspecified on
// any non-zero status. The status codes give the caller a reason for its log;
// they do not reproduce the native message text.
//
// Entry order. Native stores entries in std::map<std::string, ...>, so both
// loaders iterate in byte-lexicographic name order and a repeated name keeps
// its last value. The port writes the same order with the same last-wins rule.
//
// Floats. Sums are copied as raw f32 values and scaled in f32, like the native
// `float / float`; counts become int64 through std::lround.

// ---- status codes -------------------------------------------------------

const IM_OK: i64 = 0;
const IM_ERR_NOT_DATA: i64 = 1;   // n_entries < 1, or the file holds neither format
const IM_ERR_TRUNC: i64 = 2;      // a read ran past the buffer
const IM_ERR_NAME: i64 = 3;       // length field outside the signed range
const IM_ERR_COUNT: i64 = 4;      // nval < 1
const IM_ERR_PAIR: i64 = 5;       // sums without counts, or the reverse
const IM_ERR_TYPE: i64 = 6;       // a sums/counts tensor is not F32
const IM_ERR_META: i64 = 7;       // GGUF without the three imatrix.* keys
const IM_ERR_ASSERT: i64 = 8;     // native GGML_ASSERT path (typed KV access)
const IM_ERR_CAP: i64 = 9;        // a caller-provided output buffer is too small
const IM_ERR_RANGE: i64 = 10;     // std::stoi out_of_range in remap_layer

// ---- meta slots ---------------------------------------------------------

const M_N: i64 = 0;        // entries written
const M_NDS: i64 = 1;      // datasets written
const M_CHUNK: i64 = 2;    // chunk_count
const M_CHUNKSZ: i64 = 3;  // chunk_size
const M_LEGACY: i64 = 4;   // is_legacy
const M_HASMETA: i64 = 5;  // has_metadata
const M_LASTCALL: i64 = 6; // prepare_imatrix return value

// ---- ggml / GGUF constants ---------------------------------------------

const T_F32: i64 = 0;
const T_F16: i64 = 1;
const T_COUNT: i64 = 43;
const GGML_MAX_NAME: i64 = 64;
const GGUF_ALIGN: i64 = 32;

const G_UINT8: i64 = 0;
const G_INT8: i64 = 1;
const G_UINT16: i64 = 2;
const G_INT16: i64 = 3;
const G_UINT32: i64 = 4;
const G_INT32: i64 = 5;
const G_FLOAT32: i64 = 6;
const G_BOOL: i64 = 7;
const G_STRING: i64 = 8;
const G_ARRAY: i64 = 9;
const G_UINT64: i64 = 10;
const G_INT64: i64 = 11;
const G_FLOAT64: i64 = 12;

// ggml_type blck_size and type_size, transcribed from ggml.c type_traits.
// Types 4, 5, 31-33 and 36-38 are holes with no traits: blck_size 0.
fn blck_of(t: i64) -> i64 {
    if t == T_F32 || t == T_F16 || t == 24 || t == 25 || t == 26 || t == 27 || t == 28 || t == 30 {
        return 1;
    }
    if t == 2 || t == 3 || t == 6 || t == 7 || t == 8 || t == 9 || t == 20 || t == 39 { return 32; }
    if t == 40 || t == 42 { return 64; }
    if t == 41 { return 128; }
    if (t >= 10 && t <= 23) || t == 29 || t == 34 || t == 35 { return 256; }
    return 0;
}

fn tsize_of(t: i64) -> i64 {
    if t == T_F32 { return 4; }
    if t == T_F16 || t == 25 || t == 30 { return 2; }
    if t == 2 { return 18; }
    if t == 3 { return 20; }
    if t == 6 { return 22; }
    if t == 7 { return 24; }
    if t == 8 { return 34; }
    if t == 9 { return 36; }
    if t == 10 { return 84; }
    if t == 11 { return 110; }
    if t == 12 { return 144; }
    if t == 13 { return 176; }
    if t == 14 { return 210; }
    if t == 15 { return 292; }
    if t == 16 { return 66; }
    if t == 17 { return 74; }
    if t == 18 { return 98; }
    if t == 19 { return 50; }
    if t == 20 { return 18; }
    if t == 21 { return 110; }
    if t == 22 { return 82; }
    if t == 23 { return 136; }
    if t == 24 { return 1; }
    if t == 26 { return 4; }
    if t == 27 || t == 28 { return 8; }
    if t == 29 { return 56; }
    if t == 34 { return 54; }
    if t == 35 { return 66; }
    if t == 39 { return 17; }
    if t == 40 { return 36; }
    if t == 41 || t == 42 { return 18; }
    return 0;
}

// ---- little-endian readers ---------------------------------------------

fn rd_u8(b: const u8[:], p: i64) -> i64, bool {
    if p < 0 || p + 1 > b.len { return 0, false; }
    return b[p], true;
}

fn rd_u16(b: const u8[:], p: i64) -> i64, bool {
    if p < 0 || p + 2 > b.len { return 0, false; }
    return b[p] | ((b[p + 1] as i64) << 8), true;
}

fn rd_u32(b: const u8[:], p: i64) -> i64, bool {
    if p < 0 || p + 4 > b.len { return 0, false; }
    return b[p] | ((b[p + 1] as i64) << 8) | ((b[p + 2] as i64) << 16) |
        ((b[p + 3] as i64) << 24), true;
}

fn rd_i32(b: const u8[:], p: i64) -> i64, bool {
    let v, ok = rd_u32(b, p);
    if !ok { return 0, false; }
    if v >= 2147483648 { return v - 4294967296, true; }
    return v, true;
}

fn rd_i64(b: const u8[:], p: i64) -> i64, bool {
    if p < 0 || p + 8 > b.len { return 0, false; }
    let lo, okl = rd_u32(b, p);
    let hi, okh = rd_u32(b, p + 4);
    if !okl || !okh { return 0, false; }
    if hi >= 2147483648 { return 0, false; }
    return (hi << 32) | lo, true;
}

// An f32 from four bytes, bit for bit, including subnormals and NaN payloads.
fn rd_f32(b: const u8[:], p: i64) -> f32, bool {
    if p < 0 || p + 4 > b.len { return 0.0, false; }
    var img: u8[8] = [4, 0, 0, 0, 0, 0, 0, 0];
    img[1] = b[p];
    img[2] = b[p + 1];
    img[3] = b[p + 2];
    img[4] = b[p + 3];
    let a, ok = from_bytes<f32[>..]>(img[0..5]);
    if !ok { return 0.0, false; }
    return a[0], true;
}

// std::lround on a float: halves away from zero, exact for every f32.
fn lround32(x: f32) -> i64 {
    if x != x { return 0; }
    let d: f64 = x;
    if d >= 0.0 { return (d + 0.5) as! i64; }
    return (d - 0.5) as! i64;
}

// ---- name helpers -------------------------------------------------------

fn name_cmp(b: const u8[:], o1: i64, l1: i64, o2: i64, l2: i64) -> i64 {
    let m = if l1 < l2 { l1 } else { l2 };
    var i: i64 = 0;
    while i < m {
        let x = b[o1 + i] as i64;
        let y = b[o2 + i] as i64;
        if x < y { return -1; }
        if x > y { return 1; }
        i = i + 1;
    }
    if l1 < l2 { return -1; }
    if l1 > l2 { return 1; }
    return 0;
}

fn name_eq(b: const u8[:], o1: i64, l1: i64, o2: i64, l2: i64) -> bool {
    if l1 != l2 { return false; }
    return b[o1..o1 + l1] == b[o2..o2 + l2];
}

fn has_suffix(b: const u8[:], o: i64, l: i64, s: const u8[:]) -> bool {
    if l < s.len { return false; }
    return b[o + l - s.len..o + l] == s;
}

fn sub_find(b: const u8[:], o: i64, l: i64, s: const u8[:], so: i64, sl: i64) -> bool {
    if sl == 0 { return true; }
    if sl > l { return false; }
    var i: i64 = 0;
    while i + sl <= l {
        if b[o + i..o + i + sl] == s[so..so + sl] { return true; }
        i = i + 1;
    }
    return false;
}

fn nul_len(b: const u8[:], p: i64, l: i64) -> i64 {
    var i: i64 = 0;
    while i < l {
        if b[p + i] == 0 { return i; }
        i = i + 1;
    }
    return l;
}

// ---- record list (std::map<std::string, entry> semantics) ---------------

fn rec_put(b: const u8[:], name_off: i64[:], name_len: i64[:], n: i64,
           noff: i64, nlen: i64,
           s_off: i64[:], s_len: i64[:], c_off: i64[:], c_len: i64[:],
           so: i64, sl: i64, co: i64, cl: i64) -> i64 {
    var i: i64 = 0;
    while i < n {
        if name_eq(b, name_off[i], name_len[i], noff, nlen) {
            s_off[i] = so;
            s_len[i] = sl;
            c_off[i] = co;
            c_len[i] = cl;
            return n;
        }
        i = i + 1;
    }
    if n >= name_off.len { return -1; }
    name_off[n] = noff;
    name_len[n] = nlen;
    s_off[n] = so;
    s_len[n] = sl;
    c_off[n] = co;
    c_len[n] = cl;
    return n + 1;
}

fn sort_recs(b: const u8[:], name_off: i64[:], name_len: i64[:], s_off: i64[:],
             s_len: i64[:], c_off: i64[:], c_len: i64[:], n: i64) {
    var i: i64 = 1;
    while i < n {
        var j: i64 = i;
        while j > 0 && name_cmp(b, name_off[j - 1], name_len[j - 1], name_off[j], name_len[j]) > 0 {
            let a0 = name_off[j - 1]; name_off[j - 1] = name_off[j]; name_off[j] = a0;
            let a1 = name_len[j - 1]; name_len[j - 1] = name_len[j]; name_len[j] = a1;
            let a2 = s_off[j - 1]; s_off[j - 1] = s_off[j]; s_off[j] = a2;
            let a3 = s_len[j - 1]; s_len[j - 1] = s_len[j]; s_len[j] = a3;
            let a4 = c_off[j - 1]; c_off[j - 1] = c_off[j]; c_off[j] = a4;
            let a5 = c_len[j - 1]; c_len[j - 1] = c_len[j]; c_len[j] = a5;
            j = j - 1;
        }
        i = i + 1;
    }
}

// ---- legacy format ------------------------------------------------------

fn legacy_parse(b: const u8[:],
                name_off: i64[:], name_len: i64[:],
                s_off: i64[:], s_len: i64[:], spool: f32[:],
                c_off: i64[:], c_len: i64[:], cpool: i64[:],
                ds_off: i64[:], ds_len: i64[:], meta: i64[:]) -> i64 {
    meta[M_N] = 0;
    meta[M_NDS] = 0;
    meta[M_LEGACY] = 1;
    meta[M_HASMETA] = 0;
    meta[M_CHUNK] = 0;
    meta[M_CHUNKSZ] = 0;

    let n_entries, ok0 = rd_i32(b, 0);
    if !ok0 { return IM_ERR_TRUNC; }
    if n_entries < 1 { return IM_ERR_NOT_DATA; }

    var n: i64 = 0;
    var nds: i64 = 0;
    var sp: i64 = 0;
    var cp: i64 = 0;
    var p: i64 = 4;
    var i: i64 = 0;
    while i < n_entries {
        let l, ok1 = rd_i32(b, p);
        if !ok1 { return IM_ERR_TRUNC; }
        if l < 0 || l > 65535 { return IM_ERR_NAME; }
        p = p + 4;
        if p + l > b.len { return IM_ERR_TRUNC; }
        let noff = p;
        let nlen = nul_len(b, p, l);
        p = p + l;

        let ncall, ok2 = rd_i32(b, p);
        if !ok2 { return IM_ERR_TRUNC; }
        let nval, ok3 = rd_i32(b, p + 4);
        if !ok3 { return IM_ERR_TRUNC; }
        if nval < 1 { return IM_ERR_COUNT; }
        p = p + 8;
        if p + 4 * nval > b.len { return IM_ERR_TRUNC; }
        if sp + nval > spool.len || cp + 1 > cpool.len { return IM_ERR_CAP; }

        var j: i64 = 0;
        while j < nval {
            let f, okf = rd_f32(b, p + 4 * j);
            if !okf { return IM_ERR_TRUNC; }
            spool[sp + j] = f;
            j = j + 1;
        }
        let so = sp;
        sp = sp + nval;
        p = p + 4 * nval;

        cpool[cp] = ncall;
        let co = cp;
        cp = cp + 1;

        let nn = rec_put(b, name_off, name_len, n, noff, nlen,
                         s_off, s_len, c_off, c_len, so, nval, co, 1);
        if nn < 0 { return IM_ERR_CAP; }
        n = nn;
        i = i + 1;
    }

    // The trailing chunk count and dataset name are optional. A short read here
    // is not an error: native reads into zeroed locals, then only tests that
    // the stream stayed good for the dataset name.
    var chunk: i64 = 0;
    if p < b.len {
        let rem = b.len - p;
        var k: i64 = 0;
        var acc: i64 = 0;
        while k < rem && k < 4 {
            acc = acc | ((b[p + k] as i64) << (8 * k));
            k = k + 1;
        }
        if acc >= 2147483648 { acc = acc - 4294967296; }
        chunk = acc;
        if rem >= 4 {
            p = p + 4;
            if p < b.len {
                let l2, ok4 = rd_i32(b, p);
                if ok4 {
                    if l2 < 0 { return IM_ERR_NAME; }
                    p = p + 4;
                    if l2 > 0 && l2 <= 65535 && p + l2 <= b.len {
                        if nds >= ds_off.len { return IM_ERR_CAP; }
                        ds_off[nds] = p;
                        ds_len[nds] = nul_len(b, p, l2);
                        nds = nds + 1;
                    }
                }
            }
        }
    }

    sort_recs(b, name_off, name_len, s_off, s_len, c_off, c_len, n);
    meta[M_N] = n;
    meta[M_NDS] = nds;
    meta[M_CHUNK] = chunk;
    meta[M_CHUNKSZ] = 0;
    return IM_OK;
}

// ---- GGUF format --------------------------------------------------------

fn scalar_size(t: i64) -> i64 {
    if t == G_UINT8 || t == G_INT8 || t == G_BOOL { return 1; }
    if t == G_UINT16 || t == G_INT16 { return 2; }
    if t == G_UINT32 || t == G_INT32 || t == G_FLOAT32 { return 4; }
    if t == G_UINT64 || t == G_INT64 || t == G_FLOAT64 { return 8; }
    return 0;
}

fn align_up(v: i64, a: i64) -> i64 {
    let r = v % a;
    if r == 0 { return v; }
    return v + a - r;
}

fn gguf_parse(b: const u8[:],
              tk_off: i64[:], tk_len: i64[:], tkey_off: i64[:], tkey_len: i64[:],
              tkind: i64[:], tdata: i64[:], tnv: i64[:], ttype: i64[:],
              name_off: i64[:], name_len: i64[:],
              s_off: i64[:], s_len: i64[:], spool: f32[:],
              c_off: i64[:], c_len: i64[:], cpool: i64[:],
              ds_off: i64[:], ds_len: i64[:], meta: i64[:]) -> i64 {
    meta[M_N] = 0;
    meta[M_NDS] = 0;
    meta[M_LEGACY] = 0;
    meta[M_HASMETA] = 0;
    meta[M_CHUNK] = 0;
    meta[M_CHUNKSZ] = 0;

    if b.len < 24 { return IM_ERR_TRUNC; }
    if !(b[0..4] == "GGUF") { return IM_ERR_TRUNC; }

    let version, okv = rd_u32(b, 4);
    if !okv { return IM_ERR_TRUNC; }
    if version == 0 || version == 1 || version > 3 { return IM_ERR_TRUNC; }
    if (version & 0xFFFF) == 0 { return IM_ERR_TRUNC; }

    let n_tensors, okt = rd_i64(b, 8);
    if !okt || n_tensors < 0 { return IM_ERR_TRUNC; }
    let n_kv, okk = rd_i64(b, 16);
    if !okk || n_kv < 0 { return IM_ERR_TRUNC; }

    var p: i64 = 24;
    var i: i64 = 0;
    var align: i64 = GGUF_ALIGN;
    var has_ds: i64 = 0;
    var has_cc: i64 = 0;
    var has_cs: i64 = 0;
    var chunk: i64 = 0;
    var chunk_sz: i64 = 0;
    var nds: i64 = 0;
    var bad_u32 = false;
    while i < n_kv {
        let klen, ok1 = rd_i64(b, p);
        if !ok1 { return IM_ERR_TRUNC; }
        p = p + 8;
        if klen <= 0 || klen > b.len - p { return IM_ERR_TRUNC; }
        let koff = p;
        p = p + klen;

        let ty, ok2 = rd_u32(b, p);
        if !ok2 { return IM_ERR_TRUNC; }
        p = p + 4;
        var vt = ty;
        var vn: i64 = 1;
        var is_arr = false;
        if ty == G_ARRAY {
            let et, ok3 = rd_u32(b, p);
            if !ok3 { return IM_ERR_TRUNC; }
            let an, ok4 = rd_i64(b, p + 4);
            if !ok4 || an < 0 { return IM_ERR_TRUNC; }
            p = p + 12;
            vt = et;
            vn = an;
            is_arr = true;
        } else if ty != G_STRING && scalar_size(ty) == 0 {
            return IM_ERR_TRUNC;
        }

        let is_ds = b[koff..koff + klen] == "imatrix.datasets";
        let is_cc = b[koff..koff + klen] == "imatrix.chunk_count";
        let is_cs = b[koff..koff + klen] == "imatrix.chunk_size";
        let is_al = b[koff..koff + klen] == "general.alignment";
        if is_ds { has_ds = 1; }
        if is_cc { has_cc = 1; }
        if is_cs { has_cs = 1; }

        if vt == G_STRING {
            var j: i64 = 0;
            while j < vn {
                let sl, ok5 = rd_i64(b, p);
                if !ok5 { return IM_ERR_TRUNC; }
                p = p + 8;
                if sl < 0 || sl > b.len - p { return IM_ERR_TRUNC; }
                if is_ds && is_arr {
                    if nds >= ds_off.len { return IM_ERR_CAP; }
                    ds_off[nds] = p;
                    ds_len[nds] = sl;
                    nds = nds + 1;
                }
                p = p + sl;
                j = j + 1;
            }
        } else {
            let esz = scalar_size(vt);
            if esz == 0 { return IM_ERR_TRUNC; }
            if vn > (b.len - p) / esz { return IM_ERR_TRUNC; }
            if is_cc || is_cs || is_al {
                if is_arr || vt != G_UINT32 {
                    // general.alignment is checked by the parser; the two
                    // imatrix keys reach gguf_get_val_u32 and GGML_ASSERT.
                    if is_al { return IM_ERR_TRUNC; }
                    bad_u32 = true;
                } else {
                    let val, ok6 = rd_u32(b, p);
                    if !ok6 { return IM_ERR_TRUNC; }
                    if is_cc { chunk = val; }
                    if is_cs { chunk_sz = val; }
                    if is_al { align = val; }
                }
            }
            p = p + esz * vn;
        }
        i = i + 1;
    }

    if align <= 0 || (align & (align - 1)) != 0 { return IM_ERR_TRUNC; }

    if n_tensors < 1 { return IM_ERR_NOT_DATA; }
    if bad_u32 { return IM_ERR_ASSERT; }
    if n_tensors > tk_off.len { return IM_ERR_CAP; }

    var total: i64 = 0;
    i = 0;
    while i < n_tensors {
        let nl, ok1 = rd_i64(b, p);
        if !ok1 { return IM_ERR_TRUNC; }
        p = p + 8;
        if nl <= 0 || nl >= GGML_MAX_NAME || nl > b.len - p { return IM_ERR_TRUNC; }
        let noff = p;
        p = p + nl;
        var j: i64 = 0;
        while j < i {
            if name_eq(b, tk_off[j], tk_len[j], noff, nl) { return IM_ERR_TRUNC; }
            j = j + 1;
        }

        let nd, ok2 = rd_u32(b, p);
        if !ok2 || nd == 0 || nd > 4 { return IM_ERR_TRUNC; }
        p = p + 4;
        var ne: i64 = 1;
        j = 0;
        while j < 4 {
            var d: i64 = 1;
            if j < nd {
                let dv, ok3 = rd_i64(b, p);
                if !ok3 || dv < 0 { return IM_ERR_TRUNC; }
                p = p + 8;
                d = dv;
            }
            ne = ne * d;
            if ne < 0 { return IM_ERR_TRUNC; }
            j = j + 1;
        }
        let tt, ok4 = rd_u32(b, p);
        if !ok4 { return IM_ERR_TRUNC; }
        p = p + 4;
        let blck = blck_of(tt);
        if blck == 0 { return IM_ERR_TRUNC; }
        if ne % blck != 0 { return IM_ERR_TRUNC; }
        let off, ok5 = rd_i64(b, p);
        if !ok5 || off < 0 { return IM_ERR_TRUNC; }
        p = p + 8;

        let nb = ne / blck * tsize_of(tt);
        if off != total { return IM_ERR_TRUNC; }
        total = total + align_up(nb, align);
        if total < 0 { return IM_ERR_TRUNC; }

        tk_off[i] = noff;
        tk_len[i] = nl;
        ttype[i] = tt;
        tnv[i] = ne;
        tdata[i] = off;
        i = i + 1;
    }

    let data_start = align_up(p, align);
    if data_start < 0 || data_start + total > b.len { return IM_ERR_TRUNC; }

    // Map keys: strip .in_sum2 first, then .counts.
    i = 0;
    while i < n_tensors {
        var kl = tk_len[i];
        var kind: i64 = 0;
        if has_suffix(b, tk_off[i], kl, ".in_sum2") {
            kl = kl - 8;
            kind = 1;
        } else if has_suffix(b, tk_off[i], kl, ".counts") {
            kl = kl - 7;
            kind = 2;
        }
        tkey_off[i] = tk_off[i];
        tkey_len[i] = kl;
        tkind[i] = kind;
        tdata[i] = data_start + tdata[i];
        i = i + 1;
    }

    // std::map order: each distinct key once, sorted, last tensor of each kind wins.
    var written: i64 = 0;
    var sp: i64 = 0;
    var cp: i64 = 0;
    while written < n_tensors {
        var best: i64 = -1;
        i = 0;
        while i < n_tensors {
            if tkind[i] != 0 {
                var seen = false;
                var m: i64 = 0;
                while m < written {
                    if name_eq(b, name_off[m], name_len[m], tkey_off[i], tkey_len[i]) {
                        seen = true;
                        break;
                    }
                    m = m + 1;
                }
                if !seen {
                    if best < 0 ||
                       name_cmp(b, tkey_off[i], tkey_len[i], tkey_off[best], tkey_len[best]) < 0 {
                        best = i;
                    }
                }
            }
            i = i + 1;
        }
        if best < 0 { break; }

        var si: i64 = -1;
        var ci: i64 = -1;
        i = 0;
        while i < n_tensors {
            if tkind[i] != 0 && name_eq(b, tkey_off[i], tkey_len[i], tkey_off[best], tkey_len[best]) {
                if tkind[i] == 1 { si = i; } else { ci = i; }
            }
            i = i + 1;
        }
        if si < 0 || ci < 0 { return IM_ERR_PAIR; }
        if ttype[si] != T_F32 || ttype[ci] != T_F32 { return IM_ERR_TYPE; }

        let nval = tnv[si];
        let ncnt = tnv[ci];
        if sp + nval > spool.len || cp + ncnt > cpool.len { return IM_ERR_CAP; }
        let so = sp;
        var j: i64 = 0;
        while j < nval {
            let f, okf = rd_f32(b, tdata[si] + 4 * j);
            if !okf { return IM_ERR_TRUNC; }
            spool[sp + j] = f;
            j = j + 1;
        }
        sp = sp + nval;
        let co = cp;
        j = 0;
        while j < ncnt {
            let f, okf = rd_f32(b, tdata[ci] + 4 * j);
            if !okf { return IM_ERR_TRUNC; }
            cpool[cp + j] = lround32(f);
            j = j + 1;
        }
        cp = cp + ncnt;

        let nn = rec_put(b, name_off, name_len, written, tkey_off[best], tkey_len[best],
                         s_off, s_len, c_off, c_len, so, nval, co, ncnt);
        if nn < 0 { return IM_ERR_CAP; }
        written = nn;
    }

    meta[M_N] = written;
    meta[M_NDS] = nds;
    meta[M_HASMETA] = if has_ds != 0 && has_cc != 0 && has_cs != 0 { 1 } else { 0 };
    meta[M_CHUNK] = chunk;
    meta[M_CHUNKSZ] = chunk_sz;
    return IM_OK;
}

// ---- common_imatrix_load ------------------------------------------------

fn load_impl(b: const u8[:],
             tk_off: i64[:], tk_len: i64[:], tkey_off: i64[:], tkey_len: i64[:],
             tkind: i64[:], tdata: i64[:], tnv: i64[:], ttype: i64[:],
             name_off: i64[:], name_len: i64[:],
             s_off: i64[:], s_len: i64[:], spool: f32[:],
             c_off: i64[:], c_len: i64[:], cpool: i64[:],
             ds_off: i64[:], ds_len: i64[:], meta: i64[:]) -> i64 {
    let g = gguf_parse(b, tk_off, tk_len, tkey_off, tkey_len, tkind, tdata, tnv, ttype,
                       name_off, name_len, s_off, s_len, spool, c_off, c_len, cpool,
                       ds_off, ds_len, meta);
    if g == IM_OK || g == IM_ERR_ASSERT { return g; }
    return legacy_parse(b, name_off, name_len, s_off, s_len, spool, c_off, c_len, cpool,
                        ds_off, ds_len, meta);
}

// ---- load_imatrix (tools/quantize) --------------------------------------

fn normalize_impl(n: i64, s_off: i64[:], s_len: i64[:], spool: f32[:],
                  c_off: i64[:], c_len: i64[:], cpool: i64[:],
                  legacy: i64, has_meta: i64,
                  q_off: i64[:], q_len: i64[:], qpool: f32[:]) -> i64 {
    // `!is_legacy && !has_metadata` makes the native function exit(1).
    if legacy == 0 && has_meta == 0 { return IM_ERR_META; }

    var qp: i64 = 0;
    var i: i64 = 0;
    while i < n {
        let nval = s_len[i];
        let ncnt = c_len[i];
        if qp + nval > qpool.len { return IM_ERR_CAP; }
        q_off[i] = qp;
        q_len[i] = nval;
        var j: i64 = 0;
        while j < nval {
            qpool[qp + j] = 0.0;
            j = j + 1;
        }
        if legacy != 0 {
            var ncall: i64 = 0;
            if ncnt > 0 { ncall = cpool[c_off[i]]; }
            if ncall > 0 {
                let d: f32 = ncall;
                j = 0;
                while j < nval {
                    qpool[qp + j] = spool[s_off[i] + j] / d;
                    j = j + 1;
                }
            } else {
                j = 0;
                while j < nval {
                    qpool[qp + j] = spool[s_off[i] + j];
                    j = j + 1;
                }
            }
        } else if ncnt > 0 {
            // Per-expert counts: ne0 element groups; anything past the last
            // whole group stays zero, like the native resize-then-fill.
            let ne0 = nval / ncnt;
            var g: i64 = 0;
            while g < ncnt {
                let cnt = cpool[c_off[i] + g];
                if cnt > 0 {
                    let d: f32 = cnt;
                    j = 0;
                    while j < ne0 {
                        qpool[qp + g * ne0 + j] = spool[s_off[i] + g * ne0 + j] / d;
                        j = j + 1;
                    }
                } else {
                    j = 0;
                    while j < ne0 {
                        qpool[qp + g * ne0 + j] = 1.0;
                        j = j + 1;
                    }
                }
                g = g + 1;
            }
        }
        qp = qp + nval;
        i = i + 1;
    }
    return IM_OK;
}

// ---- prepare_imatrix ----------------------------------------------------

fn prepare_impl(n: i64, name_off: i64[:], name_len: i64[:], b: const u8[:],
                ex: const u8[:], ex_off: i64[:], ex_len: i64[:], nex: i64,
                inb: const u8[:], in_off: i64[:], in_len: i64[:], nin: i64,
                out: i64[:]) -> i64 {
    var cnt: i64 = 0;
    var i: i64 = 0;
    while i < n {
        var keep = true;
        var k: i64 = 0;
        while k < nex {
            if sub_find(b, name_off[i], name_len[i], ex, ex_off[k], ex_len[k]) {
                keep = false;
                break;
            }
            k = k + 1;
        }
        if keep && nin > 0 {
            var hit = false;
            k = 0;
            while k < nin {
                if sub_find(b, name_off[i], name_len[i], inb, in_off[k], in_len[k]) {
                    hit = true;
                    break;
                }
                k = k + 1;
            }
            keep = hit;
        }
        if keep {
            if cnt >= out.len { return IM_ERR_CAP; }
            out[cnt] = i;
            cnt = cnt + 1;
        }
        i = i + 1;
    }
    return cnt;
}

// ---- remap_layer / remap_imatrix ---------------------------------------

fn is_digit(c: u8) -> bool {
    return c >= 48 && c <= 57;
}

// Leftmost `blk\.(\d+)\.` match: the position of the first digit, or -1.
fn blk_at(b: const u8[:], o: i64, l: i64) -> i64 {
    var i: i64 = 0;
    while i + 5 <= l {
        if b[o + i] == 98 && b[o + i + 1] == 108 && b[o + i + 2] == 107 && b[o + i + 3] == 46 {
            var j: i64 = i + 4;
            while j < l && is_digit(b[o + j]) { j = j + 1; }
            if j > i + 4 && j < l && b[o + j] == 46 {
                return o + i + 4;
            }
        }
        i = i + 1;
    }
    return -1;
}

fn digits_end(b: const u8[:], p: i64, end: i64) -> i64 {
    var j: i64 = p;
    while j < end && is_digit(b[j]) { j = j + 1; }
    return j;
}

// std::stoi on the digit run: -1 when it exceeds int (native throws).
fn digits_val(b: const u8[:], p: i64, e: i64) -> i64 {
    var v: i64 = 0;
    var i: i64 = p;
    while i < e {
        v = v * 10 + b[i] - 48;
        if v > 2147483647 { return -1; }
        i = i + 1;
    }
    return v;
}

fn dec_len(v: i64) -> i64 {
    if v < 0 { return 0; }
    var n: i64 = 1;
    var x: i64 = v;
    while x >= 10 {
        x = x / 10;
        n = n + 1;
    }
    return n;
}

fn put_dec(dst: u8[:], at: i64, v: i64) -> i64 {
    let n = dec_len(v);
    var i: i64 = n;
    var x: i64 = v;
    while i > 0 {
        i = i - 1;
        dst[at + i] = (48 + (x % 10)) as u8;
        x = x / 10;
    }
    return at + n;
}

fn map_find(mapk: i64[:], nmap: i64, k: i64) -> i64 {
    var i: i64 = 0;
    while i < nmap {
        if mapk[i] == k { return i; }
        i = i + 1;
    }
    return -1;
}

fn in_prune(prune: i64[:], np: i64, blk: i64) -> bool {
    var i: i64 = 0;
    while i < np {
        if prune[i] == blk { return true; }
        i = i + 1;
    }
    return false;
}

fn dec_eq(b: const u8[:], p: i64, e: i64, v: i64) -> bool {
    if v < 0 { return false; }
    if e - p != dec_len(v) { return false; }
    return digits_val(b, p, e) == v;
}

// One remap_layer call. st[0] is next_id, nmap[0] the map size; both stay live
// across calls, like the caller's `mapped` and `blk_id`.
fn remap_one(b: const u8[:], o: i64, l: i64, prune: i64[:], np: i64,
             mapk: i64[:], mapv: i64[:], nmap: i64[:], st: i64[:],
             out: u8[:], op: i64[:], keep: i64[:], idx: i64) -> i64 {
    let at = blk_at(b, o, l);
    if np == 0 || at < 0 {
        if op[0] + l > out.len { return IM_ERR_CAP; }
        var i: i64 = 0;
        while i < l {
            out[op[0] + i] = b[o + i];
            i = i + 1;
        }
        keep[idx] = 1;
        op[0] = op[0] + l;
        return IM_OK;
    }

    let dend = digits_end(b, at, o + l);
    let blk = digits_val(b, at, dend);
    if blk < 0 { return IM_ERR_RANGE; }

    var val: i64 = -1;
    let k = map_find(mapk, nmap[0], blk);
    if k >= 0 {
        val = mapv[k];
    } else if in_prune(prune, np, blk) {
        if nmap[0] >= mapk.len { return IM_ERR_CAP; }
        mapk[nmap[0]] = blk;
        mapv[nmap[0]] = -1;
        nmap[0] = nmap[0] + 1;
    } else if blk < prune[0] {
        if nmap[0] >= mapk.len { return IM_ERR_CAP; }
        mapk[nmap[0]] = blk;
        mapv[nmap[0]] = blk;
        nmap[0] = nmap[0] + 1;
        st[0] = blk + 1;
        val = blk;
    } else {
        if nmap[0] >= mapk.len { return IM_ERR_CAP; }
        mapk[nmap[0]] = blk;
        mapv[nmap[0]] = st[0];
        nmap[0] = nmap[0] + 1;
        st[0] = st[0] + 1;
        val = mapv[nmap[0] - 1];
    }

    if val < 0 {
        keep[idx] = 0;
        return IM_OK;
    }
    let need = l + 4 + 4;
    if op[0] + need > out.len { return IM_ERR_CAP; }
    let start = op[0];
    var i2: i64 = 0;
    while i2 < at - o {
        out[start + i2] = b[o + i2];
        i2 = i2 + 1;
    }
    let mid = put_dec(out, start + (at - o), val);
    var i3: i64 = dend;
    while i3 < o + l {
        out[mid + (i3 - dend)] = b[i3];
        i3 = i3 + 1;
    }
    keep[idx] = 1;
    op[0] = mid + (o + l - dend);
    return IM_OK;
}

// ---- exports ------------------------------------------------------------

export fn im_load(b: const u8[:],
                  tk_off: i64[:], tk_len: i64[:], tkey_off: i64[:], tkey_len: i64[:],
                  tkind: i64[:], tdata: i64[:], tnv: i64[:], ttype: i64[:],
                  name_off: i64[:], name_len: i64[:],
                  s_off: i64[:], s_len: i64[:], spool: f32[:],
                  c_off: i64[:], c_len: i64[:], cpool: i64[:],
                  ds_off: i64[:], ds_len: i64[:], meta: i64[:]) -> i64 {
    return load_impl(b, tk_off, tk_len, tkey_off, tkey_len, tkind, tdata, tnv, ttype,
                     name_off, name_len, s_off, s_len, spool, c_off, c_len, cpool,
                     ds_off, ds_len, meta);
}

export fn im_gguf(b: const u8[:],
                  tk_off: i64[:], tk_len: i64[:], tkey_off: i64[:], tkey_len: i64[:],
                  tkind: i64[:], tdata: i64[:], tnv: i64[:], ttype: i64[:],
                  name_off: i64[:], name_len: i64[:],
                  s_off: i64[:], s_len: i64[:], spool: f32[:],
                  c_off: i64[:], c_len: i64[:], cpool: i64[:],
                  ds_off: i64[:], ds_len: i64[:], meta: i64[:]) -> i64 {
    return gguf_parse(b, tk_off, tk_len, tkey_off, tkey_len, tkind, tdata, tnv, ttype,
                      name_off, name_len, s_off, s_len, spool, c_off, c_len, cpool,
                      ds_off, ds_len, meta);
}

export fn im_legacy(b: const u8[:],
                    name_off: i64[:], name_len: i64[:],
                    s_off: i64[:], s_len: i64[:], spool: f32[:],
                    c_off: i64[:], c_len: i64[:], cpool: i64[:],
                    ds_off: i64[:], ds_len: i64[:], meta: i64[:]) -> i64 {
    return legacy_parse(b, name_off, name_len, s_off, s_len, spool, c_off, c_len, cpool,
                        ds_off, ds_len, meta);
}

export fn im_normalize(n: i64, s_off: i64[:], s_len: i64[:], spool: f32[:],
                       c_off: i64[:], c_len: i64[:], cpool: i64[:],
                       legacy: i64, has_meta: i64,
                       q_off: i64[:], q_len: i64[:], qpool: f32[:]) -> i64 {
    return normalize_impl(n, s_off, s_len, spool, c_off, c_len, cpool, legacy, has_meta,
                          q_off, q_len, qpool);
}

export fn im_prepare(n: i64, name_off: i64[:], name_len: i64[:], b: const u8[:],
                     ex: const u8[:], ex_off: i64[:], ex_len: i64[:], nex: i64,
                     inb: const u8[:], in_off: i64[:], in_len: i64[:], nin: i64,
                     has_file: i64, chunk: i64, out: i64[:], meta: i64[:]) -> i64 {
    meta[M_LASTCALL] = if has_file != 0 { chunk } else { -1 };
    return prepare_impl(n, name_off, name_len, b, ex, ex_off, ex_len, nex,
                        inb, in_off, in_len, nin, out);
}

export fn rl_run(b: const u8[:], off: i64[:], len: i64[:], n: i64,
                 prune: i64[:], np: i64,
                 mapk: i64[:], mapv: i64[:], nmap: i64[:], st: i64[:],
                 out: u8[:], o_off: i64[:], o_len: i64[:], keep: i64[:]) -> i64 {
    var op: i64[1] = [0];
    var i: i64 = 0;
    while i < n {
        o_off[i] = op[0];
        let r = remap_one(b, off[i], len[i], prune, np, mapk, mapv, nmap, st, out, op, keep, i);
        if r != IM_OK { return r; }
        o_len[i] = op[0] - o_off[i];
        i = i + 1;
    }
    // std::map iteration order: ascending key. Only the presentation of the
    // final map depends on it; lookups are by key.
    var a: i64 = 1;
    while a < nmap[0] {
        var c: i64 = a;
        while c > 0 && mapk[c - 1] > mapk[c] {
            let k0 = mapk[c - 1]; mapk[c - 1] = mapk[c]; mapk[c] = k0;
            let v0 = mapv[c - 1]; mapv[c - 1] = mapv[c]; mapv[c] = v0;
            c = c - 1;
        }
        a = a + 1;
    }
    return IM_OK;
}

export fn ri_run(b: const u8[:], o: i64, l: i64,
                 mapk: i64[:], mapv: i64[:], nmap: i64,
                 out: u8[:], o_off: i64[:], o_len: i64[:]) -> i64 {
    o_off[0] = 0;
    o_len[0] = 0;
    // Both unchanged paths still return the name: the native function returns
    // orig_name, and the caller stores it in tensor_metadata.
    if nmap == 0 || blk_at(b, o, l) < 0 {
        if l > out.len { return IM_ERR_CAP; }
        var c: i64 = 0;
        while c < l {
            out[c] = b[o + c];
            c = c + 1;
        }
        o_len[0] = l;
        return IM_OK;
    }
    let at = blk_at(b, o, l);
    let dend = digits_end(b, at, o + l);
    var i: i64 = 0;
    while i < nmap {
        if dec_eq(b, at, dend, mapv[i]) {
            let need = l + dec_len(mapk[i]) + 4;
            if need > out.len { return IM_ERR_CAP; }
            var k: i64 = 0;
            while k < at - o {
                out[k] = b[o + k];
                k = k + 1;
            }
            let mid = put_dec(out, at - o, mapk[i]);
            var m: i64 = dend;
            while m < o + l {
                out[mid + (m - dend)] = b[m];
                m = m + 1;
            }
            o_len[0] = mid + (o + l - dend);
            return 1;
        }
        i = i + 1;
    }
    return -1;
}
