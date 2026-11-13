// Apex KV range operations in Goose: llama_kv_cache::seq_rm(seq_id, p0, p1),
// llama_kv_cache::seq_keep(seq_id) and the same-stream branch of llama_kv_cache::seq_cp.
//
// All three are loops over the cells with a head update afterwards: a freed cell becomes the new
// search head when it lies before the current one.
//
// Cell arena per stream, int64 slots:
//   pos[n] | shift[n] | used[n] | seqw[n*W] | extx[n] | exty[n] | exttok[n]
//   | spos[S*n] | sidx[S*n] | scnt[S] | hsh
// Streams are consecutive: stream k starts at cells + k*stride.
//
// Packet, int64 slots:
//   k=0  0 op, 1 cells, 2 stride, 3 heads, 4 n, 5 W, 6 seq_n, 7 seq_to_stream, 8 n_stream
//   k=1  0 seq_id, 1 dst_seq, 2 p0, 3 p1
//   k=3  0 done, 1 ret
extern "fg291k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg291k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg291k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg291k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg291k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn w() -> i64 {
    return 8 as i64;
}

fn posmax() -> i64 {
    return 2147483647 as i64;
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

fn seq_any(a: u64, n: i64, i: i64, wd: i64) -> bool {
    var k: i64 = 0;

    while k < wd {
        if ld(a, seq_off(n, i, k)) != (0 as i64) {
            return true;
        }
        k = k + 1 as i64;
    }
    return false;
}

fn clear_seq(a: u64, n: i64, i: i64) {
    var k: i64 = 0;

    while k < w() {
        st(a, seq_off(n, i, k), 0 as i64);
        k = k + 1 as i64;
    }
}

fn clear_ext(a: u64, n: i64, i: i64) {
    st(a, ext_off(n, i, 0 as i64), 0 as i64);
    st(a, ext_off(n, i, 1 as i64), 0 as i64);
    st(a, ext_off(n, i, 2 as i64), 0 as i64);
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

fn sp_find(a: u64, n: i64, seq_n: i64, s: i64, i: i64) -> i64 {
    let c: i64 = get_scnt(a, n, seq_n, s);
    var j: i64 = 0;

    while j < c {
        if get_sidx(a, n, s, j) == i {
            return j;
        }
        j = j + 1 as i64;
    }
    return (0 as i64) - 1 as i64;
}

fn sp_del(a: u64, n: i64, seq_n: i64, s: i64, j: i64) {
    let c: i64 = get_scnt(a, n, seq_n, s);
    ensure(j >= 0 as i64 && j < c, 50);
    var k: i64 = j;

    while k < c - (1 as i64) {
        set_spos(a, n, s, k, get_spos(a, n, s, k + 1 as i64));
        set_sidx(a, n, s, k, get_sidx(a, n, s, k + 1 as i64));
        k = k + 1 as i64;
    }
    set_scnt(a, n, seq_n, s, c - (1 as i64));
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

fn sp_rm_cell(a: u64, n: i64, seq_n: i64, i: i64) {
    var s: i64 = 0;

    while s < seq_n {
        if seq_has(a, n, i, w(), s) {
            let j: i64 = sp_find(a, n, seq_n, s, i);
            ensure(j >= 0 as i64, 52);
            sp_del(a, n, seq_n, s, j);
        }
        s = s + 1 as i64;
    }
}

fn sp_inc_cell(a: u64, n: i64, seq_n: i64, s: i64, i: i64) {
    sp_ins(a, n, seq_n, s, ld(a, pos_off(i)), i);
}

fn pos_in(a: u64, i: i64, p0: i64, p1: i64) -> bool {
    let p: i64 = ld(a, pos_off(i));
    return p >= p0 && p < p1;
}
// Cell-level seq_rm: drop one bit; an empty cell is cleared and released.

fn seq_rm_cell(a: u64, n: i64, seq_n: i64, i: i64, s: i64) -> i64 {
    let wi: i64 = s / 64 as i64;
    let cur: i64 = ld(a, seq_off(n, i, wi));
    st(a, seq_off(n, i, wi), cur - (cur & bit_of(s)));
    let j: i64 = sp_find(a, n, seq_n, s, i);
    ensure(j >= 0 as i64, 53);
    sp_del(a, n, seq_n, s, j);

    if !seq_any(a, n, i, w()) {
        st(a, pos_off(i), (0 as i64) - 1 as i64);
        clear_ext(a, n, i);
        st(a, shift_off(n, i), 0 as i64);
        st(a, used_off(n, i), 0 as i64);
        return 1 as i64;
    }
    return 0 as i64;
}
// Cell-level seq_keep: keep only one bit. Returns 1 when the cell was released.

fn seq_keep_cell(a: u64, n: i64, seq_n: i64, i: i64, s: i64) -> i64 {
    if seq_has(a, n, i, w(), s) {
        sp_rm_cell(a, n, seq_n, i);
        clear_seq(a, n, i);
        st(a, seq_off(n, i, s / 64 as i64), bit_of(s));
        sp_inc_cell(a, n, seq_n, s, i);
        return 0 as i64;
    }

    if seq_any(a, n, i, w()) {
        sp_rm_cell(a, n, seq_n, i);
        clear_seq(a, n, i);
        st(a, pos_off(i), (0 as i64) - 1 as i64);
        clear_ext(a, n, i);
        st(a, shift_off(n, i), 0 as i64);
        st(a, used_off(n, i), 0 as i64);
        return 1 as i64;
    }
    ensure(ld(a, pos_off(i)) == (0 as i64) - 1 as i64, 54);
    return 0 as i64;
}
// Cell-level seq_add: set one bit and register the cell.

fn seq_add_cell(a: u64, n: i64, seq_n: i64, i: i64, s: i64) {
    ensure(!seq_has(a, n, i, w(), s), 68);
    let wi: i64 = s / 64 as i64;
    st(a, seq_off(n, i, wi), ld(a, seq_off(n, i, wi)) | bit_of(s));
    sp_inc_cell(a, n, seq_n, s, i);
}

fn head_update(hp: u64, stream: i64, new_head: i64, n: i64) {
    if new_head != n {
        let h: i64 = ld(hp, stream * (8 as i64));

        if new_head < h {
            st(hp, stream * (8 as i64), new_head);
        }
    }
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
    let sid: i64 = get(packet, 1, 0) as i64;
    let did: i64 = get(packet, 1, 1) as i64;
    var p0: i64 = get(packet, 1, 2) as i64;
    var p1: i64 = get(packet, 1, 3) as i64;
    ensure(n > 0 as i64 && wd > (0 as i64), 60);
    ensure(stride > (0 as i64), 61);

    if p0 < 0 as i64 {
        p0 = 0 as i64;
    }

    if p1 < 0 as i64 {
        p1 = posmax();
    }
    var ret: i64 = 0;

    if op == (0 as i64) {
        // seq_rm(seq_id, p0, p1) for one sequence
        ensure(sid >= 0 as i64, 62);
        let stream: i64 = ld(s2s, sid * (8 as i64));
        let a: u64 = cells + ( (stream * stride) as u64);
        var new_head: i64 = n;
        var i: i64 = 0;

        while i < n {
            if pos_in(a, i, p0, p1) {
                if seq_has(a, n, i, wd, sid) {
                    if seq_rm_cell(a, n, seq_n, i, sid) == (1 as i64) {
                        if new_head == n {
                            new_head = i;
                        }
                    }
                }
            }
            i = i + 1 as i64;
        }
        head_update(heads, stream, new_head, n);
    } else {
        if op == (1 as i64) {
            // seq_rm(-1, p0, p1): match any sequence, every stream
            ensure(n_stream > (0 as i64), 63);
            var stream: i64 = 0;

            while stream < n_stream {
                let a: u64 = cells + ( (stream * stride) as u64);
                var new_head: i64 = n;
                var i: i64 = 0;

                while i < n {
                    if pos_in(a, i, p0, p1) {
                        sp_rm_cell(a, n, seq_n, i);
                        st(a, pos_off(i), (0 as i64) - 1 as i64);
                        clear_seq(a, n, i);
                        clear_ext(a, n, i);
                        st(a, shift_off(n, i), 0 as i64);
                        st(a, used_off(n, i), 0 as i64);

                        if new_head == n {
                            new_head = i;
                        }
                    }
                    i = i + 1 as i64;
                }
                head_update(heads, stream, new_head, n);
                stream = stream + 1 as i64;
            }
        } else {
            if op == (2 as i64) {
                // seq_keep(seq_id)
                ensure(sid >= 0 as i64, 64);
                let stream: i64 = ld(s2s, sid * (8 as i64));
                let a: u64 = cells + ( (stream * stride) as u64);
                var new_head: i64 = n;
                var i: i64 = 0;

                while i < n {
                    if seq_keep_cell(a, n, seq_n, i, sid) == (1 as i64) {
                        if new_head == n {
                            new_head = i;
                        }
                    }
                    i = i + 1 as i64;
                }
                head_update(heads, stream, new_head, n);
            } else {
                if op == (3 as i64) {
                    // seq_cp(sid, did, p0, p1) inside one stream
                    ensure(sid >= 0 as i64 && did >= 0 as i64, 65);
                    let s0: i64 = ld(s2s, sid * (8 as i64));
                    let s1: i64 = ld(s2s, did * (8 as i64));
                    ensure(s0 == s1, 66);

                    if sid != did {
                        let a: u64 = cells + ( (s0 * stride) as u64);
                        var i: i64 = 0;

                        while i < n {
                            if pos_in(a, i, p0, p1) {
                                if seq_has(a, n, i, wd, sid) {
                                    seq_add_cell(a, n, seq_n, i, did);
                                }
                            }
                            i = i + 1 as i64;
                        }
                    }
                } else {
                    ensure(false, 67);
                }
            }
        }
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, ret as u64);
    return true;
}

fn main() {
    run(0);
}

