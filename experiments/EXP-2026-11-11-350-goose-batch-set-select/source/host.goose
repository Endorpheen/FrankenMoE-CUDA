// Apex batch sequence-set selection in Goose (llama_batch_allocr::split_equal, opening block).
//
// Before any token is handed out, the split decides which sequence sets take part:
//
//   a row this split already used is skipped;
//   a row whose sequence set overlaps a set already chosen is skipped — one ubatch never carries two
//   overlapping sets;
//   in sequential mode only a set whose first sequence id is exactly one past the last accepted id is
//   accepted;
//   the scan stops as soon as **more than** `n_ubatch` sets are collected, so it can take
//   `n_ubatch + 1` sets before breaking.
//
// If nothing was collected the ubatch is empty. Otherwise each set's cursor starts at zero and moves
// past rows this split already used, so the expansion loop never re-takes a row — and it stops at the
// first unused row, not at the end.
//
// Packet:
//   k=0  0 n_tokens, 1 used, 2 seq_set, 3 seq_first, 4 n_ubatch, 5 set_map, 6 set_off, 7 n_sets
//   k=1  0 out, 1 cnt, 2 sequential, 3 chosen (scratch, one mask per accepted row), 4 chosen_max
//   k=2  0 done, 1 ret (n_seqs, 0 when empty)
//
// Records: 0 accepted (a = row, b = bitmask), 1 refused overlap (a = row), 2 refused order (a = row),
// 3 refused used (a = row), 4 cursor (a = set, b = index), 5 final (a = n_seqs).
extern "fg350k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg350k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg350k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg350k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg350k_fail" fn fail(code: i64);

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
    let n_tokens: i64 = get(packet, 0, 0) as i64;
    let used: u64 = get(packet, 0, 1);
    let seq_set: u64 = get(packet, 0, 2);
    let seq_first: u64 = get(packet, 0, 3);
    let n_ubatch: i64 = get(packet, 0, 4) as i64;
    let set_map: u64 = get(packet, 0, 5);
    let set_off: u64 = get(packet, 0, 6);
    let n_sets: i64 = get(packet, 0, 7) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 1, 1);
    let sequential: i64 = get(packet, 1, 2) as i64;
    let chosen_buf: u64 = get(packet, 1, 3);
    let chosen_max: i64 = get(packet, 1, 4) as i64;
    ensure(n_tokens >= (0 as i64) && n_sets >= (0 as i64), 60);
    ensure(chosen_max >= n_ubatch + (1 as i64), 61);
    var chosen: i64 = 0 as i64;
    var last_seq: i64 = (0 as i64) - 1 as i64;
    var stop: i64 = 0 as i64;
    var i: i64 = 0;

    while i < n_tokens && stop == (0 as i64) {
        if at(used, i) != (0 as i64) {
            emit(out, cntp, 3 as i64, i, 0 as i64);
            i = i + 1 as i64;
        } else {
            let ss: i64 = at(seq_set, i);
            var add: i64 = 1 as i64;
            var c: i64 = 0;

            while c < chosen {
                let cs: i64 = at(chosen_buf, c);

                if (cs & ss) != (0 as i64) {
                    add = 0 as i64;
                }
                c = c + 1 as i64;
            }

            if add == (0 as i64) {
                emit(out, cntp, 1 as i64, i, 0 as i64);
                i = i + 1 as i64;
            } else {
                if sequential != (0 as i64) {
                    if chosen != (0 as i64) {
                        if at(seq_first, i) != last_seq + (1 as i64) {
                            add = 0 as i64;
                        }
                    }
                }

                if add == (0 as i64) {
                    emit(out, cntp, 2 as i64, i, 0 as i64);
                    i = i + 1 as i64;
                } else {
                    emit(out, cntp, 0 as i64, i, ss);
                    setat(chosen_buf, chosen, ss);
                    chosen = chosen + (1 as i64);
                    last_seq = at(seq_first, i);

                    if chosen > n_ubatch {
                        stop = 1 as i64;
                    }
                    i = i + 1 as i64;
                }
            }
        }
    }
    let n_seqs: i64 = chosen;
    emit(out, cntp, 5 as i64, n_seqs, 0 as i64);

    if n_seqs == (0 as i64) {
        put(packet, 2, 0, 1 as u64);
        put(packet, 2, 1, 0 as i64 as u64);
        return true;
    }
    var s: i64 = 0;

    while s < n_seqs {
        let cntset: i64 = at(set_off, s + (1 as i64)) - at(set_off, s);
        var cur: i64 = 0 as i64;
        var go: i64 = 1 as i64;

        while cur < cntset && go != (0 as i64) {
            let row: i64 = at(set_map, at(set_off, s) + cur);

            if at(used, row) == (0 as i64) {
                go = 0 as i64;
            } else {
                cur = cur + (1 as i64);
            }
        }
        emit(out, cntp, 4 as i64, s, cur);
        s = s + 1 as i64;
    }
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, n_seqs as u64);
    return true;
}

fn main() {
    run(0);
}

