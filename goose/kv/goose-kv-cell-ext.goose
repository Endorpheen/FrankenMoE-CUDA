// llama_kv_cell_ext construction in Goose (APX: P5).
//
// From llama-kv-cache.cpp:1155-1179 inside apply_ubatch:
//
//   if (ubatch.is_pos_2d() || ubatch.token || hparams.ple_n_heads > 0) {
//       llama_kv_cell_ext ext;                       // defaults: x=0, y=0, tok=LLAMA_TOKEN_NULL(-1)
//       if (ubatch.is_pos_2d()) { ext.x = pos[i + n_tokens*2]; ext.y = pos[i + n_tokens]; }
//       const bool is_embd = !ubatch.token || (ubatch.is_mixed() && ubatch.type[i]);
//       if (!is_embd)                       ext.tok = ubatch.token[i];
//       else if (hparams.ple_n_heads > 0)   ext.tok = ple_image_token_id != 0 ? ple_image_token_id
//                                                                            : ple_eos_token_id;
//       cells.ext_set(idx, ext);
//   }
//
// Three things a rewrite must not lose, and the gate checks each:
//   * when the batch carries only token ids (no 2D positions, no PLE), x and y stay EMPTY, not copied
//     from the position array;
//   * is_embd is true when there is no token array at all, and also for an individual row of a mixed
//     batch whose type flags that row as embedding;
//   * tok defaults to LLAMA_TOKEN_NULL (-1) and only the two branches above can change it, so an
//     embedding row without PLE leaves it -1 rather than zero.
extern "fg228e_fail" fn fail(code: i64);
extern "fg228e_st_i32" fn st_i32(base: u64, idx: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// build_ext writes [x, y, tok] to out and returns 1 when the cell should receive an ext, 0 otherwise.

fn build_ext(
    has_ext: i64,
    pos_2d: i64,
    px: i64,
    py: i64,
    has_token: i64,
    tok_i: i64,
    is_mixed: i64,
    type_i: i64,
    ple_n_heads: i64,
    ple_image_id: i64,
    ple_eos_id: i64,
    out: u64) -> i64 {
    ensure(has_ext == 0 as i64 || has_ext == 1 as i64, 700);
    ensure(pos_2d == 0 as i64 || pos_2d == 1 as i64, 701);
    ensure(has_token == 0 as i64 || has_token == 1 as i64, 702);
    ensure(is_mixed == 0 as i64 || is_mixed == 1 as i64, 703);
    ensure(ple_n_heads >= 0, 704);

    if has_ext == 0 as i64 {
        return 0 as i64;
    }
    var x: i64 = 0;
    var y: i64 = 0;
    var tok: i64 = 0 as i64 - 1 as i64;

    if pos_2d != 0 as i64 {
        x = px;
        y = py;
    }
    var is_embd: i64 = 0;

    if has_token == 0 as i64 {
        is_embd = 1 as i64;
    }

    if is_mixed != 0 as i64 && type_i != 0 as i64 {
        is_embd = 1 as i64;
    }

    if is_embd == 0 as i64 {
        tok = tok_i;
    } else {
        if ple_n_heads > 0 as i64 {
            if ple_image_id != 0 as i64 {
                tok = ple_image_id;
            } else {
                tok = ple_eos_id;
            }
        }
    }
    st_i32(out, 0 as i64, x);
    st_i32(out, 1 as i64, y);
    st_i32(out, 2 as i64, tok);
    return 1 as i64;
}

fn main() {
    build_ext(
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as u64);
    build_ext(
        1 as i64,
        1 as i64,
        7 as i64,
        3 as i64,
        1 as i64,
        42 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        16 as u64);
    build_ext(
        1 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        4 as i64,
        99 as i64,
        1 as i64,
        32 as u64);
}

