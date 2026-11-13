// llama_batch_allocr::split_equal -- the expansion step in Goose (APX: P6-07).
//
// Native (llama-batch.cpp:683-713), after the sequence sets are chosen:
//
//   while (true) {
//       can_expand = all s: cur_idx[s] < seq_set_map[set[s]].size()
//       if (!can_expand) break;
//       for s: idx = seq_set_map[set[s]][cur_idx[s]]; push idx; used[idx]=true; ++n_used; ++cur_idx[s];
//       if ((idxs_per_seq[0].size() + 1)*n_seqs > n_ubatch) break;
//   }
//
// Two things the gate must hold:
//   * the loop adds exactly ONE token per sequence set per iteration, all-or-nothing -- it never
//     advances one set and stops early on another;
//   * the size test runs AFTER the push and uses size()+1, so the ubatch can exceed n_ubatch by up to
//     n_seqs-1 tokens before the break. Off-by-one here changes how batches are formed, which is
//     invisible to token-level correctness but changes order and performance.
//
// Four explicit cursors rather than an array: the logic is per-set and identical, and four sets cover
// every branch while keeping the module free of aggregate locals.
extern "fg237s_fail" fn fail(code: i64);
extern "fg237s_ld_i32" fn ld_i32(base: u64, idx: i64) -> i64;
extern "fg237s_st_i32" fn st_i32(base: u64, idx: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// plan_expand returns the number of iterations (tokens added per set) and writes final cursors.

fn plan_expand(n_seqs: i64, n_ubatch: i64, set_len: u64, cursor: u64, out_cursor: u64) -> i64 {
    ensure(n_seqs >= 1 as i64, 1700);
    ensure(n_seqs <= 4 as i64, 1703);
    ensure(n_ubatch >= 0 as i64, 1701);
    let l0: i64 = ld_i32(set_len, 0 as i64);
    var c0: i64 = ld_i32(cursor, 0 as i64);
    let l1: i64 = ld_i32(set_len, 1 as i64);
    var c1: i64 = ld_i32(cursor, 1 as i64);
    let l2: i64 = ld_i32(set_len, 2 as i64);
    var c2: i64 = ld_i32(cursor, 2 as i64);
    let l3: i64 = ld_i32(set_len, 3 as i64);
    var c3: i64 = ld_i32(cursor, 3 as i64);
    var iters: i64 = 0;
    var go: i64 = 1;

    while go != 0 as i64 {
        var can: i64 = 1;

        if c0 >= l0 {
            can = 0 as i64;
        }

        if n_seqs > 1 as i64 && c1 >= l1 {
            can = 0 as i64;
        }

        if n_seqs > 2 as i64 && c2 >= l2 {
            can = 0 as i64;
        }

        if n_seqs > 3 as i64 && c3 >= l3 {
            can = 0 as i64;
        }

        if can == 0 as i64 {
            go = 0 as i64;
        } else {
            c0 = c0 + 1 as i64;

            if n_seqs > 1 as i64 {
                c1 = c1 + 1 as i64;
            }

            if n_seqs > 2 as i64 {
                c2 = c2 + 1 as i64;
            }

            if n_seqs > 3 as i64 {
                c3 = c3 + 1 as i64;
            }
            iters = iters + 1 as i64;

            if ( (iters + 1 as i64) * n_seqs) > n_ubatch {
                go = 0 as i64;
            }
        }
    }
    st_i32(out_cursor, 0 as i64, c0);

    if n_seqs > 1 as i64 {
        st_i32(out_cursor, 1 as i64, c1);
    }

    if n_seqs > 2 as i64 {
        st_i32(out_cursor, 2 as i64, c2);
    }

    if n_seqs > 3 as i64 {
        st_i32(out_cursor, 3 as i64, c3);
    }
    iters
}

fn main() {
    plan_expand(1 as i64, 10 as i64, 0 as u64, 8 as u64, 16 as u64);
    plan_expand(2 as i64, 8 as i64, 24 as u64, 32 as u64, 40 as u64);
    plan_expand(4 as i64, 9 as i64, 48 as u64, 56 as u64, 64 as u64);
}

