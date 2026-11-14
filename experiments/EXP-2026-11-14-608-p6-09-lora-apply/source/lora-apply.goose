// LoRA application, scale and cleanup in Goose (P6-09 S2), from
// llm_graph_context::build_lora_mm() in src/llama-graph.cpp:1629-1671,
// llama_adapter_lora::get_weight() in src/llama-adapter.cpp:140-150,
// llama_adapter_lora_weight::get_scale() in src/llama-adapter.h:52-57,
// llama_adapter_lora::get_n_nodes() in src/llama-adapter.h:85-87 and
// llama_adapter_lora_free() in src/llama-adapter.cpp:503-515
// (flint tree, base commit 05048b02).
//
// Owned decisions, no graph, no weights:
//   - selection: a weight tensor w participates only when an adapter holds a
//     pair under exactly that name (native get_weight), otherwise the adapter
//     contributes nothing to that matrix multiply;
//   - per-adapter application scale: alpha == 0 keeps the caller scale, else
//     caller_scale * alpha / rank with rank = b.ne[0] (native get_scale, the
//     same formula the loader stores in lora_scale);
//   - node sequence per applied adapter, in native order: a x cur, then
//     b x (a x cur), then scale, then add onto the base mul_mat result;
//   - batch semantics: cur is [n_embd, n_tokens, ne2, ne3]; the a-matmul
//     yields [rank, n_tokens, ne2, ne3], the b-matmul and the delta keep
//     [n_out, n_tokens, ne2, ne3], and the add is elementwise on that shape;
//   - node budget: native get_n_nodes() reserves 6 nodes per pair;
//   - cleanup: free unregisters the adapter from model.loras while it still
//     points at the model, then releases it; a null adapter is a no-op and an
//     adapter that was never registered only gets released.
//
// Flat layouts (no structs):
//   adapters: a_alpha / a_caller are f32 per adapter, in the caller's order
//     (the runtime iterates an unordered map, so the order is the caller's,
//     not the port's);
//   pairs: PAIR_SLOTS i64 per pair = adapter index, base-name offset in
//     `names`, base-name length, a tensor index, b tensor index; one entry per
//     base name, as the S1 pair map guarantees;
//   adapter tensors: t_ne0 / t_ne1 hold a and b geometry;
//   the weight: its name range in `names` and w_ne1, the output width the add
//     must match; the base matmul precondition w.ne0 == cur.ne0 is outside the
//     LoRA loop and is not re-checked here;
//   the activation: cur_ne0..cur_ne3;
//   st[AP_SLOTS * i] per adapter = applied, reason, pair index, delta ne0,
//     delta ne1; fs[i] = the application scale of adapter i (0.0 when skipped);
//   out[OUT_SLOTS] = code, applied count, LoRA node count, delta ne2,
//     delta ne3, pairs seen.
//
// Return codes:
//    0 ok
//   -1 bad argument or caller capacity (port-only guard)
//   -2 activation, weight and adapter shapes cannot form the four nodes
//      (native GGML_ASSERT inside ggml_mul_mat/ggml_add)
//   -3 rank is not positive (native divides by zero into an inf scale)
//   -1 also covers a pair table whose adapter index or tensor index is out of
//      range, which the loader cannot produce
//
// Reason codes in st[1]: 0 applied, 1 adapter has no pair for this weight.

const E_OK: i64 = 0;
const E_ARG: i64 = -1;
const E_SHAPE: i64 = -2;
const E_RANK: i64 = -3;

const R_APPLIED: i64 = 0;
const R_NO_WEIGHT: i64 = 1;

const PAIR_SLOTS: i64 = 5;
const AP_SLOTS: i64 = 5;
const OUT_SLOTS: i64 = 6;

// Node budget of one adapter: llama_adapter_lora::get_n_nodes().
export fn lora_budget(n_pairs: i64) -> i64 {
    if n_pairs < 0 {
        return 0;
    }
    return n_pairs * 6;
}

// llama_adapter_lora_weight::get_scale: alpha == 0 keeps the caller scale,
// otherwise rank divides caller_scale * alpha. The loader (S1) owns the same
// formula as lora_scale; the application reads it again because that is what
// build_lora_mm does for every call.
export fn lora_app_scale(alpha: f32, caller_scale: f32, rank: i64) -> f32 {
    var scale = caller_scale;
    if alpha != 0.0 {
        let r: f32 = rank;
        scale = caller_scale * alpha / r;
    }
    return scale;
}

fn name_eq(p: const u8[:], ao: i64, al: i64, bo: i64, bl: i64) -> i64 {
    if al != bl {
        return 0;
    }
    if ao < 0 || bo < 0 || ao + al > p.len || bo + bl > p.len {
        return 0;
    }
    var i: i64 = 0;
    while i < al {
        if p[ao + i] != p[bo + i] {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

// First pair of adapter `ad` whose base name is the weight name. The S1 pair
// table is a map, so a second entry for one name cannot reach this function.
fn pair_for_adapter(
    n_pairs: i64,
    pairs: const i64[:],
    names: const u8[:],
    w_off: i64,
    w_len: i64,
    ad: i64) -> i64 {

    var p: i64 = 0;
    while p < n_pairs {
        if pairs[PAIR_SLOTS * p + 0] == ad {
            let noff = pairs[PAIR_SLOTS * p + 1];
            let nlen = pairs[PAIR_SLOTS * p + 2];
            if name_eq(names, noff, nlen, w_off, w_len) != 0 {
                return p;
            }
        }
        p = p + 1;
    }
    return -1;
}

// The four nodes of one applied adapter must fit: a.ne0 == cur.ne0,
// b.ne0 == a.ne1 (the transpose check the loader already made), and
// b.ne1 == w.ne1 so the delta has the shape of the base mul_mat result.
fn apply_geometry(
    pairs: const i64[:],
    p: i64,
    t_ne0: const i64[:],
    t_ne1: const i64[:],
    w_ne1: i64,
    cur_ne0: i64) -> i64 {

    let a = pairs[PAIR_SLOTS * p + 3];
    let b = pairs[PAIR_SLOTS * p + 4];

    if t_ne0[a] != cur_ne0 {
        return E_SHAPE;
    }
    if t_ne1[b] != w_ne1 {
        return E_SHAPE;
    }
    if t_ne0[b] != t_ne1[a] {
        return E_SHAPE;
    }
    if t_ne0[b] <= 0 {
        return E_RANK;
    }
    return E_OK;
}

// llm_graph_context::build_lora_mm for one weight tensor: which adapters
// contribute, their scale, the delta geometry and the node count.
export fn lora_apply_dense(
    n_ad: i64,
    a_alpha: const f32[:],
    a_caller: const f32[:],
    n_pairs: i64,
    pairs: const i64[:],
    names: const u8[:],
    w_off: i64,
    w_len: i64,
    t_ne0: const i64[:],
    t_ne1: const i64[:],
    w_ne1: i64,
    cur_ne0: i64,
    cur_ne1: i64,
    cur_ne2: i64,
    cur_ne3: i64,
    st: i64[:],
    fs: f32[:],
    out: i64[:]) -> i64 {

    if n_ad < 0 || n_pairs < 0 {
        return E_ARG;
    }
    if a_alpha.len < n_ad || a_caller.len < n_ad {
        return E_ARG;
    }
    if pairs.len < PAIR_SLOTS * n_pairs || st.len < AP_SLOTS * n_ad || fs.len < n_ad {
        return E_ARG;
    }
    if out.len < OUT_SLOTS {
        return E_ARG;
    }
    if w_off < 0 || w_len < 0 || w_off + w_len > names.len {
        return E_ARG;
    }
    if w_ne1 <= 0 || cur_ne0 <= 0 || cur_ne1 <= 0 || cur_ne2 <= 0 || cur_ne3 <= 0 {
        return E_ARG;
    }

    var i: i64 = 0;
    while i < n_ad {
        st[AP_SLOTS * i + 0] = 0;
        st[AP_SLOTS * i + 1] = R_NO_WEIGHT;
        st[AP_SLOTS * i + 2] = -1;
        st[AP_SLOTS * i + 3] = 0;
        st[AP_SLOTS * i + 4] = 0;
        fs[i] = 0.0;
        i = i + 1;
    }
    out[0] = E_OK;
    out[1] = 0;
    out[2] = 0;
    out[3] = cur_ne2;
    out[4] = cur_ne3;
    out[5] = n_pairs;

    // Guard pass: every matching pair must form the four nodes before any
    // adapter is applied, so a bad table cannot leave a half-built graph.
    var p: i64 = 0;
    while p < n_pairs {
        let ad = pairs[PAIR_SLOTS * p + 0];
        let noff = pairs[PAIR_SLOTS * p + 1];
        let nlen = pairs[PAIR_SLOTS * p + 2];
        let a = pairs[PAIR_SLOTS * p + 3];
        let b = pairs[PAIR_SLOTS * p + 4];
        if ad < 0 || ad >= n_ad {
            out[0] = E_ARG;
            return E_ARG;
        }
        if noff < 0 || nlen < 0 || noff + nlen > names.len {
            out[0] = E_ARG;
            return E_ARG;
        }
        if a < 0 || b < 0 || a >= t_ne0.len || a >= t_ne1.len || b >= t_ne0.len || b >= t_ne1.len {
            out[0] = E_ARG;
            return E_ARG;
        }
        if name_eq(names, noff, nlen, w_off, w_len) != 0 {
            let rc = apply_geometry(pairs, p, t_ne0, t_ne1, w_ne1, cur_ne0);
            if rc != E_OK {
                out[0] = rc;
                return rc;
            }
        }
        p = p + 1;
    }

    // Application pass, adapter by adapter in the caller's order.
    i = 0;
    while i < n_ad {
        let hit = pair_for_adapter(n_pairs, pairs, names, w_off, w_len, i);
        if hit >= 0 {
            let b = pairs[PAIR_SLOTS * hit + 4];
            let rank = t_ne0[b];
            let scale = lora_app_scale(a_alpha[i], a_caller[i], rank);
            st[AP_SLOTS * i + 0] = 1;
            st[AP_SLOTS * i + 1] = R_APPLIED;
            st[AP_SLOTS * i + 2] = hit;
            st[AP_SLOTS * i + 3] = t_ne1[b];
            st[AP_SLOTS * i + 4] = cur_ne1;
            fs[i] = scale;
            out[1] = out[1] + 1;
        }
        i = i + 1;
    }

    out[2] = out[1] * 4;
    return E_OK;
}

// llama_adapter_lora_free: unregister from model.loras, then release. The
// registry is the caller's ordered copy of that set; the freed entry is
// removed and the rest keeps its order and scale.
// st[0] = 1 when the adapter was registered, st[1] = the new size, st[2] = code.
export fn lora_cleanup(
    ids: i64[:],
    n: i64,
    target: i64,
    st: i64[:]) -> i64 {

    if n < 0 || n > ids.len || st.len < 3 || target < 0 {
        return E_ARG;
    }
    st[0] = 0;
    st[1] = n;
    st[2] = E_OK;

    var at: i64 = -1;
    var i: i64 = 0;
    while i < n {
        if ids[i] == target && at < 0 {
            at = i;
        }
        i = i + 1;
    }
    if at < 0 {
        // Native erase() on a missing key is a no-op; the adapter is still
        // released, which is not registry state.
        return E_OK;
    }

    st[0] = 1;
    i = at;
    while i + 1 < n {
        ids[i] = ids[i + 1];
        i = i + 1;
    }
    if n > 0 {
        ids[n - 1] = 0;
    }
    st[1] = n - 1;
    return E_OK;
}
