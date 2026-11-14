// Control vectors in Goose, from src/llama-adapter.cpp:16-138 and
// src/llama-adapter.h:17-46 (flint tree, commit base 6395755a).
//
// Owned decisions:
//   - the per-layer direction table: layer 0 never has a direction, layers
//     1..n_layer-1 get one n_embd-wide F32 vector, zeroed at init;
//   - init once (a second init is the native GGML_ASSERT);
//   - apply: a null direction pointer disables; n_embd mismatch is refused
//     before any state change; the layer range is stored unvalidated; the
//     copy is layer-major at off = n_embd*(il-1) and only when
//     off + n_embd <= len, so a short vector leaves later layers untouched;
//   - tensor_for bounds: negative, below layer_start, above layer_end and
//     past the table are all "no direction";
//   - apply_to: the direction is broadcast over every row of cur, exactly like
//     ggml_add of a 1-D [n_embd] tensor with [n_embd, n_rows].
//
// Boundaries kept native: buffer type selection per layer
// (model.select_buft), ggml context/buffer allocation, backend tensor writes,
// and the debug log texts.
//
// Flat layouts (no structs):
//   S (i64), state:
//     [0] n_layer, [1] n_embd, [2] layer_start, [3] layer_end,
//     [4] initialized, [5] tensor count (= n_layer), [6..] reserved.
//   T (i64), one 3-slot descriptor per layer at T_SLOTS*il:
//     present, direction offset in F, direction length (n_embd).
//   F (f32), direction pool; layer il lives at il*n_embd, layer 0 unused.
//
// Return codes:
//   0 ok; -1 bad argument/capacity/shape; -2 direction pool too small;
//   -3 init on an already initialized state (native assert).

const S_NLAYER: i64 = 0;
const S_NEMBD: i64 = 1;
const S_START: i64 = 2;
const S_END: i64 = 3;
const S_INIT: i64 = 4;
const S_TCOUNT: i64 = 5;
const S_WORDS: i64 = 6;

const T_SLOTS: i64 = 3;

const MAX_LAYERS: i64 = 512;

fn cv_tbase(il: i64) -> i64 {
    return T_SLOTS * il;
}

// Construct an empty adapter state: no tensors yet, disabled range.
export fn cv_init_state(S: i64[:], T: i64[:], n_layer: i64, n_embd: i64) -> i64 {
    if S.len < S_WORDS {
        return -1;
    }
    if n_layer < 0 || n_layer > MAX_LAYERS || n_embd <= 0 {
        return -1;
    }
    if T.len < T_SLOTS * n_layer {
        return -1;
    }
    S[S_NLAYER] = n_layer;
    S[S_NEMBD]  = n_embd;
    S[S_START]  = -1;
    S[S_END]    = -1;
    S[S_INIT]   = 0;
    S[S_TCOUNT] = n_layer;

    var i: i64 = 0;

    while i < T_SLOTS * n_layer {
        T[i] = 0;
        i = i + 1;
    }
    return 0;
}

// Native init: one zeroed F32 direction per layer except layer 0.
fn cv_init_impl(S: i64[:], T: i64[:], F: f32[:]) -> i64 {
    if S.len < S_WORDS {
        return -1;
    }
    if S[S_INIT] != 0 {
        return -3;
    }
    let n = S[S_NLAYER];
    let e = S[S_NEMBD];

    if n < 0 || n > MAX_LAYERS || e <= 0 {
        return -1;
    }
    if T.len < T_SLOTS * n {
        return -1;
    }
    if F.len < n * e {
        return -2;
    }

    var il: i64 = 1;

    while il < n {
        let tb = cv_tbase(il);
        let base = il * e;
        T[tb]     = 1;
        T[tb + 1] = base;
        T[tb + 2] = e;

        var k: i64 = 0;

        while k < e {
            F[base + k] = 0.0;
            k = k + 1;
        }
        il = il + 1;
    }
    S[S_INIT] = 1;
    return 0;
}

export fn cv_init(S: i64[:], T: i64[:], F: f32[:]) -> i64 {
    return cv_init_impl(S, T, F);
}

// Native apply with a non-null data pointer.
export fn cv_apply(
    S: i64[:],
    T: i64[:],
    F: f32[:],
    data: const f32[:],
    n_embd: i64,
    il_start: i64,
    il_end: i64) -> i64 {

    if S.len < S_WORDS {
        return -1;
    }
    let e = S[S_NEMBD];

    if n_embd != e {
        return -1;
    }
    if S[S_INIT] == 0 {
        let ri = cv_init_impl(S, T, F);

        if ri != 0 {
            return ri;
        }
    }
    let n = S[S_NLAYER];

    S[S_START] = il_start;
    S[S_END]   = il_end;

    var il: i64 = 1;

    while il < n {
        let off = n_embd * (il - 1);

        if off >= 0 && off + n_embd <= data.len {
            let base = T[cv_tbase(il) + 1];

            var k: i64 = 0;

            while k < n_embd {
                F[base + k] = data[off + k];
                k = k + 1;
            }
        }
        il = il + 1;
    }
    return 0;
}

// Native apply with data == nullptr: disable, keep the allocated directions.
export fn cv_disable(S: i64[:]) -> i64 {
    if S.len < S_WORDS {
        return -1;
    }
    S[S_START] = -1;
    S[S_END]   = -1;
    return 0;
}

// Direction offset in F for layer il, or -1 when tensor_for would return null.
fn cv_direction_off_impl(S: const i64[:], T: const i64[:], il: i64) -> i64 {
    if S.len < S_WORDS {
        return -1;
    }
    if il < 0 {
        return -1;
    }
    if il < S[S_START] || il > S[S_END] {
        return -1;
    }
    if il >= S[S_NLAYER] {
        return -1;
    }
    let tb = cv_tbase(il);

    if tb + 2 >= T.len {
        return -1;
    }
    if T[tb] == 0 {
        return -1;
    }
    return T[tb + 1];
}

export fn cv_direction_off(S: const i64[:], T: const i64[:], il: i64) -> i64 {
    return cv_direction_off_impl(S, T, il);
}

// Native apply_to: cur += direction for every row of cur. A missing direction
// leaves cur untouched (native returns the same tensor pointer).
export fn cv_apply_to(
    S: const i64[:],
    T: const i64[:],
    F: const f32[:],
    cur: f32[:],
    il: i64) -> i64 {

    let off = cv_direction_off_impl(S, T, il);

    if off < 0 {
        return 0;
    }
    let e = S[S_NEMBD];

    if e <= 0 || cur.len % e != 0 {
        return -1;
    }
    if off + e > F.len {
        return -1;
    }
    let rows = cur.len / e;

    var r: i64 = 0;

    while r < rows {
        var k: i64 = 0;

        while k < e {
            cur[r * e + k] = cur[r * e + k] + F[off + k];
            k = k + 1;
        }
        r = r + 1;
    }
    return 0;
}
