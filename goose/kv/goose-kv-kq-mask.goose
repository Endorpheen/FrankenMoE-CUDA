// Apex KV cache attention mask in Goose (set_input_kq_mask, the float path).
//
// For every token the mask row holds 0 for a cell it may attend and -inf for one it may not. A cell
// is dropped when it is empty, when it does not carry the token's sequence, when causal masking puts
// it after the token, or when the sliding window excludes it. Tokens of the same sequence reuse the
// previous row: only the cells whose position is within `n_swa + 32` of the sequence's first batch
// position can change, so the row is copied and exactly those cells are revisited.
//
// Not transferred here, stated rather than implied: ALiBi values, the 2-D/M-RoPE causal comparison,
// the half-precision path, and the non-causal SWA-full span.
//
// Packet, int64 slots:
//   k=0  0 unused, 1 n_kv, 2 n_stream, 3 n_tps, 4 n_tokens, 5 n_seq_max, 6 n_swa, 7 swa_type,
//        8 causal, 9 swa_on, 10 seq_to_stream, 11 ub_seq, 12 width, 13 pos
//   k=1  0 cells_pos (per stream, stride n_kv), 1 cells_seqw, 2 W
//   k=2  0 mask out, 1 srct, 2 counts, 3 idxs, 4 seq_pos_min
//   k=3  0 done, 1 ret
extern "fg316k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg316k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg316k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg316k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg316k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(arr: u64, i: i64) -> i64 {
    return ld(arr, i * (8 as i64));
}

fn setat(arr: u64, i: i64, v: i64) {
    st(arr, i * (8 as i64), v);
}

fn neg_inf() -> i64 {
    return (0 as i64) - 16777216 as i64;
}

fn posmax() -> i64 {
    return 2147483647 as i64;
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

fn is_masked_swa(n_swa: i64, t: i64, p0: i64, p1: i64) -> bool {
    if t == (0 as i64) {
        return false;
    }

    if t == (1 as i64) {
        if p1 - p0 >= n_swa {
            return true;
        }
        return false;
    }

    if t == (2 as i64) {
        ensure(n_swa > (0 as i64), 60);
        let c: i64 = (p1 / n_swa) * n_swa;

        if p0 < c {
            return true;
        }
        return false;
    }

    if t == (3 as i64) {
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
    let n_kv: i64 = get(packet, 0, 1) as i64;
    let n_stream: i64 = get(packet, 0, 2) as i64;
    let n_tps: i64 = get(packet, 0, 3) as i64;
    let n_tokens: i64 = get(packet, 0, 4) as i64;
    let n_seq_max: i64 = get(packet, 0, 5) as i64;
    let n_swa: i64 = get(packet, 0, 6) as i64;
    let swa_type: i64 = get(packet, 0, 7) as i64;
    let causal: bool = get(packet, 0, 8) != (0 as u64);
    let swa_on: bool = get(packet, 0, 9) != (0 as u64);
    let s2s: u64 = get(packet, 0, 10);
    let ub_seq: u64 = get(packet, 0, 11);
    let width: i64 = get(packet, 0, 12) as i64;
    let posarr: u64 = get(packet, 0, 13);
    let cpos: u64 = get(packet, 1, 0);
    let cseqw: u64 = get(packet, 1, 1);
    let W: i64 = get(packet, 1, 2) as i64;
    let mask: u64 = get(packet, 2, 0);
    let srct: u64 = get(packet, 2, 1);
    let counts: u64 = get(packet, 2, 2);
    let idxs: u64 = get(packet, 2, 3);
    let smin: u64 = get(packet, 2, 4);
    ensure(n_kv >= (0 as i64) && n_stream > (0 as i64) && n_tps > (0 as i64), 61);
    ensure(width >= (1 as i64) && W > (0 as i64), 62);
    ensure(n_tokens == n_stream * n_tps, 63);
    // per-sequence state for this call
    var s0: i64 = 0;

    while s0 < n_seq_max {
        setat(smin, s0, posmax());
        setat(srct, s0, (0 as i64) - 1 as i64);
        setat(counts, s0, 0 as i64);
        s0 = s0 + 1 as i64;
    }
    var i0: i64 = 0;

    while i0 < n_tokens {
        let sid0: i64 = at(ub_seq, i0 * width);
        let p0v: i64 = at(posarr, i0);

        if p0v < at(smin, sid0) {
            setat(smin, sid0, p0v);
        }
        i0 = i0 + 1 as i64;
    }
    var s: i64 = 0;

    while s < n_stream {
        var ii: i64 = 0;

        while ii < n_tps {
            let it: i64 = s * n_tps + ii;
            let sid: i64 = at(ub_seq, it * width);
            let stream: i64 = at(s2s, sid);
            let p1: i64 = at(posarr, it);
            let idst: i64 = n_kv * it;
            var prev: bool = false;

            if at(srct, sid) != ( (0 as i64) - 1 as i64) {
                let src: i64 = at(srct, sid);
                var q: i64 = 0;

                while q < n_kv {
                    setat(mask, idst + q, at(mask, n_kv * src + q));
                    q = q + 1 as i64;
                }
                prev = true;
            } else {
                setat(srct, sid, it);
                setat(counts, sid, 0 as i64);
            }
            var nidx: i64 = at(counts, sid);
            let ibase: i64 = sid * n_kv;
            var jj: i64 = 0;

            while jj < n_kv {
                var j: i64 = jj;

                if prev {
                    if jj >= nidx {
                        jj = n_kv;
                    } else {
                        j = at(idxs, ibase + jj);
                    }
                }

                if jj < n_kv {
                    let pcell: i64 = at(cpos, stream * n_kv + j);

                    if pcell == ( (0 as i64) - 1 as i64) {
                        setat(mask, idst + j, neg_inf());
                    } else {
                        let wi: i64 = sid / 64 as i64;
                        let has: bool = (at(cseqw, (stream * n_kv + j) * W + wi) & bit_of(sid)) != (0 as i64);

                        if !has {
                            setat(mask, idst + j, neg_inf());
                        } else {
                            var drop: bool = false;

                            if !prev {
                                if pcell + n_swa + 32 as i64 >= at(smin, sid) {
                                    setat(idxs, ibase + nidx, j);
                                    nidx = nidx + 1 as i64;
                                    setat(counts, sid, nidx);
                                }
                            }

                            if causal {
                                if pcell > p1 {
                                    drop = true;
                                }
                            }

                            if !drop {
                                if swa_on {
                                    if is_masked_swa(n_swa, swa_type, pcell, p1) {
                                        drop = true;
                                    }
                                }
                            }
                            var v: i64 = 0 as i64;

                            if drop {
                                v = neg_inf();
                            }
                            setat(mask, idst + j, v);
                        }
                    }
                    jj = jj + 1 as i64;
                }
            }
            ii = ii + 1 as i64;
        }
        s = s + 1 as i64;
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, n_tokens as u64);
    return true;
}

fn main() {
    run(0);
}

