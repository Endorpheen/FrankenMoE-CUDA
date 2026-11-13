// Apex KV state ranges in Goose: which cells state_write puts in the stream (P5-08).
//
// A cell is written when it is not empty, when it carries the requested sequence (all cells when the
// sequence is -1), and — only for a named sequence — when it is not excluded by the sliding window
// at the sequence's highest position. Consecutive cells are coalesced into ranges, and the counts
// must match the sum of the range lengths.
//
// The partial-only flag drops the whole section for a cache without a sliding window.
//
// Packet, int64 slots:
//   k=0  0 n_stream, 1 n_cells, 2 seq_id, 3 n_swa, 4 swa_type, 5 partial_only, 6 cells_pos,
//        7 cells_seqw, 8 W, 9 seq_pos_max
//   k=1  0 out counts, 1 out ranges, 2 out total
//   k=3  0 done, 1 ret (total cells written, or -1 when the section is dropped)
extern "fg321k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg321k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg321k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg321k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg321k_fail" fn fail(code: i64);

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
    let n_stream: i64 = get(packet, 0, 0) as i64;
    let n_cells: i64 = get(packet, 0, 1) as i64;
    let seq_id: i64 = get(packet, 0, 2) as i64;
    let n_swa: i64 = get(packet, 0, 3) as i64;
    let swa_type: i64 = get(packet, 0, 4) as i64;
    let partial_only: i64 = get(packet, 0, 5) as i64;
    let cpos: u64 = get(packet, 0, 6);
    let cseqw: u64 = get(packet, 0, 7);
    let W: i64 = get(packet, 0, 8) as i64;
    let spmax: u64 = get(packet, 0, 9);
    let outc: u64 = get(packet, 1, 0);
    let outr: u64 = get(packet, 1, 1);
    let outt: u64 = get(packet, 1, 2);
    ensure(n_stream >= (0 as i64) && n_cells >= (0 as i64) && W > (0 as i64), 61);
    // a cache without a sliding window drops the partial-only section entirely

    if partial_only != (0 as i64) {
        if swa_type == (0 as i64) {
            put(packet, 3, 0, 1 as u64);
            put(packet, 3, 1, ( (0 as i64) - 1 as i64) as u64);
            return true;
        }
    }
    var total: i64 = 0;
    var rr: i64 = 0;
    var s: i64 = 0;

    while s < n_stream {
        let base: i64 = s * n_cells;
        var cnt: i64 = 0;
        var begin: i64 = n_cells;
        var i: i64 = 0;

        while i < n_cells {
            var add: bool = true;

            if at(cpos, base + i) == ( (0 as i64) - 1 as i64) {
                add = false;
            }

            if add {
                if seq_id != (0 as i64) - 1 as i64 {
                    let wi: i64 = seq_id / 64 as i64;

                    if (at(cseqw, (base + i) * W + wi) & bit_of(seq_id)) == (0 as i64) {
                        add = false;
                    }
                }
            }

            if add {
                if seq_id != (0 as i64) - 1 as i64 {
                    let is_masked: bool = is_masked_swa(n_swa, swa_type, at(cpos, base + i), at(spmax, s));

                    if is_masked {
                        add = false;
                    }
                }
            }

            if add {
                cnt = cnt + 1 as i64;

                if begin == n_cells {
                    begin = i;
                }
            } else {
                if begin != n_cells {
                    setat(outr, (2 as i64 * rr), begin);
                    setat(outr, (2 as i64 * rr + 1 as i64), i);
                    rr = rr + 1 as i64;
                    begin = n_cells;
                }
            }
            i = i + 1 as i64;
        }

        if begin != n_cells {
            setat(outr, (2 as i64 * rr), begin);
            setat(outr, (2 as i64 * rr + 1 as i64), n_cells);
            rr = rr + 1 as i64;
        }
        setat(outc, s, cnt);
        total = total + cnt;
        s = s + 1 as i64;
    }
    setat(outt, 0 as i64, total);
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, total as u64);
    return true;
}

fn main() {
    run(0);
}

