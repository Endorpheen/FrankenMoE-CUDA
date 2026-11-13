// llama_batch_allocr::ubatch_add classification in Goose (APX: P6-07).
//
// Native (llama-batch.cpp:871-887):
//     mixed_batch = !is_embd_vec.empty()
//     n_embd_rows = sum over idxs of is_embd_vec[idx]      (0 when !mixed_batch)
//     mixed     = mixed_batch && n_embd_rows > 0 && n_embd_rows < n_tokens
//     use_token = batch.token && !(mixed_batch && n_embd_rows == n_tokens)
//     use_embd  = batch.embd  && !(mixed_batch && n_embd_rows == 0)
//     n_embd_all  = use_embd  ? n_tokens*n_embd       : 0
//     n_state_all = has_state ? n_tokens*n_embd_state : 0
//     n_pos_all   =             n_tokens*n_pos_per_embd
//
// Two asymmetries that are easy to invert and hard to notice:
//   * use_token is suppressed only when ALL rows are embeddings; use_embd is suppressed only when NONE
//     are. Swapping the two equality tests produces a ubatch that is entirely the wrong kind for a batch
//     that is entirely one kind -- which still has the right token count.
//   * n_pos_all is unconditional. A ubatch of embeddings still carries n_pos per row, and zeroing it makes
//     the positions array short by n_tokens*n_pos_per_embd floats.
//
// Note n_embd_all is multiplied by n_tokens while n_pos_all is not, because n_pos_per_embd is already a
// per-token count. That difference is in the native code and is preserved rather than "fixed".
extern "fg241u_fail" fn fail(code: i64);
extern "fg241u_ld_i32" fn ld_i32(base: u64, idx: i64) -> i64;
extern "fg241u_st_i64" fn st_i64(base: u64, idx: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// classify returns the number of embedding rows and writes
// [mixed, use_token, use_embd, n_embd_all, n_state_all, n_pos_all] to out.

fn classify(
    n_tokens: i64,
    mixed_batch: i64,
    is_embd: u64,
    idxs: u64,
    has_token: i64,
    has_embd: i64,
    has_state: i64,
    n_embd: i64,
    n_embd_state: i64,
    n_pos_per_embd: i64,
    out: u64) -> i64 {
    ensure(n_tokens > 0 as i64, 2200);
    ensure(mixed_batch == 0 as i64 || mixed_batch == 1 as i64, 2201);
    ensure(n_pos_per_embd > 0 as i64, 2202);
    var n_embd_rows: i64 = 0;

    if mixed_batch != 0 as i64 {
        var i: i64 = 0;

        while i < n_tokens {
            let ix: i64 = ld_i32(idxs, i);
            ensure(ix >= 0 as i64, 2203);
            n_embd_rows = n_embd_rows + ld_i32(is_embd, ix);
            i = i + 1 as i64;
        }
    }
    var mixed: i64 = 0;

    if mixed_batch != 0 as i64 && n_embd_rows > 0 as i64 && n_embd_rows < n_tokens {
        mixed = 1 as i64;
    }
    var use_token: i64 = 0;

    if has_token != 0 as i64 {
        if ! (mixed_batch != 0 as i64 && n_embd_rows == n_tokens) {
            use_token = 1 as i64;
        }
    }
    var use_embd: i64 = 0;

    if has_embd != 0 as i64 {
        if ! (mixed_batch != 0 as i64 && n_embd_rows == 0 as i64) {
            use_embd = 1 as i64;
        }
    }
    var n_embd_all: i64 = 0;

    if use_embd != 0 as i64 {
        n_embd_all = n_tokens * n_embd;
    }
    var n_state_all: i64 = 0;

    if has_state != 0 as i64 {
        n_state_all = n_tokens * n_embd_state;
    }
    let n_pos_all: i64 = n_tokens * n_pos_per_embd;
    st_i64(out, 0 as i64, mixed);
    st_i64(out, 1 as i64, use_token);
    st_i64(out, 2 as i64, use_embd);
    st_i64(out, 3 as i64, n_embd_all);
    st_i64(out, 4 as i64, n_state_all);
    st_i64(out, 5 as i64, n_pos_all);
    n_embd_rows
}

fn main() {
    classify(
        1 as i64,
        0 as i64,
        0 as u64,
        0 as u64,
        1 as i64,
        0 as i64,
        0 as i64,
        1 as i64,
        1 as i64,
        1 as i64,
        0 as u64);
    classify(
        2 as i64,
        1 as i64,
        8 as u64,
        16 as u64,
        1 as i64,
        1 as i64,
        1 as i64,
        4 as i64,
        2 as i64,
        1 as i64,
        24 as u64);
}

