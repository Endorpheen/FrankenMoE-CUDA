// Apex KV slot search in Goose: llama_hparams::is_masked_swa and llama_kv_cache::find_slot.
//
// find_slot walks the ring of cells from the per-stream head, accepts a cell when it is empty or
// when it is held by exactly one sequence and that sequence's highest position makes the cell fall
// outside the sliding window, and reports the indices it found. `cont` asks for a continuous run.
//
// The cell containers stay on the host, as in EXP199/EXP289: pos[], used[], and the sequence
// words are storage. The one behavioural substitution is seq_pos_max, which native reads from the
// cached std::set; here it is computed by scanning the cells. The value is identical by the
// invariant that seq_pos holds exactly the cells carrying that sequence's bit.
//
// Packet layout, int64 slots:
//   k=0  0 op, 1 pos, 2 used, 3 seqw, 4 n, 5 W, 6 head, 7 n_swa, 8 swa_type,
//        9 n_tokens, 10 n_seqs_unq, 11 seq_id_unq, 12 n_seq_id, 13 seq_id, 14 seq_to_stream,
//        15 n_stream
//   k=1  0 cont, 1 idxs_out, 2 counts_out
//   k=3  0 ok, 1 s0, 2 s1, 3.. stream ids, then counts live in counts_out
extern "fg290k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg290k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg290k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg290k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg290k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn bit_of(s: i64) -> i64 {
    var b: i64 = 1 as i64;
    var k: i64 = 0;

    while k < s % 64 as i64 {
        b = b * 2 as i64;
        k = k + 1 as i64;
    }
    b
}

fn seq_has(seqw: u64, i: i64, w: i64, s: i64) -> bool {
    return (ld(seqw, (i * w + s / 64 as i64) * (8 as i64)) & bit_of(s)) != (0 as i64);
}

fn seq_count(seqw: u64, i: i64, w: i64, seq_n: i64) -> i64 {
    var c: i64 = 0;
    var s: i64 = 0;

    while s < seq_n {
        if seq_has(seqw, i, w, s) {
            c = c + 1 as i64;
        }
        s = s + 1 as i64;
    }
    c
}

fn seq_get(seqw: u64, i: i64, w: i64, seq_n: i64) -> i64 {
    var s: i64 = 0;

    while s < seq_n {
        if seq_has(seqw, i, w, s) {
            return s;
        }
        s = s + 1 as i64;
    }
    return (0 as i64) - 1 as i64;
}
// The highest position among the cells that carry sequence s, or -1.

fn seq_pos_max(pos: u64, seqw: u64, n: i64, w: i64, seq_n: i64, s: i64) -> i64 {
    var best: i64 = (0 as i64) - 1 as i64;
    var i: i64 = 0;

    while i < n {
        let q: i64 = ld(pos, i * (8 as i64));

        if q != (0 as i64) - 1 as i64 {
            if seq_has(seqw, i, w, s) {
                if q > best {
                    best = q;
                }
            }
        }
        i = i + 1 as i64;
    }
    best
}

fn is_masked_swa(n_swa: i64, swa_type: i64, p0: i64, p1: i64) -> bool {
    ensure(p0 >= 0 as i64 && p1 >= 0 as i64, 10);

    if swa_type == (0 as i64) {
        return false;
    }

    if swa_type == (1 as i64) {
        if p1 - p0 >= n_swa {
            return true;
        }
        return false;
    }

    if swa_type == (2 as i64) {
        ensure(n_swa > (0 as i64), 11);
        let c: i64 = (p1 / n_swa) * n_swa;

        if p0 < c {
            return true;
        }
        return false;
    }

    if swa_type == (3 as i64) {
        let h: i64 = n_swa / (2 as i64);
        let d: i64 = p1 - p0;

        if d < (0 as i64) - h {
            return true;
        }

        if d > h {
            return true;
        }
        return false;
    }
    return false;
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let pos: u64 = get(packet, 0, 1);
    let used: u64 = get(packet, 0, 2);
    let seqw: u64 = get(packet, 0, 3);
    let n: i64 = get(packet, 0, 4) as i64;
    let w: i64 = get(packet, 0, 5) as i64;
    let head0: i64 = get(packet, 0, 6) as i64;
    let n_swa: i64 = get(packet, 0, 7) as i64;
    let swa_type: i64 = get(packet, 0, 8) as i64;
    let n_tokens0: i64 = get(packet, 0, 9) as i64;
    let n_seqs_unq: i64 = get(packet, 0, 10) as i64;
    let seq_id_unq: u64 = get(packet, 0, 11);
    let n_seq_id: u64 = get(packet, 0, 12);
    let seq_id: u64 = get(packet, 0, 13);
    let seq_to_stream: u64 = get(packet, 0, 14);
    let n_stream: i64 = get(packet, 0, 15) as i64;
    let cont: bool = get(packet, 1, 0) != (0 as u64);
    let idxs_out: u64 = get(packet, 1, 1);
    let counts_out: u64 = get(packet, 1, 2);

    if op == (0 as i64) {
        let r: bool = is_masked_swa(n_swa, swa_type, ld(pos, 0 as i64), ld(pos, 8 as i64));
        put(packet, 3, 0, (1 as i64) as u64);
        var v: i64 = 0;

        if r {
            v = 1 as i64;
        }
        put(packet, 3, 1, v as u64);
        return true;
    }
    ensure(n > 0 as i64, 20);
    ensure(w > (0 as i64), 21);
    ensure(n_tokens0 > 0 as i64, 22);
    ensure(n_seqs_unq > (0 as i64), 23);
    ensure(head0 >= 0 as i64, 24);
    var n_tokens: i64 = n_tokens0;
    var n_seqs: i64 = 1;

    if n_stream > (1 as i64) {
        ensure(n_tokens % n_seqs_unq == (0 as i64), 25);
        n_seqs = n_seqs_unq;
        n_tokens = n_tokens / n_seqs_unq;
    }
    var s0: i64 = 512 as i64;
    var s1: i64 = 0;
    var ok: bool = true;

    if n_tokens > n {
        ok = false;
    }
    var s: i64 = 0;

    while s < n_seqs && ok {
        let sid: i64 = ld(seq_id_unq, s * (8 as i64));
        ensure(sid >= 0 as i64, 27);

        if n_stream > (1 as i64) {
            ensure(ld(n_seq_id, (s * n_tokens) * (8 as i64)) == (1 as i64), 28);
            ensure(ld(seq_id, ( (s * n_tokens) * 512 as i64 + 0 as i64) * (8 as i64)) == sid, 29);
        }
        let stream: i64 = ld(seq_to_stream, sid * (8 as i64));
        ensure(stream >= 0 as i64 && stream < n_stream as i64, 30);

        if stream < s0 {
            s0 = stream;
        }

        if stream > s1 {
            s1 = stream;
        }
        put(packet, 3, (4 as i64 + s) as i64, stream as u64);
        var head_cur: i64 = head0;
        var used_cnt: i64 = 0;
        var uc: i64 = 0;

        while uc < n {
            if ld(used, uc * (8 as i64)) != (0 as i64) {
                used_cnt = used_cnt + 1 as i64;
            }
            uc = uc + 1 as i64;
        }

        if head_cur > used_cnt + 2 as i64 * n_tokens {
            head_cur = 0;
        }
        var n_tested: i64 = 0;
        var n_test: i64 = 1;

        if cont {
            n_test = n_tokens;
        }
        var cnt: i64 = 0;
        var done: bool = false;
        var failed: bool = false;

        while !done && !failed {
            if head_cur + n_test > n {
                n_tested = n_tested + (n - head_cur);
                head_cur = 0;
            } else {
                var i: i64 = 0;
                var brk: bool = false;

                while i < n_test && !brk {
                    let idx: i64 = head_cur;
                    head_cur = head_cur + 1 as i64;
                    n_tested = n_tested + 1 as i64;
                    let pc: i64 = ld(pos, idx * (8 as i64));
                    var can_use: bool = pc == (0 as i64) - 1 as i64;

                    if !can_use {
                        if seq_count(seqw, idx, w, 512 as i64) == (1 as i64) {
                            let sid_cell: i64 = seq_get(seqw, idx, w, 512 as i64);
                            ensure(sid_cell >= 0 as i64, 31);

                            if is_masked_swa(
                                n_swa,
                                swa_type,
                                pc,
                                seq_pos_max(pos, seqw, n, w, 512 as i64, sid_cell) + 1 as i64) {
                                can_use = true;
                            }
                        }
                    }

                    if can_use {
                        st(idxs_out, (s * n_tokens + cnt) * (8 as i64), idx);
                        cnt = cnt + 1 as i64;
                    } else {
                        if cont {
                            brk = true;
                        }
                    }
                    i = i + 1 as i64;
                }

                if cnt == n_tokens {
                    done = true;
                }

                if !done {
                    if cont {
                        cnt = 0;
                    }

                    if n_tested >= n {
                        failed = true;
                    }
                }
            }
        }

        if cnt < n_tokens {
            failed = true;
        }
        st(counts_out, s * (8 as i64), cnt);

        if failed {
            ok = false;
        }

        if !ok {
            s = n_seqs;
        } else {
            s = s + 1 as i64;
        }
    }

    if !ok {
        s0 = 0 as i64;
        s1 = 0 as i64;
    }
    put(packet, 3, 0, (1 as i64) as u64);
    var flag: i64 = 0;

    if ok {
        flag = 1 as i64;
    }
    put(packet, 3, 1, flag as u64);
    put(packet, 3, 2, s0 as u64);
    put(packet, 3, 3, s1 as u64);
    return true;
}

fn main() {
    run(0);
}

