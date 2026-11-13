// Apex KV state write range collection in Goose (llama_kv_cache::state_write).
//
// A cell is written when it is non-empty and holds the requested sequence (or any sequence, when the
// request is -1), and — only for a real sequence — when the cell is **not SWA-masked** relative to that
// sequence's maximum position. A whole-cache write therefore never applies the SWA filter, which is
// why a partial write of a single sequence can be smaller than the whole.
//
// The selected cells are then coalesced into contiguous ranges: a run opens at the first selected cell
// and closes just before the first unselected one, or at the end of the stream. The section carries the
// stream count first, then per stream the cell count, and the stream is skipped entirely when that
// count is zero — no ranges, no meta, no data.
//
// Masking rules (native, same shape as the find_slot/attention-mask use):
//   none       never masked
//   standard   masked when p1 - p0 >= n_swa
//   chunked    masked when p0 falls before the chunk that p1 sits in, `(p1 / n_swa) * n_swa`
//   symmetric  masked when |p1 - p0| > n_swa / 2, with the half computed in integers
//
// Packet:
//   k=0  0 op, 1 other, 2 partial, 3 swa_none, 4 seq_id, 5 n_stream, 6 n_swa, 7 swa_type, 8 size,
//        9 pos, 10 seqmask
//   k=1  0 out, 1 cnt
//   k=2  0 done, 1 ret
//
// Records: 0 stream header skipped, 1 stream count written (a = n_stream), 2 cell count (a = stream,
// b = count), 3 range (a = from, b = to), 4 meta written (a = stream, b = count), 5 data written,
// 6 stream skipped.
extern "fg345k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg345k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg345k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg345k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg345k_fail" fn fail(code: i64);

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

fn masked(n_swa: i64, swa_type: i64, p0: i64, p1: i64) -> i64 {
    if swa_type == (0 as i64) {
        return 0 as i64;
    }

    if swa_type == (1 as i64) {
        if p1 - p0 >= n_swa {
            return 1 as i64;
        }
        return 0 as i64;
    }

    if swa_type == (2 as i64) {
        let start: i64 = (p1 / n_swa) * n_swa;

        if p0 < start {
            return 1 as i64;
        }
        return 0 as i64;
    }
    // symmetric
    let half: i64 = n_swa / (2 as i64);
    let diff: i64 = p1 - p0;

    if diff < (0 as i64) - half {
        return 1 as i64;
    }

    if diff > half {
        return 1 as i64;
    }
    return 0 as i64;
}

fn run(packet: i64) -> bool {
    let other: i64 = get(packet, 0, 1) as i64;
    let partial: i64 = get(packet, 0, 2) as i64;
    let swa_none: i64 = get(packet, 0, 3) as i64;
    let seq_id: i64 = get(packet, 0, 4) as i64;
    let n_stream: i64 = get(packet, 0, 5) as i64;
    let n_swa: i64 = get(packet, 0, 6) as i64;
    let swa_type: i64 = get(packet, 0, 7) as i64;
    let size: i64 = get(packet, 0, 8) as i64;
    let pos: u64 = get(packet, 0, 9);
    let seqmask: u64 = get(packet, 0, 10);
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 1, 1);
    ensure(n_stream >= (0 as i64) && size >= (0 as i64), 60);

    if other != (0 as i64) {
        done_marker(packet, 0 as i64);
        return true;
    }

    if partial != (0 as i64) && swa_none != (0 as i64) {
        done_marker(packet, 0 as i64);
        return true;
    }
    emit(out, cntp, 1 as i64, n_stream, 0 as i64);
    var s: i64 = 0;

    while s < n_stream {
        let base: i64 = s * size;
        // the sequence's maximum position within this stream, for the SWA filter
        var smax: i64 = (0 as i64) - 1 as i64;

        if seq_id != (0 as i64) - 1 as i64 {
            var q: i64 = 0;

            while q < size {
                if (at(seqmask, base + q) & ( (1 as i64) << seq_id)) != (0 as i64) {
                    if at(pos, base + q) > smax {
                        smax = at(pos, base + q);
                    }
                }
                q = q + 1 as i64;
            }
        }
        var count: i64 = 0;
        var begin: i64 = (0 as i64) - 1 as i64;
        var i: i64 = 0;

        while i < size {
            let p: i64 = at(pos, base + i);
            var add: i64 = 0 as i64;

            if p != (0 as i64) - 1 as i64 {
                if seq_id == (0 as i64) - 1 as i64 {
                    add = 1 as i64;
                } else {
                    if (at(seqmask, base + i) & ( (1 as i64) << seq_id)) != (0 as i64) {
                        add = 1 as i64;
                    }
                }
            }

            if add != (0 as i64) {
                if seq_id != (0 as i64) - 1 as i64 {
                    if masked(n_swa, swa_type, p, smax) != (0 as i64) {
                        add = 0 as i64;
                    }
                }
            }

            if add != (0 as i64) {
                count = count + 1 as i64;

                if begin < (0 as i64) {
                    begin = i;
                }
            } else {
                if begin >= (0 as i64) {
                    emit(out, cntp, 3 as i64, begin, i);
                    begin = (0 as i64) - 1 as i64;
                }
            }
            i = i + 1 as i64;
        }

        if begin >= (0 as i64) {
            emit(out, cntp, 3 as i64, begin, size);
        }
        emit(out, cntp, 2 as i64, s, count);

        if count == (0 as i64) {
            emit(out, cntp, 6 as i64, s, 0 as i64);
        } else {
            emit(out, cntp, 4 as i64, s, count);
            emit(out, cntp, 5 as i64, s, 0 as i64);
        }
        s = s + 1 as i64;
    }
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, 0 as i64 as u64);
    return true;
}

fn done_marker(packet: i64, code: i64) {
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, code as u64);
}

fn main() {
    run(0);
}

