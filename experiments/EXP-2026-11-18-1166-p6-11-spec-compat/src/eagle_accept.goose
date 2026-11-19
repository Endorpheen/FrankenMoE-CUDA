// EAGLE3 / MTP accept: pick the deferred boundary row and rebase its position.
// Guards mirror the native order: sequence range first, then a non-positive row count.
// out[0] = row index i_g, out[1] = rebased position, and the return value is the
// number of outputs written (0 when the native code returns early).
fn g_eagle_accept(seq_id: i64, n_seq: i64, n_rows: i64, n_accepted: i64,
                  verify_pos_first: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 193 {
        return -1;
    }
    if seq_id < 0 || seq_id >= n_seq {
        return 0;
    }
    if n_rows <= 0 {
        return 0;
    }
    var i_g = n_accepted;
    if i_g > n_rows - 1 {
        i_g = n_rows - 1;
    }
    if i_g < 0 {
        i_g = 0;
    }
    out[0] = i_g;
    out[1] = verify_pos_first + i_g;
    return 2;
}

fn main() {
    let o: i64[2] = [0, 0];
    let r = g_eagle_accept(0, 1, 4, 2, 100, o, 193);
    if r < 0 {
        return;
    }
}
