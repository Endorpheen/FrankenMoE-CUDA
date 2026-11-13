// P5-07/P6: context-checkpoint eviction policy, in Goose.
//
// Native reference: tools/server/server-context.cpp, create_checkpoint(). Three decisions, all over the checkpoint
// list of one slot, represented here as parallel arrays (n_tokens, id_task):
//
//   1. near-eviction, run only while the list is full: walk from the oldest, and erase an entry when a previous
//      surviving entry exists and the entry is within min_step tokens of it, unless the entry belongs to the task
//      that is creating the new checkpoint. The guard uses the live list size, which shrinks with every erase.
//   2. make-room: while the list still holds at least n_ctx_checkpoints entries, drop the oldest.
//   3. supersede: drop entries whose n_tokens equals the token count of the checkpoint about to be stored.
//
// All three return a mask of entries to erase (bit i = entry i), so the caller keeps ownership of the container.
extern "fg268p_fail" fn fail(c: i64);
extern "fg268p_ld_i64" fn ld_i64(base: u64, byte: i64) -> i64;
extern "fg268p_st_i64" fn st_i64(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(a: u64, i: i64) -> i64 {
    return ld_i64(a, i * 8 as i64);
}

fn setbit(m: u64, i: i64) {
    st_i64(m, (i / 64 as i64) * 8 as i64, at(m, i / 64 as i64) | ( (1 as i64) << (i % (64 as i64))));
}
// returns the number of entries erased, writes the mask

fn near_evict(tokens: u64, task: u64, count: i64, id_task: i64, min_step: i64, limit: i64, out_mask: u64) -> i64 {
    ensure(count >= 0 as i64 && limit > 0 as i64 && min_step >= 0 as i64, 10);
    var size: i64 = count;
    var last: i64 = -1 as i64;
    var erased: i64 = 0 as i64;
    var i: i64 = 0 as i64;

    while i < count {
        if size + 1 < limit {
            i = count;
        } else {
            let t: i64 = at(task, i);
            let n: i64 = at(tokens, i);

            if t != id_task && last >= 0 as i64 && n <= last + min_step {
                setbit(out_mask, i);
                size = size - 1 as i64;
                erased = erased + 1 as i64;
            } else {
                last = n;
            }
            i = i + 1 as i64;
        }
    }
    erased
}
// entries the caller must drop from the front before storing one more

fn make_room(count: i64, limit: i64) -> i64 {
    ensure(count >= 0 as i64 && limit > 0 as i64, 11);
    var n: i64 = 0 as i64;
    var size: i64 = count;

    while size >= limit {
        n = n + 1 as i64;
        size = size - 1 as i64;
    }
    n
}
// entries with the same token count as the new checkpoint

fn supersede(tokens: u64, count: i64, n_tokens_new: i64, out_mask: u64) -> i64 {
    ensure(count >= 0 as i64, 12);
    var n: i64 = 0 as i64;
    var i: i64 = 0 as i64;

    while i < count {
        if at(tokens, i) == n_tokens_new {
            setbit(out_mask, i);
            n = n + 1 as i64;
        }
        i = i + 1 as i64;
    }
    n
}

fn main() {
    near_evict(0 as u64, 0 as u64, 0 as i64, 0 as i64, 0 as i64, 1 as i64, 0 as u64);
    make_room(0 as i64, 1 as i64);
    supersede(0 as u64, 0 as i64, 0 as i64, 0 as u64);
}

