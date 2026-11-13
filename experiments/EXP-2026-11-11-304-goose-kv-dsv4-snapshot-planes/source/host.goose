// Apex DSV4 rollback planes in Goose: the restore/snapshot tail of dsv4_build_comp_plan.
//
// When per-token snapshots are enabled (n_rs_seq > 0), the plan carries two more index lists:
//
//   restore : copy each stream's state from the pending rollback plane (or plane 0 when no
//             rollback is pending) into the live row,
//   snapshot: for every depth d, copy the live row into plane d, taking the trailing `d` tokens
//             of the sequence from the scratch rows so a rollback can land on the right state.
//
// The tail is emitted for every layout stream, whether or not the ubatch touches it, so the graph
// tensor widths never depend on the batch: absent streams get no-op entries.
//
// Packet, int64 slots:
//   k=0  0 op, 1 n_stream, 2 state_size, 3 n_rs_seq, 4 rs_idx, 5 n_seqs_unq, 6 seq_id_unq,
//        7 n_tokens, 8 pos, 9 n_seq_id, 10 seq_id, 11 width
//   k=1  0 restore_src, 1 restore_dst, 2 snap_src, 3 snap_dst
//   k=3  0 done, 1 ret (count of restore entries; snapshot count = ret*(state_size*n_rs_seq))
extern "fg304k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg304k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg304k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg304k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg304k_fail" fn fail(code: i64);

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

fn token_has_seq(i: i64, seq_id: i64, width: i64, sid: u64, nsid: u64) -> bool {
    let k: i64 = at(nsid, i);
    var j: i64 = 0;

    while j < k {
        if at(sid, i * width + j) == seq_id {
            return true;
        }
        j = j + 1 as i64;
    }
    return false;
}

fn run(packet: i64) -> bool {
    let n_stream: i64 = get(packet, 0, 1) as i64;
    let state_size: i64 = get(packet, 0, 2) as i64;
    let n_rs_seq: i64 = get(packet, 0, 3) as i64;
    let rs_idx: u64 = get(packet, 0, 4);
    let n_seqs_unq: i64 = get(packet, 0, 5) as i64;
    let sid_unq: u64 = get(packet, 0, 6);
    let n_tokens: i64 = get(packet, 0, 7) as i64;
    let posarr: u64 = get(packet, 0, 8);
    let nsid: u64 = get(packet, 0, 9);
    let sid: u64 = get(packet, 0, 10);
    let width: i64 = get(packet, 0, 11) as i64;
    let rs_len: i64 = get(packet, 0, 12) as i64;
    let o_rrs: u64 = get(packet, 1, 0);
    let o_rdd: u64 = get(packet, 1, 1);
    let o_sss: u64 = get(packet, 1, 2);
    let o_sdd: u64 = get(packet, 1, 3);
    ensure(n_stream > (0 as i64) && state_size > (0 as i64), 60);
    ensure(n_rs_seq >= (0 as i64) && width > (0 as i64), 61);
    let state_rows: i64 = state_size * n_stream;
    var cnt_r: i64 = 0;
    var cnt_s: i64 = 0;

    if n_rs_seq > (0 as i64) {
        var stream: i64 = 0;

        while stream < n_stream {
            var seq_id: i64 = (0 as i64) - 1 as i64;

            if n_stream == (1 as i64) {
                if n_seqs_unq > (0 as i64) {
                    seq_id = at(sid_unq, 0);
                }
            } else {
                var s: i64 = 0;

                while s < n_seqs_unq {
                    if at(sid_unq, s) == stream {
                        seq_id = at(sid_unq, s);
                    }
                    s = s + 1 as i64;
                }
            }
            let stream_off: i64 = stream * state_size;
            var rollback: i64 = 0;

            if seq_id >= (0 as i64) {
                if seq_id < rs_len {
                    rollback = at(rs_idx, seq_id);
                }
            }
            var src_plane: i64 = 0;

            if rollback > (0 as i64) {
                if rollback <= n_rs_seq {
                    src_plane = rollback * state_rows;
                }
            }
            var r: i64 = 0;

            while r < state_size {
                setat(o_rrs, cnt_r, src_plane + stream_off + r);
                setat(o_rdd, cnt_r, stream_off + r);
                cnt_r = cnt_r + 1 as i64;
                r = r + 1 as i64;
            }
            // tokens of this sequence, in batch order
            var n_seq_tokens: i64 = 0;
            var i: i64 = 0;

            while i < n_tokens {
                if seq_id >= (0 as i64) {
                    if token_has_seq(i, seq_id, width, sid, nsid) {
                        n_seq_tokens = n_seq_tokens + 1 as i64;
                    }
                }
                i = i + 1 as i64;
            }
            let scratch_off: i64 = state_rows * ( (1 as i64) + n_rs_seq);
            var d: i64 = 1;

            while d <= n_rs_seq {
                let dst_plane: i64 = d * state_rows;
                var prefix: i64 = 0;

                if d <= n_seq_tokens {
                    prefix = n_seq_tokens - d;
                }
                var rr: i64 = 0;

                while rr < state_size {
                    var src: i64 = stream_off + rr;
                    // the trailing `prefix` tokens of the sequence override the live row for their own row
                    var j: i64 = 0;
                    var k: i64 = 0;

                    while j < n_tokens {
                        if seq_id >= (0 as i64) {
                            if token_has_seq(j, seq_id, width, sid, nsid) {
                                if k < prefix {
                                    let p: i64 = at(posarr, j);

                                    if p >= (0 as i64) {
                                        if p % state_size == rr {
                                            src = scratch_off + j;
                                        }
                                    }
                                    k = k + 1 as i64;
                                }
                            }
                        }
                        j = j + 1 as i64;
                    }

                    if n_seq_tokens == (0 as i64) {
                        src = dst_plane + stream_off + rr;
                    }
                    setat(o_sss, cnt_s, src);
                    setat(o_sdd, cnt_s, dst_plane + stream_off + rr);
                    cnt_s = cnt_s + 1 as i64;
                    rr = rr + 1 as i64;
                }
                d = d + 1 as i64;
            }
            stream = stream + 1 as i64;
        }
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, cnt_r as u64);
    return true;
}

fn main() {
    run(0);
}

