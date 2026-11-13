// Apex recurrent-state sequence operations in Goose:
// llama_memory_recurrent::seq_cp, seq_keep, seq_add and seq_div.
//
// Recurrent caches hold one cell per sequence, so these operations touch a single cell per
// sequence (or the whole array for seq_keep) instead of a position range of cells. The position
// bounds still apply, but they select the sequence's cell, not individual cells.
//
// Arena layout, int64 slots:
//   pos[n] | src[n] | src0[n] | tail[n] | seqw[n*SW] | head | used | n
//
// Packet, int64 slots:
//   k=0  0 op, 1 arena, 2 n, 3 SW
//   k=1  0 seq_id, 1 seq_dst, 2 p0, 3 p1, 4 shift
//   k=3  0 done
extern "fg294k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg294k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg294k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg294k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg294k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn posmax() -> i64 {
    return 2147483647 as i64;
}

fn pos_off(i: i64) -> i64 {
    return i * 8 as i64;
}

fn src_off(n: i64, i: i64) -> i64 {
    return (n + i) * 8 as i64;
}

fn src0_off(n: i64, i: i64) -> i64 {
    return (2 * n + i) * 8 as i64;
}

fn tail_off(n: i64, i: i64) -> i64 {
    return (3 * n + i) * 8 as i64;
}

fn seqw_off(n: i64, i: i64, sw: i64, w: i64) -> i64 {
    return (4 * n + i * sw + w) * 8 as i64;
}

fn head_off(n: i64, sw: i64) -> i64 {
    return (4 * n + n * sw) * 8 as i64;
}

fn used_off(n: i64, sw: i64) -> i64 {
    return (4 * n + n * sw + 1 as i64) * 8 as i64;
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

fn seq_test(a: u64, n: i64, i: i64, sw: i64, s: i64) -> bool {
    return (ld(a, seqw_off(n, i, sw, s / 64 as i64)) & bit_of(s)) != (0 as i64);
}

fn seq_set(a: u64, n: i64, i: i64, sw: i64, s: i64) {
    let wi: i64 = s / 64 as i64;
    st(a, seqw_off(n, i, sw, wi), ld(a, seqw_off(n, i, sw, wi)) | bit_of(s));
}

fn seq_del(a: u64, n: i64, i: i64, sw: i64, s: i64) {
    let wi: i64 = s / 64 as i64;
    let cur: i64 = ld(a, seqw_off(n, i, sw, wi));
    st(a, seqw_off(n, i, sw, wi), cur - (cur & bit_of(s)));
}

fn seq_clear(a: u64, n: i64, i: i64, sw: i64) {
    var w: i64 = 0;

    while w < sw {
        st(a, seqw_off(n, i, sw, w), 0 as i64);
        w = w + 1 as i64;
    }
}

fn seq_size(a: u64, n: i64, i: i64, sw: i64) -> i64 {
    var c: i64 = 0;
    var s: i64 = 0;

    while s < sw * 64 as i64 {
        if seq_test(a, n, i, sw, s) {
            c = c + 1 as i64;
        }
        s = s + 1 as i64;
    }
    c
}
// seq_cp: move the destination's ownership to the source's cell.

fn seq_cp(a: u64, n: i64, sw: i64, sid: i64, did: i64) -> bool {
    if sid == did {
        return true;
    }

    if ! (did < n && sid < n) {
        return true;
    }
    let td: i64 = ld(a, tail_off(n, did));

    if td >= 0 as i64 {
        seq_del(a, n, td, sw, did);
        st(a, tail_off(n, did), (0 as i64) - 1 as i64);

        if seq_size(a, n, td, sw) == (0 as i64) {
            st(a, pos_off(td), (0 as i64) - 1 as i64);
            st(a, src_off(n, td), (0 as i64) - 1 as i64);
            st(a, used_off(n, sw), ld(a, used_off(n, sw)) - (1 as i64));
        }
    }
    let ts: i64 = ld(a, tail_off(n, sid));

    if ts >= 0 as i64 {
        seq_set(a, n, ts, sw, did);
        st(a, tail_off(n, did), ts);
    }
    return true;
}
// seq_keep: only one sequence survives, every other cell is released.

fn seq_keep(a: u64, n: i64, sw: i64, sid: i64) {
    var new_head: i64 = n;
    var i: i64 = 0;

    while i < n {
        if i != sid {
            st(a, tail_off(n, i), (0 as i64) - 1 as i64);
        }

        if !seq_test(a, n, i, sw, sid) {
            if ld(a, pos_off(i)) >= 0 as i64 {
                st(a, used_off(n, sw), ld(a, used_off(n, sw)) - (1 as i64));
            }
            st(a, pos_off(i), (0 as i64) - 1 as i64);
            st(a, src_off(n, i), (0 as i64) - 1 as i64);
            seq_clear(a, n, i, sw);

            if new_head == n {
                new_head = i;
            }
        } else {
            seq_clear(a, n, i, sw);
            seq_set(a, n, i, sw, sid);
        }
        i = i + 1 as i64;
    }

    if new_head != n {
        if new_head < ld(a, head_off(n, sw)) {
            st(a, head_off(n, sw), new_head);
        }
    }
}
// The cell of sid, if it owns one and its position falls in [p0, p1).

fn cell_in_range(a: u64, n: i64, sw: i64, sid: i64, p0: i64, p1: i64) -> i64 {
    if sid < 0 as i64 {
        return (0 as i64) - 1 as i64;
    }

    if sid >= n {
        return (0 as i64) - 1 as i64;
    }
    let t: i64 = ld(a, tail_off(n, sid));

    if t < 0 as i64 {
        return (0 as i64) - 1 as i64;
    }

    if !seq_test(a, n, t, sw, sid) {
        return (0 as i64) - 1 as i64;
    }
    let p: i64 = ld(a, pos_off(t));

    if p < p0 {
        return (0 as i64) - 1 as i64;
    }

    if p >= p1 {
        return (0 as i64) - 1 as i64;
    }
    return t;
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let a: u64 = get(packet, 0, 1);
    let n: i64 = get(packet, 0, 2) as i64;
    let sw: i64 = get(packet, 0, 3) as i64;
    let sid: i64 = get(packet, 1, 0) as i64;
    let did: i64 = get(packet, 1, 1) as i64;
    var p0: i64 = get(packet, 1, 2) as i64;
    var p1: i64 = get(packet, 1, 3) as i64;
    let sh: i64 = get(packet, 1, 4) as i64;
    ensure(n > 0 as i64 && sw > (0 as i64), 60);

    if op == (0 as i64) {
        // Native seq_cp clamps p0/p1 but never uses them: the copy is per sequence.
        seq_cp(a, n, sw, sid, did);
    } else {
        if op == (1 as i64) {
            seq_keep(a, n, sw, sid);
        } else {
            if op == (2 as i64) {
                if sh != (0 as i64) {
                    var q0: i64 = p0;
                    var q1: i64 = p1;

                    if q0 < 0 as i64 {
                        q0 = 0 as i64;
                    }

                    if q1 < 0 as i64 {
                        q1 = posmax();
                    }

                    if q0 != q1 {
                        let c: i64 = cell_in_range(a, n, sw, sid, q0, q1);

                        if c >= 0 as i64 {
                            st(a, pos_off(c), ld(a, pos_off(c)) + sh);
                        }
                    }
                }
            } else {
                if op == (3 as i64) {
                    let d: i64 = sh;

                    if d != (1 as i64) {
                        var q0: i64 = p0;
                        var q1: i64 = p1;

                        if q0 < 0 as i64 {
                            q0 = 0 as i64;
                        }

                        if q1 < 0 as i64 {
                            q1 = posmax();
                        }

                        if q0 != q1 {
                            ensure(d != (0 as i64), 61);
                            let c: i64 = cell_in_range(a, n, sw, sid, q0, q1);

                            if c >= 0 as i64 {
                                st(a, pos_off(c), ld(a, pos_off(c)) / d);
                            }
                        }
                    }
                } else {
                    ensure(false, 62);
                }
            }
        }
    }
    put(packet, 3, 0, 1 as u64);
    return true;
}

fn main() {
    run(0);
}

