// Apex KV previous-token lookup in Goose (llama_kv_cache::get_prev_tokens).
//
// For every token of the ubatch and every lookback depth d in 1..n, the answer is the token stored in
// the nearest cell at or before a target position, for the token's first sequence.
//
// Two rules decide the target position:
//
//   by position — a token batch: the target is `pos[i] - d`; a negative target means "no answer" and
//   the entry stays null.
//
//   by order — an embedding or mixed batch: one image can repeat a single position for many tokens, so
//   positions no longer order the tokens. The predecessors are then taken from the ubatch order of the
//   same sequence: a depth that reaches inside the ubatch uses that token's own position, and a depth
//   that reaches before the chunk extends backwards from the first token of the sequence.
//
// The nearest cell is the **last** cell at the target position, not the first: the per-sequence index
// is ordered by (position, cell index), so at equal positions the higher cell index wins. This is the
// same trap as the token-order lookup in the dsv4 compressor and is why equal positions appear in the
// gate.
//
// Packet:
//   k=0  0 n_tokens, 1 n, 2 tok_present, 3 is_mixed, 4 ub_pos, 5 ub_seq0, 6 n_cells, 7 cpos,
//        8 ctok, 9 cmask
//   k=1  0 out
//   k=2  0 count ptr
//   k=3  0 done, 1 ret
//
// Records, three slots: 0 answer written (a = flat index, b = token, -1 for null).
extern "fg334k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg334k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg334k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg334k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg334k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(arr: u64, i: i64) -> i64 {
    return ld(arr, i * (8 as i64));
}

fn setat(arr: u64, i: i64, v: i64) {
    stw(arr, i * (8 as i64), v);
}

fn emit(out: u64, cntp: u64, kind: i64, a: i64, b: i64) {
    let n: i64 = at(cntp, 0 as i64);
    setat(out, n * (3 as i64), kind);
    setat(out, n * (3 as i64) + 1 as i64, a);
    setat(out, n * (3 as i64) + 2 as i64, b);
    setat(cntp, 0 as i64, n + (1 as i64));
}
// index of the k-th occurrence (0-based) of seq in the ubatch, or -1 when there is none

fn nth_of(ub_seq0: u64, n_tokens: i64, seq: i64, k: i64) -> i64 {
    var seen: i64 = 0 as i64;
    var i: i64 = 0;

    while i < n_tokens {
        if at(ub_seq0, i) == seq {
            if seen == k {
                return i;
            }
            seen = seen + 1 as i64;
        }
        i = i + 1 as i64;
    }
    return (0 as i64) - 1 as i64;
}

fn count_before(ub_seq0: u64, n_tokens: i64, seq: i64, upto: i64) -> i64 {
    var seen: i64 = 0 as i64;
    var i: i64 = 0;

    while i < upto {
        if at(ub_seq0, i) == seq {
            seen = seen + 1 as i64;
        }
        i = i + 1 as i64;
    }
    return seen;
}
// last cell at or before p holding seq: highest (position, cell index)

fn tok_le(n_cells: i64, cpos: u64, ctok: u64, cmask: u64, seq: i64, p: i64) -> i64 {
    var best_p: i64 = (0 as i64) - 1 as i64;
    var best_i: i64 = (0 as i64) - 1 as i64;
    var i: i64 = 0;

    while i < n_cells {
        let cp: i64 = at(cpos, i);

        if cp >= (0 as i64) {
            if ( (at(cmask, i) & ( (1 as i64) << seq)) != (0 as i64)) {
                if cp <= p {
                    if cp > best_p || (cp == best_p && i > best_i) {
                        best_p = cp;
                        best_i = i;
                    }
                }
            }
        }
        i = i + 1 as i64;
    }

    if best_i < (0 as i64) {
        return (0 as i64) - 1 as i64;
    }
    return at(ctok, best_i);
}

fn run(packet: i64) -> bool {
    let n_tokens: i64 = get(packet, 0, 0) as i64;
    let n: i64 = get(packet, 0, 1) as i64;
    let tok_present: i64 = get(packet, 0, 2) as i64;
    let is_mixed: i64 = get(packet, 0, 3) as i64;
    let ub_pos: u64 = get(packet, 0, 4);
    let ub_seq0: u64 = get(packet, 0, 5);
    let n_cells: i64 = get(packet, 0, 6) as i64;
    let cpos: u64 = get(packet, 0, 7);
    let ctok: u64 = get(packet, 0, 8);
    let cmask: u64 = get(packet, 0, 9);
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 2, 0);
    ensure(n_tokens >= (0 as i64) && n >= (0 as i64), 60);
    ensure(
        ub_pos != (0 as u64) && ub_seq0 != (0 as u64) && cpos != (0 as u64) && ctok != (0 as u64) && cmask != (0 as u64),
        61);
    var by_order: i64 = 0 as i64;

    if tok_present == (0 as i64) {
        by_order = 1 as i64;
    } else {
        if is_mixed != (0 as i64) {
            by_order = 1 as i64;
        }
    }
    var i: i64 = 0;

    while i < n_tokens {
        let seq: i64 = at(ub_seq0, i);
        var j: i64 = 0;

        while j < n {
            let d: i64 = n - j;
            var p: i64 = (0 as i64) - 1 as i64;

            if by_order != (0 as i64) {
                let cb: i64 = count_before(ub_seq0, n_tokens, seq, i);
                let k: i64 = cb - d;

                if k >= (0 as i64) {
                    let vk: i64 = nth_of(ub_seq0, n_tokens, seq, k);
                    ensure(vk >= (0 as i64), 62);
                    p = at(ub_pos, vk);
                } else {
                    let v0: i64 = nth_of(ub_seq0, n_tokens, seq, 0 as i64);
                    ensure(v0 >= (0 as i64), 63);
                    p = at(ub_pos, v0) + k;
                }
            } else {
                p = at(ub_pos, i) - d;
            }
            var v: i64 = (0 as i64) - 1 as i64;

            if p >= (0 as i64) {
                v = tok_le(n_cells, cpos, ctok, cmask, seq, p);
            }
            emit(out, cntp, 0 as i64, i * n + j, v);
            j = j + 1 as i64;
        }
        i = i + 1 as i64;
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, 0 as i64 as u64);
    return true;
}

fn main() {
    run(0);
}

