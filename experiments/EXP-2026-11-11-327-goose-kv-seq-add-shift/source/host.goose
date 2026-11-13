// Apex KV sequence shift in Goose (llama_kv_cache::seq_add(seq_id, p0, p1, shift)).
//
// A shifted cache is a no-op in four cases, checked before any cell is touched: the cache mirrors
// another one, the shift is zero, the resolved range is empty, and — per cell — the position is
// outside the range or the cell does not hold this sequence.
//
// A cell that is shifted to a negative position is freed: its sequence bits are dropped and its
// position and shift reset. The head becomes the first such cell, and when nothing was freed it
// becomes zero — not the smallest freed index, and not left alone.
//
// Order inside a shifted cell matters: the per-sequence position index is unlinked first, then the
// position moves, then it is relinked unless the cell was freed.
//
// Packet, int64 slots:
//   k=0  0 other, 1 n, 2 seq_id, 3 p0, 4 p1, 5 shift, 6 pos, 7 seqbits, 8 cell_shift, 9 head
//   k=1  0 out
//   k=2  0 count ptr
//   k=3  0 done, 1 ret (number of shifted cells)
//
// Records, three slots: kind 0 = cell shifted (a = index, b = new position, c is derived: negative
// means freed), kind 1 = head written (a = value), kind 2 = per-sequence index unlinked (a = index),
// kind 3 = per-sequence index relinked (a = index).
extern "fg327k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg327k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg327k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg327k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg327k_fail" fn fail(code: i64);

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

fn run(packet: i64) -> bool {
    let other: i64 = get(packet, 0, 0) as i64;
    let n: i64 = get(packet, 0, 1) as i64;
    let seq_id: i64 = get(packet, 0, 2) as i64;
    var p0: i64 = get(packet, 0, 3) as i64;
    var p1: i64 = get(packet, 0, 4) as i64;
    let shift: i64 = get(packet, 0, 5) as i64;
    let pos: u64 = get(packet, 0, 6);
    let seqbits: u64 = get(packet, 0, 7);
    let headp: u64 = get(packet, 0, 9);
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 2, 0);
    ensure(n > 0 as i64, 60);
    ensure(seq_id >= (0 as i64), 61);
    ensure(pos != (0 as u64) && seqbits != (0 as u64) && headp != (0 as u64), 62);

    if other != (0 as i64) {
        put(packet, 3, 0, 1 as u64);
        put(packet, 3, 1, 0 as i64 as u64);
        return true;
    }

    if shift == (0 as i64) {
        put(packet, 3, 0, 1 as u64);
        put(packet, 3, 1, 0 as i64 as u64);
        return true;
    }

    if p0 < (0 as i64) {
        p0 = 0 as i64;
    }

    if p1 < (0 as i64) {
        p1 = (1 as i64) << 62 as i64;
    }

    if p0 == p1 {
        put(packet, 3, 0, 1 as u64);
        put(packet, 3, 1, 0 as i64 as u64);
        return true;
    }
    var count: i64 = 0;
    var new_head: i64 = n;
    var i: i64 = 0;

    while i < n {
        let p: i64 = at(pos, i);
        var inside: bool = false;

        if p >= p0 && p < p1 {
            inside = true;
        }
        var has: bool = false;

        if ( (at(seqbits, i) & (1 as i64)) != (0 as i64)) {
            has = true;
        }

        if inside && has {
            emit(out, cntp, 2 as i64, i, 0 as i64);
            let np: i64 = p + shift;
            emit(out, cntp, 0 as i64, i, np);
            count = count + 1 as i64;

            if np < (0 as i64) {
                if new_head == n {
                    new_head = i;
                }
            } else {
                emit(out, cntp, 3 as i64, i, 0 as i64);
            }
        }
        i = i + 1 as i64;
    }
    var h: i64 = 0;

    if new_head != n {
        h = new_head;
    }
    emit(out, cntp, 1 as i64, h, 0 as i64);
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, count as u64);
    return true;
}

fn main() {
    run(0);
}

