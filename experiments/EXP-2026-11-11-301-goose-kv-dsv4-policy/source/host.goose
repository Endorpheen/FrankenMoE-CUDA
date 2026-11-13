// Apex DSV4 KV cache policy in Goose (llama-kv-cache-dsv4.cpp, policy half).
//
// The dsv4 cache adds a compressed state beside the ordinary KV cache. This module transfers the
// decisions that shape that state and the graph that touches it:
//
//   - compressed size and used key rows,
//   - the per-sequence stream offset, with the native range rejection,
//   - coupled-sequence detection and per-token sequence membership,
//   - how many graph streams the compressed state may fan out to (coupled sequence sets share
//     their compressed state, so they must stay in one stream),
//   - the K-only hyperparameter tweak, and the plane count of a state tensor.
//
// The native stream-offset function throws on an out-of-range sequence. An FFI entry cannot
// throw, so the contract here is -1 for that case, documented rather than hidden.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 a, 3 b, 4 c, 5 d, 6 arr, 7 n, 8 width
//   k=3  0 done, 1 ret
extern "fg301k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg301k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg301k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg301k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg301k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(arr: u64, i: i64) -> i64 {
    return ld(arr, i * (8 as i64));
}
// max(1, ceil(kv_size / ratio))

fn comp_size(kv: i64, ratio: i64) -> i64 {
    ensure(ratio > (0 as i64), 60);
    let r: i64 = (kv + ratio - (1 as i64)) / ratio;

    if r < (1 as i64) {
        return 1 as i64;
    }
    return r;
}
// (pos_max + 1) / ratio, clamped to kv_size; zero when the sequence is empty.

fn n_used_k_rows(pos_max: i64, ratio: i64, kv: i64) -> i64 {
    if pos_max < (0 as i64) {
        return 0 as i64;
    }
    ensure(ratio > (0 as i64), 61);
    let rows: i64 = (pos_max + (1 as i64)) / ratio;

    if rows < kv {
        return rows;
    }
    return kv;
}
// Native throws when the sequence is out of the stream range; -1 is the FFI form of that.

fn stream_offset(n_stream: i64, seq_id: i64, size: i64) -> i64 {
    if n_stream <= (1 as i64) {
        return 0 as i64;
    }

    if seq_id < (0 as i64) {
        return (0 as i64) - 1 as i64;
    }

    if seq_id >= n_stream {
        return (0 as i64) - 1 as i64;
    }
    return seq_id * size;
}
// Coupled sequence sets: any token that belongs to more than one sequence.

fn ubatch_has_coupled(cnt: i64, nsid: u64) -> i64 {
    var i: i64 = 0;

    while i < cnt {
        if at(nsid, i) > 1 as i64 {
            return 1 as i64;
        }
        i = i + 1 as i64;
    }
    return 0 as i64;
}

fn token_has_seq(i: i64, seq_id: i64, width: i64, sid: u64, nsid: u64) -> i64 {
    let k: i64 = at(nsid, i);
    var j: i64 = 0;

    while j < k {
        if at(sid, i * width + j) == seq_id {
            return 1 as i64;
        }
        j = j + 1 as i64;
    }
    return 0 as i64;
}
// One graph stream when there is nothing to fan out or when any set is coupled.

fn comp_graph_n_stream(n_stream: i64, n_seqs_unq: i64, coupled: i64) -> i64 {
    if n_stream <= (1 as i64) {
        return 1 as i64;
    }

    if n_seqs_unq <= (1 as i64) {
        return 1 as i64;
    }

    if coupled != (0 as i64) {
        return 1 as i64;
    }
    return n_seqs_unq;
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let out: u64 = get(packet, 0, 1);
    let a: i64 = get(packet, 0, 2) as i64;
    let b: i64 = get(packet, 0, 3) as i64;
    let c: i64 = get(packet, 0, 4) as i64;
    let d: i64 = get(packet, 0, 5) as i64;
    let arr: u64 = get(packet, 0, 6);
    let n: i64 = get(packet, 0, 7) as i64;
    let width: i64 = get(packet, 0, 8) as i64;
    var ret: i64 = 0;

    if op == (0 as i64) {
        ret = comp_size(a, b);
    } else {
        if op == (1 as i64) {
            ret = n_used_k_rows(a, b, c);
        } else {
            if op == (2 as i64) {
                ret = stream_offset(a, b, c);
            } else {
                if op == (3 as i64) {
                    ret = ubatch_has_coupled(n, arr);
                } else {
                    if op == (4 as i64) {
                        ret = token_has_seq(a, b, width, arr, get(packet, 0, 9));
                    } else {
                        if op == (5 as i64) {
                            ret = comp_graph_n_stream(a, b, c);
                        } else {
                            if op == (6 as i64) {
                                // dsv4_make_k_only: both MLA head widths become the key head width
                                st(out, 0 as i64, a);
                                st(out, (1 as i64) * (8 as i64), a);
                                ret = 1 as i64;
                            } else {
                                if op == (7 as i64) {
                                    // state planes: n_stream * (1 + n_rs_seq)
                                    ret = a * ( (1 as i64) + b);
                                } else {
                                    ensure(false, 62);
                                }
                            }
                        }
                    }
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

