// Apex KV slot placement in Goose (llama_kv_cache::find_slot, the walk).
//
// The per-cell predicate is already Goose (fg199k_can_use). What moves here is the walk itself: from
// the current head, test `cont ? n_tokens : 1` cells at a time, wrap at the cache end, and stop as soon
// as n_tokens indices are collected. For a continuous insert a failing cell abandons the group.
//
// The cell contents cross as flat arrays, so nothing container-shaped passes the boundary.
//
// Packet:
//   k=0  0 size, 1 head, 2 n_tokens, 3 cont, 4 n_swa, 5 swa_type
//   k=1  0 pos array, 1 seqcnt array, 2 maxp1 array, 3 out array, 4 cap
//   k=2  0 done, 1 ret
//
// Records: 0 walk (a = size, b = found)
extern "fg392k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg392k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg392k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg392k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg392k_fail" fn fail(code: i64);
extern "fg392k_can" fn can_use(occupied: i64, seq_count: i64, pos_cell: i64, max_p1: i64, n_swa: i64, swa_type: i64) -> i64;

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

fn run(packet: i64) -> bool {
    let size: i64 = get(packet, 0, 0) as i64;
    let head0: i64 = get(packet, 0, 1) as i64;
    let n_tokens: i64 = get(packet, 0, 2) as i64;
    let cont: i64 = get(packet, 0, 3) as i64;
    let n_swa: i64 = get(packet, 0, 4) as i64;
    let swa_type: i64 = get(packet, 0, 5) as i64;
    let posa: u64 = get(packet, 1, 0);
    let cnta: u64 = get(packet, 1, 1);
    let maxa: u64 = get(packet, 1, 2);
    let outp: u64 = get(packet, 1, 3);
    let cap: i64 = get(packet, 1, 4) as i64;
    let cntp: u64 = get(packet, 2, 0);
    ensure(size > (0 as i64), 10);
    ensure(n_tokens > (0 as i64), 11);
    let n_test: i64 = if cont != (0 as i64) {
        n_tokens
    } else {
        1 as i64
    }
    ;
    var head: i64 = head0;
    var n_tested: i64 = 0 as i64;
    var out_n: i64 = 0 as i64;
    var done: i64 = 0 as i64;
    var bad: i64 = 0 as i64;

    while done == (0 as i64) {
        if head + n_test > size {
            n_tested = n_tested + (size - head);
            head = 0 as i64;
        } else {
            var i: i64 = 0 as i64;

            while i < n_test {
                let idx: i64 = head;
                head = head + (1 as i64);
                n_tested = n_tested + (1 as i64);
                let occupied: i64 = if at(cnta, idx) > (0 as i64) {
                    1 as i64
                } else {
                    0 as i64
                }
                ;
                let can: i64 = can_use(occupied, at(cnta, idx), at(posa, idx), at(maxa, idx), n_swa, swa_type);

                if can != (0 as i64) {
                    if out_n < cap {
                        setat(outp, out_n, idx);
                        out_n = out_n + (1 as i64);
                    }
                } else {
                    if cont != (0 as i64) {
                        i = n_test;
                    }
                }
                i = i + (1 as i64);
            }

            if out_n == n_tokens {
                done = 1 as i64;
            } else {
                if cont != (0 as i64) {
                    out_n = 0 as i64;
                }

                if n_tested >= size {
                    done = 1 as i64;
                    bad = 1 as i64;
                }
            }
        }
    }

    if bad != (0 as i64) {
        out_n = (0 as i64) - (1 as i64);
    }

    if out_n >= 0 as i64 {
        if out_n < n_tokens {
            out_n = (0 as i64) - (1 as i64);
        }
    }
    setat(cntp, 0 as i64, 1 as i64);
    put(packet, 2, 1, out_n as u64);
    return true;
}

fn main() {
    run(0);
}

