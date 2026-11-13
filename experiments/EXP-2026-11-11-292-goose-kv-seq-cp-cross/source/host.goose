// Apex KV cross-stream seq_cp in Goose (llama_kv_cache::seq_cp, stream-copy branch).
//
// When source and destination sequences live in different streams, the cell metadata is not
// enough: the destination stream's cells are rebuilt from the source. Native code also enqueues a
// buffer copy through sc_info; here that is reported as the (src,dst) pair the caller must queue,
// which is the only part of the branch that touches buffers.
//
// The copy is only supported for full buffers: native rejects it when either position bound sits
// inside the cache.
//
// Cell arena per stream, int64 slots (same layout as EXP289/EXP291):
//   pos[n] | shift[n] | used[n] | seqw[n*W] | extx[n] | exty[n] | exttok[n]
//   | spos[S*n] | sidx[S*n] | scnt[S] | hsh
//
// Packet, int64 slots:
//   k=0  0 op, 1 cells, 2 stride, 3 heads, 4 n, 5 W, 6 seq_n, 7 seq_to_stream, 8 n_stream, 9 size
//   k=1  0 src_seq, 1 dst_seq, 2 p0, 3 p1, 4 queue_src, 5 queue_dst
//   k=3  0 done, 1 ok
extern "fg292k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg292k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg292k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg292k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg292k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn w() -> i64 {
    return 8 as i64;
}

fn pos_off(i: i64) -> i64 {
    return i * 8 as i64;
}

fn shift_off(n: i64, i: i64) -> i64 {
    return (n + i) * 8 as i64;
}

fn used_off(n: i64, i: i64) -> i64 {
    return (2 * n + i) * 8 as i64;
}

fn seq_off(n: i64, i: i64, wi: i64) -> i64 {
    return (3 * n + i * w() + wi) * 8 as i64;
}

fn ext_off(n: i64, i: i64, k: i64) -> i64 {
    return (3 * n + n * w() + k * n + i) * 8 as i64;
}

fn spos_off(n: i64, s: i64, j: i64) -> i64 {
    return (6 * n + n * w() + (s * n + j)) * 8 as i64;
}

fn sidx_off(n: i64, s: i64, j: i64) -> i64 {
    return (6 * n + n * w() + n * n + (s * n + j)) * 8 as i64;
}

fn scnt_off(n: i64, seq_n: i64, s: i64) -> i64 {
    return (6 * n + n * w() + 2 * n * n + s) * 8 as i64;
}

fn hsh_off(n: i64, seq_n: i64) -> i64 {
    return (6 * n + n * w() + 2 * n * n + seq_n) * 8 as i64;
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

fn seq_has(a: u64, n: i64, i: i64, wd: i64, s: i64) -> bool {
    return (ld(a, seq_off(n, i, wd * s / 64 as i64)) & bit_of(s)) != (0 as i64);
}

fn get_scnt(a: u64, n: i64, seq_n: i64, s: i64) -> i64 {
    return ld(a, scnt_off(n, seq_n, s));
}

fn set_scnt(a: u64, n: i64, seq_n: i64, s: i64, v: i64) {
    st(a, scnt_off(n, seq_n, s), v);
}

fn get_spos(a: u64, n: i64, s: i64, j: i64) -> i64 {
    return ld(a, spos_off(n, s, j));
}

fn set_spos(a: u64, n: i64, s: i64, j: i64, v: i64) {
    st(a, spos_off(n, s, j), v);
}

fn get_sidx(a: u64, n: i64, s: i64, j: i64) -> i64 {
    return ld(a, sidx_off(n, s, j));
}

fn set_sidx(a: u64, n: i64, s: i64, j: i64, v: i64) {
    st(a, sidx_off(n, s, j), v);
}

fn sp_ins(a: u64, n: i64, seq_n: i64, s: i64, p: i64, i: i64) {
    let c: i64 = get_scnt(a, n, seq_n, s);
    ensure(c < n, 51);
    var j: i64 = c;
    var k: i64 = 0;
    var done: bool = false;

    while k < c {
        if !done {
            let q: i64 = get_spos(a, n, s, k);

            if q > p {
                j = k;
                done = true;
            } else {
                if q == p {
                    if get_sidx(a, n, s, k) > i {
                        j = k;
                        done = true;
                    }
                }
            }
        }
        k = k + 1 as i64;
    }
    var m: i64 = c;

    while m > j {
        set_spos(a, n, s, m, get_spos(a, n, s, m - (1 as i64)));
        set_sidx(a, n, s, m, get_sidx(a, n, s, m - (1 as i64)));
        m = m - (1 as i64);
    }
    set_spos(a, n, s, j, p);
    set_sidx(a, n, s, j, i);
    set_scnt(a, n, seq_n, s, c + (1 as i64));
}
// v_cells[s1].reset(): positions -1, all words and ext zero, every count zero, head untouched.

fn reset_cells(a: u64, n: i64, seq_n: i64) {
    var i: i64 = 0;

    while i < n {
        st(a, pos_off(i), (0 as i64) - 1 as i64);
        st(a, shift_off(n, i), 0 as i64);
        st(a, used_off(n, i), 0 as i64);
        st(a, ext_off(n, i, 0 as i64), 0 as i64);
        st(a, ext_off(n, i, 1 as i64), 0 as i64);
        st(a, ext_off(n, i, 2 as i64), 0 as i64);
        var wi: i64 = 0;

        while wi < w() {
            st(a, seq_off(n, i, wi), 0 as i64);
            wi = wi + 1 as i64;
        }
        i = i + 1 as i64;
    }
    var s: i64 = 0;

    while s < seq_n {
        set_scnt(a, n, seq_n, s, 0 as i64);
        s = s + 1 as i64;
    }
    st(a, hsh_off(n, seq_n), 0 as i64);
}
// The cross-stream branch of seq_cp.

fn seq_cp_cross(
    c0: u64,
    c1: u64,
    n: i64,
    wd: i64,
    seq_n: i64,
    sid: i64,
    did: i64,
    p0: i64,
    p1: i64,
    size: i64) -> bool {
    var is_full: bool = true;

    if p0 > 0 as i64 {
        if p0 + (1 as i64) < size {
            is_full = false;
        }
    }

    if p1 > 0 as i64 {
        if p1 + (1 as i64) < size {
            is_full = false;
        }
    }
    ensure(is_full, 72);
    reset_cells(c1, n, seq_n);
    var i: i64 = 0;

    while i < n {
        if seq_has(c0, n, i, wd, sid) {
            var p: i64 = ld(c0, pos_off(i));
            let sh: i64 = ld(c0, shift_off(n, i));
            let ex: i64 = ld(c0, ext_off(n, i, 0 as i64));
            let ey: i64 = ld(c0, ext_off(n, i, 1 as i64));
            let et: i64 = ld(c0, ext_off(n, i, 2 as i64));

            if sh != (0 as i64) {
                p = p - sh;
                ensure(p >= 0 as i64, 70);
            }
            st(c1, pos_off(i), p);
            st(c1, used_off(n, i), 1 as i64);
            let wi: i64 = did / 64 as i64;
            st(c1, seq_off(n, i, wi), ld(c1, seq_off(n, i, wi)) | bit_of(did));
            sp_ins(c1, n, seq_n, did, p, i);

            if sh != (0 as i64) {
                // pos_add: move the cell by the shift and keep the sequence arrays ordered.
                let c2: i64 = get_scnt(c1, n, seq_n, did);
                var j: i64 = 0;
                var found: i64 = (0 as i64) - 1 as i64;

                while j < c2 {
                    if get_sidx(c1, n, did, j) == i {
                        found = j;
                    }
                    j = j + 1 as i64;
                }
                ensure(found >= 0 as i64, 71);
                var m: i64 = found;

                while m < c2 - (1 as i64) {
                    set_spos(c1, n, did, m, get_spos(c1, n, did, m + 1 as i64));
                    set_sidx(c1, n, did, m, get_sidx(c1, n, did, m + 1 as i64));
                    m = m + 1 as i64;
                }
                set_scnt(c1, n, seq_n, did, c2 - (1 as i64));
                let np: i64 = p + sh;
                st(c1, pos_off(i), np);
                st(c1, shift_off(n, i), ld(c1, shift_off(n, i)) + sh);
                st(c1, hsh_off(n, seq_n), 1 as i64);
                sp_ins(c1, n, seq_n, did, np, i);
            }
            st(c1, ext_off(n, i, 0 as i64), ex);
            st(c1, ext_off(n, i, 1 as i64), ey);
            st(c1, ext_off(n, i, 2 as i64), et);
        }
        i = i + 1 as i64;
    }
    return true;
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let cells: u64 = get(packet, 0, 1);
    let stride: i64 = get(packet, 0, 2) as i64;
    let heads: u64 = get(packet, 0, 3);
    let n: i64 = get(packet, 0, 4) as i64;
    let wd: i64 = get(packet, 0, 5) as i64;
    let seq_n: i64 = get(packet, 0, 6) as i64;
    let s2s: u64 = get(packet, 0, 7);
    let n_stream: i64 = get(packet, 0, 8) as i64;
    let size: i64 = get(packet, 0, 9) as i64;
    let sid: i64 = get(packet, 1, 0) as i64;
    let did: i64 = get(packet, 1, 1) as i64;
    let p0: i64 = get(packet, 1, 2) as i64;
    let p1: i64 = get(packet, 1, 3) as i64;
    ensure(n > 0 as i64 && wd > (0 as i64) && seq_n > (0 as i64), 60);
    ensure(stride > (0 as i64), 61);
    ensure(n_stream > (1 as i64), 62);
    ensure(size > 0 as i64, 63);
    ensure(op == (0 as i64), 64);
    ensure(sid >= 0 as i64 && did >= 0 as i64, 65);
    let s0: i64 = ld(s2s, sid * (8 as i64));
    let s1: i64 = ld(s2s, did * (8 as i64));
    ensure(s0 != s1, 66);
    let c0: u64 = cells + ( (s0 * stride) as u64);
    let c1: u64 = cells + ( (s1 * stride) as u64);
    let ok: bool = seq_cp_cross(c0, c1, n, wd, seq_n, sid, did, p0, p1, size);

    if ok {
        st(heads, s1 * (8 as i64), ld(heads, s0 * (8 as i64)));
        let qs: u64 = get(packet, 1, 4);
        let qd: u64 = get(packet, 1, 5);
        st(qs, 0 as i64, s0);
        st(qd, 0 as i64, s1);
    }
    put(packet, 3, 0, 1 as u64);
    var flag: i64 = 0;

    if ok {
        flag = 1 as i64;
    }
    put(packet, 3, 1, flag as u64);
    return true;
}

fn main() {
    run(0);
}

