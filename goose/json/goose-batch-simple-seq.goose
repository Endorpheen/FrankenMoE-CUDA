// Apex simple and sequential batch splits in Goose
// (llama_batch_allocr::split_simple, llama_batch_allocr::split_seq).
//
// Both start by locating the first unused token; finding none means "no ubatch".
//
// split_simple: take that token and then every following token in order, marking each used, until the
// token list is exhausted or the ubatch reaches its size. The size test runs **after** the token was
// added, so a full ubatch is exactly `n_ubatch` tokens — and because the walk starts at the first
// unused index rather than at zero, a gap in the middle does not stop it.
//
// split_seq: a ubatch holds tokens of one sequence set and of its subsets. After each token the scan
// advances past used tokens and past tokens whose set is **not** a subset of the current one
// (`(cur & s) != s`); the set is then replaced by the set of the token it landed on. The size test runs
// **before** that scan, so a reached limit stops without looking ahead.
//
// The returned ubatch differs in its `equal_seqs` flag and in what counts as the number of sequences:
// the simple split reports one sequence per token, the sequential split reports a single sequence.
//
// Packet:
//   k=0  0 op, 1 n_tokens, 2 used, 3 seq_set, 4 n_ubatch
//   k=1  0 out, 1 cnt
//   k=2  0 done, 1 ret (token count, -1 when empty because no token is free)
//
// Records: 0 taken (a = token index), 1 final (a = n_tokens in the ubatch), 2 used written (a = index),
// 3 empty (a = 0 when everything was used).
extern "fg351k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg351k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg351k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg351k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg351k_fail" fn fail(code: i64);

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
    let op: i64 = get(packet, 0, 0) as i64;
    let n_tokens: i64 = get(packet, 0, 1) as i64;
    let used: u64 = get(packet, 0, 2);
    let seq_set: u64 = get(packet, 0, 3);
    let n_ubatch: i64 = get(packet, 0, 4) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 1, 1);
    ensure(n_tokens >= (0 as i64), 60);
    var cur: i64 = 0;

    while cur < n_tokens && at(used, cur) != (0 as i64) {
        cur = cur + (1 as i64);
    }

    if cur >= n_tokens {
        emit(out, cntp, 3 as i64, 0 as i64, 0 as i64);
        put(packet, 2, 0, 1 as u64);
        put(packet, 2, 1, ( (0 as i64) - 1 as i64) as u64);
        return true;
    }
    var count: i64 = 0 as i64;

    if op == (0 as i64) {
        // simple: consecutive tokens from the first free one
        var go: i64 = 1 as i64;

        while go != (0 as i64) {
            emit(out, cntp, 0 as i64, cur, 0 as i64);
            setat(used, cur, 1 as i64);
            count = count + (1 as i64);
            cur = cur + (1 as i64);

            if cur >= n_tokens {
                go = 0 as i64;
            } else {
                if count >= n_ubatch {
                    go = 0 as i64;
                }
            }
        }
    } else {
        // sequential: stay inside one sequence set and its subsets
        var cur_set: i64 = at(seq_set, cur);
        var go2: i64 = 1 as i64;

        while go2 != (0 as i64) {
            emit(out, cntp, 0 as i64, cur, 0 as i64);
            setat(used, cur, 1 as i64);
            count = count + (1 as i64);

            if count >= n_ubatch {
                go2 = 0 as i64;
            } else {
                var scan: i64 = 1 as i64;

                while scan != (0 as i64) {
                    cur = cur + (1 as i64);

                    if cur >= n_tokens {
                        scan = 0 as i64;
                    } else {
                        let s: i64 = at(seq_set, cur);

                        if at(used, cur) != (0 as i64) {
                            scan = 1 as i64;
                        } else {
                            if (cur_set & s) != s {
                                scan = 1 as i64;
                            } else {
                                scan = 0 as i64;
                            }
                        }
                    }
                }

                if cur >= n_tokens {
                    go2 = 0 as i64;
                } else {
                    cur_set = at(seq_set, cur);
                }
            }
        }
    }
    emit(out, cntp, 1 as i64, count, 0 as i64);
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, count as u64);
    return true;
}

fn main() {
    run(0);
}

