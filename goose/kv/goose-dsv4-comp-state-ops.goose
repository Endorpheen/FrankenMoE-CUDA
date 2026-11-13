// Apex DSV4 compressed-state operations in Goose:
// llama_dsv4_comp_state::{clear, seq_cp, apply_copies} plus the stream-range helpers and the
// stream ids that state_write computes.
//
// The compressed state holds one plane per stream and per rollback depth: plane index
// d*n_stream + stream, for d in [0, n_rs_seq]. Clearing one sequence clears its column in every
// depth; clearing everything clears the buffers. A sequence copy clears the destination column and
// queues a copy, and applying the queue copies each source column onto its destination in order.
//
// The native helpers throw on mismatched or out-of-range arguments. An FFI entry cannot throw, so
// the contract here is -1 for a rejection, stated and pinned by the gate.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 a, 3 b, 4 c, 5 d, 6 arr, 7 n, 8 arr2, 9 n2
//   k=3  0 done, 1 ret
extern "fg305k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg305k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg305k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg305k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg305k_fail" fn fail(code: i64);

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
    let arr: u64 = get(packet, 0, 6);
    let n: i64 = get(packet, 0, 7) as i64;
    let arr2: u64 = get(packet, 0, 8);
    let n2: i64 = get(packet, 0, 9) as i64;
    var ret: i64 = 0;

    if op == (0 as i64) {
        // src_stream_range(n_stream, seq_id) -> (s0, ns)
        let n_stream: i64 = a;
        let seq_id: i64 = b;
        var s0: i64 = 0;
        var ns: i64 = 0;

        if seq_id >= (0 as i64) {
            if n_stream > (1 as i64) {
                if seq_id >= n_stream {
                    ret = (0 as i64) - 1 as i64;
                } else {
                    s0 = seq_id;
                    ns = 1 as i64;
                    ret = 1 as i64;
                }
            } else {
                s0 = 0;
                ns = 1 as i64;
                ret = 1 as i64;
            }
        } else {
            s0 = 0;
            ns = n_stream;
            ret = 1 as i64;
        }
        setat(out, 0 as i64, s0);
        setat(out, 1 as i64, ns);
    } else {
        if op == (1 as i64) {
            // dst_stream_range(n_stream, seq_id, ns) -> s0
            let n_stream: i64 = a;
            let seq_id: i64 = b;
            let ns: i64 = c;
            var s0: i64 = 0;

            if seq_id >= (0 as i64) {
                if ns != (1 as i64) {
                    ret = (0 as i64) - 1 as i64;
                } else {
                    if n_stream > (1 as i64) {
                        if seq_id >= n_stream {
                            ret = (0 as i64) - 1 as i64;
                        } else {
                            s0 = seq_id;
                            ret = 1 as i64;
                        }
                    } else {
                        s0 = 0;
                        ret = 1 as i64;
                    }
                }
            } else {
                if ns != n_stream {
                    ret = (0 as i64) - 1 as i64;
                } else {
                    s0 = 0;
                    ret = 1 as i64;
                }
            }
            setat(out, 0 as i64, s0);
        } else {
            if op == (2 as i64) {
                // clear(seq_id, data): the plane ids of one sequence column, or every plane
                let n_stream: i64 = a;
                let seq_id: i64 = b;
                let n_rs_seq: i64 = c;
                let want_data: bool = d != (0 as i64);

                if !want_data {
                    ret = 1 as i64;
                } else {
                    if seq_id >= (0 as i64) {
                        if seq_id >= n_stream {
                            fail(60);
                        }
                        var cnt: i64 = 0;
                        var dd: i64 = 0;

                        while dd <= n_rs_seq {
                            setat(out, cnt, dd * n_stream + seq_id);
                            cnt = cnt + 1 as i64;
                            dd = dd + 1 as i64;
                        }
                        ret = cnt;
                    } else {
                        // whole buffers: planes = n_stream*(1+n_rs_seq)
                        ret = n_stream * ( (1 as i64) + n_rs_seq);
                    }
                }
            } else {
                if op == (3 as i64) {
                    // seq_cp(src, dst): 0 = no-op, 1 = clear the destination column and queue the copy
                    let n_stream: i64 = a;
                    let src: i64 = b;
                    let dst: i64 = c;

                    if src < (0 as i64) {
                        fail(61);
                    }

                    if dst < (0 as i64) {
                        fail(62);
                    }

                    if src >= n_stream {
                        fail(63);
                    }

                    if dst >= n_stream {
                        fail(64);
                    }

                    if src == dst {
                        ret = 0 as i64;
                    } else {
                        setat(out, 0 as i64, src);
                        setat(out, 1 as i64, dst);
                        ret = 1 as i64;
                    }
                } else {
                    if op == (4 as i64) {
                        // apply_copies: copy each source column onto its destination, in queue order
                        var i: i64 = 0;

                        while i < n {
                            setat(out, (2 as i64 * i), at(arr2, i));
                            // dst
                            setat(out, (2 as i64 * i + 1 as i64), at(arr, i));
                            // src
                            i = i + 1 as i64;
                        }
                        ret = n;
                    } else {
                        if op == (5 as i64) {
                            // state_write stream ids: rs_idx[seq]*n_stream + s0 + s, rejected when out of range
                            let n_stream: i64 = a;
                            let seq_id: i64 = b;
                            let n_rs_seq: i64 = c;
                            let s0: i64 = d;
                            let ns: i64 = n2;
                            let rs: u64 = arr;
                            let rs_len: i64 = n;
                            var ok: bool = true;
                            var i: i64 = 0;

                            while i < ns && ok {
                                var seq: i64 = s0 + i;

                                if seq_id >= (0 as i64) {
                                    seq = seq_id;
                                }

                                if seq >= rs_len {
                                    ok = false;
                                } else {
                                    let rb: i64 = at(rs, seq);

                                    if rb > n_rs_seq {
                                        ok = false;
                                    } else {
                                        setat(out, i, rb * n_stream + s0 + i);
                                    }
                                }
                                i = i + 1 as i64;
                            }

                            if ok {
                                ret = ns;
                            } else {
                                ret = (0 as i64) - 1 as i64;
                            }
                        } else {
                            ensure(false, 65);
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

