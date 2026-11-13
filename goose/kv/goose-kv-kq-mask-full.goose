// Apex KV cache attention mask in Goose, the full float path
// (set_input_kq_mask with ALiBi, 2-D/M-RoPE and the non-causal SWA span).
//
// Extends EXP316 with the three branches it left out:
//   - ALiBi: the mask stores -|p0-p1| instead of 0 (and no row reuse, because the value depends on
//     the query position);
//   - 2-D/M-RoPE: with causal masking, a cell at the same position is additionally masked when its
//     (x, y) extension is greater than the token's (p1_x, p1_y), compared as y first, then x;
//   - non-causal SWA-full: when attention is non-causal and the cache says so, the sliding window is
//     skipped for cells at or after the sequence's first batch position.
//
// The row-reuse shortcut keeps EXP316's shape: tokens of the same sequence copy the previous row and
// revisit only the cells recorded for that sequence. It is disabled under ALiBi.
//
// Packet, int64 slots:
//   k=0  0 unused, 1 n_kv, 2 n_stream, 3 n_tps, 4 n_tokens, 5 n_seq_max, 6 n_swa, 7 swa_type,
//        8 causal, 9 swa_on, 10 seq_to_stream, 11 ub_seq, 12 width, 13 pos
//   k=1  0 cells_pos, 1 cells_seqw, 2 W, 3 cells_ext_x, 4 cells_ext_y, 5 p1_x, 6 p1_y,
//        7 alibi, 8 is_2d, 9 non_causal_swa_full
//   k=2  0 mask out, 1 srct, 2 counts, 3 idxs, 4 seq_pos_min
//   k=3  0 done, 1 ret
extern "fg317k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg317k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg317k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg317k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg317k_fail" fn fail(code: i64);

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
    let extx: u64 = get(packet, 1, 3);
    let exty: u64 = get(packet, 1, 4);
    let p1x: u64 = get(packet, 1, 5);
    let p1y: u64 = get(packet, 1, 6);
    let alibi: bool = get(packet, 1, 7) != (0 as u64);
    let is_2d: bool = get(packet, 1, 8) != (0 as u64);
    let nc_full: bool = get(packet, 1, 9) != (0 as u64);
    let mask: u64 = get(packet, 2, 0);
    let srct: u64 = get(packet, 2, 1);
    let counts: u64 = get(packet, 2, 2);
    let idxs: u64 = get(packet, 2, 3);
    let smin: u64 = get(packet, 2, 4);
    ensure(n_kv >= (0 as i64) && n_stream > (0 as i64) && n_tps > (0 as i64), 61);
    ensure(width >= (1 as i64) && W > (0 as i64), 62);
    ensure(n_tokens == n_stream * n_tps, 63);
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
            // ALiBi values depend on the query position, so rows cannot be reused

            if !alibi {
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
            } else {
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
                    let cc: i64 = stream * n_kv + j;
                    let pcell: i64 = at(cpos, cc);

                    if pcell == ( (0 as i64) - 1 as i64) {
                        setat(mask, idst + j, neg_inf());
                    } else {
                        let wi: i64 = sid / 64 as i64;
                        let has: bool = (at(cseqw, cc * W + wi) & bit_of(sid)) != (0 as i64);

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

                                if !drop {
                                    if is_2d {
                                        if pcell == p1 {
                                            let ex: i64 = at(extx, cc);
                                            let ey: i64 = at(exty, cc);
                                            let tx: i64 = at(p1x, it);
                                            let ty: i64 = at(p1y, it);
                                            // greater means y first, then x

                                            if ey > ty {
                                                drop = true;
                                            }

                                            if !drop {
                                                if ey == ty {
                                                    if ex > tx {
                                                        drop = true;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            if !drop {
                                if swa_on {
                                    var in_span: bool = false;

                                    if !causal {
                                        if nc_full {
                                            if pcell >= at(smin, sid) {
                                                in_span = true;
                                            }
                                        }
                                    }

                                    if !in_span {
                                        if is_masked_swa(n_swa, swa_type, pcell, p1) {
                                            drop = true;
                                        }
                                    }
                                }
                            }

                            if drop {
                                setat(mask, idst + j, neg_inf());
                            } else {
                                if alibi {
                                    var d: i64 = pcell - p1;

                                    if d < (0 as i64) {
                                        d = (0 as i64) - d;
                                    }
                                    setat(mask, idst + j, (0 as i64) - d);
                                } else {
                                    setat(mask, idst + j, 0 as i64);
                                }
                            }
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

