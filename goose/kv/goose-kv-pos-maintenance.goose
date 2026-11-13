// KV cell position maintenance in Goose, from llama-kv-cells.h.
//
// Transfers pos_add, pos_div and the seq_pos observers (min, max, tok_le) together with the
// seq_pos bookkeeping they depend on: seq_pos_dec, seq_pos_inc, seq_pos_rm and seq_pos_add.
//
// The containers stay on the host side, as in EXP199: std::bitset, std::set and std::vector are
// storage. seq_pos[s] is held as the ordered array (spos, sidx) with a count, which is the same
// order std::set<std::pair<pos,uint32_t>> keeps: position first, cell index second. Every
// decision - what moves, where the pair is inserted, what the observers return - is Goose.
//
// Arena layout, int64 slots, all regions consecutive:
//   pos[n] | shift[n] | used[n] | seqw[n*W] | ext[n] | spos[S*n] | sidx[S*n] | scnt[S] | hsh
// where W = 8 words (512 sequence bits) and S = the sequence count passed in.
//
// hsh is the class's has_shift flag. It lives in the arena because pos_add and pos_div set it.
extern "fg289k_fail" fn fail(code: i64);
extern "fg289k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg289k_st_i64" fn st(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// A const would read as zero: the emitter puts consts in gg_globals, which only gg_program
// initialises, and this module is called directly. EXP199 hit the same trap.

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

fn ext_off(n: i64, i: i64) -> i64 {
    return (3 * n + n * w() + i) * 8 as i64;
}

fn spos_off(n: i64, s: i64, j: i64) -> i64 {
    return (4 * n + n * w() + (s * n + j)) * 8 as i64;
}

fn sidx_off(n: i64, s: i64, j: i64) -> i64 {
    return (4 * n + n * w() + n * n + (s * n + j)) * 8 as i64;
}

fn scnt_off(n: i64, s: i64, seq_n: i64) -> i64 {
    return (4 * n + n * w() + 2 * n * n + s) * 8 as i64;
}

fn hsh_off(n: i64, seq_n: i64) -> i64 {
    return (4 * n + n * w() + 2 * n * n + seq_n) * 8 as i64;
}

fn get_pos(a: u64, i: i64) -> i64 {
    return ld(a, pos_off(i));
}

fn set_pos(a: u64, i: i64, v: i64) {
    st(a, pos_off(i), v);
}

fn get_shift(a: u64, n: i64, i: i64) -> i64 {
    return ld(a, shift_off(n, i));
}

fn set_shift(a: u64, n: i64, i: i64, v: i64) {
    st(a, shift_off(n, i), v);
}

fn get_used(a: u64, n: i64, i: i64) -> i64 {
    return ld(a, used_off(n, i));
}

fn set_used(a: u64, n: i64, i: i64, v: i64) {
    st(a, used_off(n, i), v);
}

fn get_ext(a: u64, n: i64, i: i64) -> i64 {
    return ld(a, ext_off(n, i));
}

fn set_ext(a: u64, n: i64, i: i64, v: i64) {
    st(a, ext_off(n, i), v);
}

fn get_scnt(a: u64, n: i64, seq_n: i64, s: i64) -> i64 {
    return ld(a, scnt_off(n, s, seq_n));
}

fn set_scnt(a: u64, n: i64, seq_n: i64, s: i64, v: i64) {
    st(a, scnt_off(n, s, seq_n), v);
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
// bit of sequence s in the cell's word array

fn bit_of(s: i64) -> i64 {
    var b: i64 = 1 as i64;
    var k: i64 = 0;

    while k < s % 64 as i64 {
        b = b * 2 as i64;
        k = k + 1 as i64;
    }
    b
}

fn seq_has(a: u64, n: i64, i: i64, s: i64) -> bool {
    let wi: i64 = s / 64 as i64;
    return (ld(a, seq_off(n, i, wi)) & bit_of(s)) != (0 as i64);
}

fn clear_seq(a: u64, n: i64, i: i64) {
    var wi: i64 = 0;

    while wi < w() {
        st(a, seq_off(n, i, wi), 0 as i64);
        wi = wi + 1 as i64;
    }
}
// Position of cell i inside seq s's ordered array, or -1.

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
// Remove slot j, shifting the tail down.

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
// Insert (p,i) keeping the array ordered by (position, cell index).

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
// seq_pos_rm: drop cell i from every sequence that owns it.

fn sp_rm_cell(a: u64, n: i64, seq_n: i64, i: i64) {
    var s: i64 = 0;

    while s < seq_n {
        if seq_has(a, n, i, s) {
            let j: i64 = sp_find(a, n, seq_n, s, i);
            ensure(j >= 0 as i64, 52);
            sp_del(a, n, seq_n, s, j);
        }
        s = s + 1 as i64;
    }
}
// seq_pos_add: insert cell i into every sequence that owns it.

fn sp_add_cell(a: u64, n: i64, seq_n: i64, i: i64) {
    let p: i64 = get_pos(a, i);
    var s: i64 = 0;

    while s < seq_n {
        if seq_has(a, n, i, s) {
            sp_ins(a, n, seq_n, s, p, i);
        }
        s = s + 1 as i64;
    }
}
// pos[i] += d. Returns 1 when the cell was released, 0 otherwise.

fn pos_add(a: u64, n: i64, seq_n: i64, i: i64, d: i64) -> i64 {
    ensure(n > 0 as i64 && i >= 0 as i64 && i < n, 60);
    let p0: i64 = get_pos(a, i);
    ensure(p0 != (0 as i64) - 1 as i64, 61);
    sp_rm_cell(a, n, seq_n, i);
    let p: i64 = p0 + d;
    let sh: i64 = get_shift(a, n, i) + d;
    set_pos(a, i, p);
    set_shift(a, n, i, sh);
    st(a, hsh_off(n, seq_n), 1 as i64);

    if p < 0 as i64 {
        clear_seq(a, n, i);
        set_pos(a, i, (0 as i64) - 1 as i64);
        set_shift(a, n, i, 0 as i64);
        set_used(a, n, i, 0 as i64);
        return 1 as i64;
    }
    sp_add_cell(a, n, seq_n, i);
    return 0 as i64;
}
// pos[i] /= d, and the shift absorbs the difference.

fn pos_div(a: u64, n: i64, seq_n: i64, i: i64, d: i64) {
    ensure(n > 0 as i64 && i >= 0 as i64 && i < n, 70);
    ensure(d != (0 as i64), 71);
    let p0: i64 = get_pos(a, i);
    ensure(p0 != (0 as i64) - 1 as i64, 72);
    sp_rm_cell(a, n, seq_n, i);
    let p: i64 = p0 / d;
    set_pos(a, i, p);
    set_shift(a, n, i, get_shift(a, n, i) + (p0 - p));
    sp_add_cell(a, n, seq_n, i);
    st(a, hsh_off(n, seq_n), 1 as i64);
}

fn seq_pos_min(a: u64, n: i64, seq_n: i64, s: i64) -> i64 {
    ensure(s >= 0 as i64 && s < seq_n, 80);

    if get_scnt(a, n, seq_n, s) == (0 as i64) {
        return (0 as i64) - 1 as i64;
    }
    return get_spos(a, n, s, 0 as i64);
}

fn seq_pos_max(a: u64, n: i64, seq_n: i64, s: i64) -> i64 {
    ensure(s >= 0 as i64 && s < seq_n, 81);
    let c: i64 = get_scnt(a, n, seq_n, s);

    if c == (0 as i64) {
        return (0 as i64) - 1 as i64;
    }
    return get_spos(a, n, s, c - (1 as i64));
}
// The cell of seq s at the largest position <= p; the highest index wins a tie.

fn seq_pos_tok_le(a: u64, n: i64, seq_n: i64, s: i64, p: i64) -> i64 {
    ensure(s >= 0 as i64 && s < seq_n, 82);
    let c: i64 = get_scnt(a, n, seq_n, s);
    var j: i64 = (0 as i64) - 1 as i64;
    var k: i64 = 0;

    while k < c {
        if get_spos(a, n, s, k) <= p {
            j = k;
        }
        k = k + 1 as i64;
    }

    if j < 0 as i64 {
        return (0 as i64) - 1 as i64;
    }
    return get_ext(a, n, get_sidx(a, n, s, j));
}
// pos_set: install a position in an empty cell and mark it used.

fn pos_set(a: u64, n: i64, i: i64, p: i64) {
    ensure(n > 0 as i64 && i >= 0 as i64 && i < n, 90);
    ensure(get_pos(a, i) == (0 as i64) - 1 as i64, 91);
    var s: i64 = 0;

    while s < 64 as i64 {
        ensure(!seq_has(a, n, i, s), 92);
        s = s + 1 as i64;
    }
    set_pos(a, i, p);
    set_used(a, n, i, 1 as i64);
}
// seq_add: set one sequence bit and register the cell in that sequence's array.

fn seq_add(a: u64, n: i64, seq_n: i64, i: i64, s: i64) {
    ensure(i >= 0 as i64 && i < n, 93);
    ensure(get_pos(a, i) != (0 as i64) - 1 as i64, 94);
    ensure(!seq_has(a, n, i, s), 95);
    let wi: i64 = s / 64 as i64;
    st(a, seq_off(n, i, wi), ld(a, seq_off(n, i, wi)) | bit_of(s));
    sp_ins(a, n, seq_n, s, get_pos(a, i), i);
}
// Dispatcher so the host gate reaches every operation through one entry.
// Packet (0,k): 0 op, 1 arena, 2 n, 3 seq_n, 4 i, 5 d, 6 s, 7 p.
// Packet (1,0): the return value. (1,1): has_shift after the call.
extern "fg289h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg289h_put" fn put(p: i64, k: i64, i: i64, v: u64);

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let a: u64 = get(packet, 0, 1);
    let n: i64 = get(packet, 0, 2) as i64;
    let seq_n: i64 = get(packet, 0, 3) as i64;
    let i: i64 = get(packet, 0, 4) as i64;
    let d: i64 = get(packet, 0, 5) as i64;
    let s: i64 = get(packet, 0, 6) as i64;
    let p: i64 = get(packet, 0, 7) as i64;
    var r: i64 = 0;

    if op == (0 as i64) {
        r = pos_add(a, n, seq_n, i, d);
    }

    if op == (1 as i64) {
        pos_div(a, n, seq_n, i, d);
    }

    if op == (2 as i64) {
        r = seq_pos_min(a, n, seq_n, s);
    }

    if op == (3 as i64) {
        r = seq_pos_max(a, n, seq_n, s);
    }

    if op == (4 as i64) {
        r = seq_pos_tok_le(a, n, seq_n, s, p);
    }

    if op == (5 as i64) {
        set_ext(a, n, i, d);
    }

    if op == (6 as i64) {
        pos_set(a, n, i, d);
    }

    if op == (7 as i64) {
        seq_add(a, n, seq_n, i, s);
    }
    put(packet, 1, 0, r as u64);
    put(packet, 1, 1, (ld(a, hsh_off(n, seq_n))) as u64);
    return true;
}

fn main() {
    run(0);
}

