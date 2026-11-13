// Apex hybrid indexed memory: state ordering and the restore transaction
// (llama_memory_hybrid_idx::{state_write, state_read, state_drop} and the ubatch stream count).
//
// The indexer section is a pure suffix of the stream, so an older reader stops early instead of
// misparsing it. On restore the order is mirrored: attention first (it hands back the layout the
// indexer adopts instead of searching for cells of its own, because two searches agree only while
// both caches see the same occupancy), then the recurrent part, then the indexer. A restore that
// rewrote cells invalidates the pool layout, and a failure anywhere drops the sequence from all
// three caches — the one state they agree on — before the error propagates.
//
// state_drop clears directly instead of going through seq_rm, because the recurrent cache may refuse
// and then only the other two would be cleared.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 a, 3 b, 4 c, 5 d, 6 n, 7 arr, 8 arr2
//   k=3  0 done, 1 ret
extern "fg311k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg311k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg311k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg311k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg311k_fail" fn fail(code: i64);

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

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let out: u64 = get(packet, 0, 1);
    let a: i64 = get(packet, 0, 2) as i64;
    let b: i64 = get(packet, 0, 3) as i64;
    let c: i64 = get(packet, 0, 4) as i64;
    let d: i64 = get(packet, 0, 5) as i64;
    let n: i64 = get(packet, 0, 6) as i64;
    let arr: u64 = get(packet, 0, 7);
    let arr2: u64 = get(packet, 0, 8);
    var ret: i64 = 0;

    if op == (0 as i64) {
        // state_write: base first, indexer last (1 = indexer present)
        setat(out, 0 as i64, 0 as i64);
        // base hybrid section
        var k: i64 = 1;

        if a != (0 as i64) {
            setat(out, 1 as i64, 1 as i64);
            k = 2;
        }
        ret = k;
    } else {
        if op == (1 as i64) {
            // state_read: a = attn fails, b = recr fails, c = idx fails, d = has_idx, arr = sinfos
            // (0 = empty layout returned by the indexer restore)
            var ne: i64 = 0;
            var failed: bool = false;

            if a != (0 as i64) {
                failed = true;
            } else {
                setat(out, ne, 0 as i64);
                ne = ne + 1 as i64;

                if b != (0 as i64) {
                    failed = true;
                } else {
                    setat(out, ne, 1 as i64);
                    ne = ne + 1 as i64;

                    if d != (0 as i64) {
                        if c != (0 as i64) {
                            failed = true;
                        } else {
                            setat(out, ne, 2 as i64);
                            ne = ne + 1 as i64;

                            if at(arr, 0) != 0 as i64 {
                                setat(out, ne, 3 as i64);
                                ne = ne + 1 as i64;
                            }
                        }
                    }
                }
            }

            if failed {
                setat(out, ne, 4 as i64);
                ne = ne + 1 as i64;
                ret = (0 as i64) - 1 as i64;
            } else {
                ret = ne;
            }
        } else {
            if op == (2 as i64) {
                // state_drop: seq_id < 0 clears everything; otherwise attn clear, recr rm, idx clear + stale
                let seq_id: i64 = a;

                if seq_id < (0 as i64) {
                    setat(out, 0 as i64, 7 as i64);
                    // clear(true)
                    ret = 1 as i64;
                } else {
                    var ne: i64 = 0;
                    setat(out, ne, 0 as i64);
                    ne = ne + 1 as i64;
                    // attn state_clear
                    setat(out, ne, 1 as i64);
                    ne = ne + 1 as i64;
                    // recr seq_rm(-1,-1)

                    if b != (0 as i64) {
                        setat(out, ne, 2 as i64);
                        ne = ne + 1 as i64;
                        // idx state_clear
                        setat(out, ne, 3 as i64);
                        ne = ne + 1 as i64;
                        // stale_set(seq, 0)
                    }
                    ret = ne;
                }
            } else {
                if op == (3 as i64) {
                    // ubatch stream counts: s1 - s0 + 1
                    var i: i64 = 0;

                    while i < n {
                        let s0: i64 = at(arr, i);
                        let s1: i64 = at(arr2, i);
                        setat(out, i, s1 - s0 + (1 as i64));
                        i = i + 1 as i64;
                    }
                    ret = n;
                } else {
                    ensure(false, 60);
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

