// Recurrent-memory sequence prune in Goose (APX: P5-04/P5-07).
//
// From llama-memory-recurrent.cpp:302-332 llama_memory_recurrent::seq_keep. This is the path the
// checkpoint machinery uses when a context keeps one sequence and drops the others: every state
// cell that does not carry the sequence is released, and head is moved to the first freed slot so
// the next search starts there instead of scanning from the old head.
//
// This is NOT the KV cell class (EXP199/204/205/209 own that). Recurrent state is a separate
// subsystem with its own cells, its own tail/head bookkeeping and a `src` field that records which
// source cell a copy came from. Getting `head` wrong is silent: the cache keeps working and only
// degrades as slots are missed.
//
// Arena, int64 slots per cell i, sized n:
//   pos[i]   -1 = free, else the position
//   src[i]   -1 = not a copy, else the source cell index
//   tail[i]  -1 = not a per-sequence tail, else the tail cell index
//   seq[i*w + k]  w words of 64 sequence bits
// plus two scalars passed by pointer: head, used.
extern "fg210k_fail" fn fail(code: i64);
extern "fg210k_ld_i64" fn ld_i64(base: u64, byte: i64) -> i64;
extern "fg210k_st_i64" fn st_i64(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn pos_at(a: u64, i: i64) -> i64 {
    return ld_i64(a, i * 8 as i64);
}

fn tail_at(a: u64, n: i64, i: i64) -> i64 {
    return ld_i64(a, (n + i) * 8 as i64);
}

fn src_at(a: u64, n: i64, i: i64) -> i64 {
    return ld_i64(a, (2 as i64 * n + i) * 8 as i64);
}

fn seq_base(n: i64) -> i64 {
    return 3 as i64 * n;
}

fn seqword(a: u64, n: i64, i: i64, w: i64, k: i64) -> i64 {
    return ld_i64(a, (seq_base(n) + (i * w + k)) * 8 as i64);
}

fn has_seq(a: u64, n: i64, i: i64, s: i64, w: i64) -> i64 {
    let k: i64 = s / 64 as i64;

    if k >= w {
        return 0 as i64;
    }
    let word: i64 = seqword(a, n, i, w, k);

    if word & ( (1 as i64) << (s % 64 as i64)) != 0 as i64 {
        return 1 as i64;
    }
    return 0 as i64;
}
// seq_keep: drop every cell that does not carry `s`, keep only `s` in the ones that do, and move
// head to the first freed slot when it is below the current head. Returns the new head; `used` is
// updated in place through usedp.

fn seq_keep(a: u64, n: i64, w: i64, s: i64, head: i64, usedp: u64) -> i64 {
    ensure(n >= 0 && w > 0 && s >= 0, 700);
    ensure(head >= 0, 701);
    var new_head: i64 = n;
    // "size" in native, meaning no slot freed
    var used: i64 = ld_i64(usedp, 0 as i64);
    var i: i64 = 0;

    while i < n {
        if i != s {
            st_i64(a, (n + i) * 8 as i64, 0 as i64 - 1 as i64);
            // tail = -1
        }

        if has_seq(a, n, i, s, w) == 0 as i64 {
            if pos_at(a, i) >= 0 as i64 {
                used -= 1 as i64;
            }
            st_i64(a, i * 8 as i64, 0 as i64 - 1 as i64);
            // pos = -1
            st_i64(a, (2 as i64 * n + i) * 8 as i64, 0 as i64 - 1 as i64);
            // src = -1
            var k: i64 = 0;

            while k < w {
                st_i64(a, (seq_base(n) + (i * w + k)) * 8 as i64, 0 as i64);
                // seq.clear()
                k += 1;
            }

            if new_head == n {
                new_head = i;
            }
        } else {
            var k: i64 = 0;

            while k < w {
                st_i64(a, (seq_base(n) + (i * w + k)) * 8 as i64, 0 as i64);
                k += 1;
            }
            // seq.clear(); seq.insert(s)
            let kk: i64 = s / 64 as i64;

            if kk < w {
                st_i64(a, (seq_base(n) + (i * w + kk)) * 8 as i64, (1 as i64) << (s % 64 as i64));
            }
        }
        i += 1;
    }
    st_i64(usedp, 0 as i64, used);

    if new_head != n {
        if new_head < head {
            return new_head;
        }
    }
    return head;
}

fn main() {
    seq_keep(0 as u64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as u64);
    seq_keep(8 as u64, 1 as i64, 4 as i64, 1 as i64, 2 as i64, 72 as u64);
}

