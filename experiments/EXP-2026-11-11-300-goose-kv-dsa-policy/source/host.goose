// Apex DSA KV cache policy in Goose (llama_kv_cache_dsa).
//
// The DSA cache is the MLA cache plus a lightning-indexer cache built from derived
// hyperparameters: one key head per layer (MQA), the key head width replaced by the indexer head
// size, and rope forced to NeoX. Two more rules are DSA-specific and are the reason this module
// exists separately from the MSA one:
//
//   can_shift = mla.can_shift && lid.can_shift && mla.size == lid.size
//   state_read is a transaction: the MLA part is read first, and if the indexer read throws, the
//   MLA part is cleared again so a failed restore leaves nothing behind.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 n_layer, 3 arr_in, 4 indexer_head_size, 5 unified, 6 n_seq_max,
//        7 filter_mla, 8 filter_lid, 9 a, 10 b
//   k=3  0 done, 1 ret
extern "fg300k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg300k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg300k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg300k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg300k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(arr: u64, i: i64) -> i64 {
    return ld(arr, i * (8 as i64));
}

fn neox() -> i64 {
    return 2 as i64;
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let out: u64 = get(packet, 0, 1);
    let n_layer: i64 = get(packet, 0, 2) as i64;
    let arr_in: u64 = get(packet, 0, 3);
    let idx_head: i64 = get(packet, 0, 4) as i64;
    let unified: bool = get(packet, 0, 5) != (0 as u64);
    let n_seq_max: i64 = get(packet, 0, 6) as i64;
    let filt_mla: u64 = get(packet, 0, 7);
    let filt_lid: u64 = get(packet, 0, 8);
    let a: i64 = get(packet, 0, 9) as i64;
    let b: i64 = get(packet, 0, 10) as i64;
    ensure(n_layer >= (0 as i64), 60);
    var ret: i64 = 0;

    if op == (0 as i64) {
        var il: i64 = 0;

        while il < n_layer {
            st(out, il * (8 as i64), 1 as i64);
            il = il + 1 as i64;
        }
        st(out, (n_layer * (8 as i64)), idx_head);
        st(out, ( (n_layer + (1 as i64)) * (8 as i64)), neox());
        var n_stream: i64 = n_seq_max;

        if unified {
            n_stream = 1 as i64;
        }
        st(out, ( (n_layer + (2 as i64)) * (8 as i64)), n_stream);
        ret = 1 as i64;
    } else {
        if op == (1 as i64) {
            var il: i64 = 0;

            while il < n_layer {
                st(out, (2 as i64 * il) * (8 as i64), at(filt_mla, il));
                st(out, (2 as i64 * il + 1 as i64) * (8 as i64), at(filt_lid, il));
                il = il + 1 as i64;
            }
            ret = 1 as i64;
        } else {
            if op == (2 as i64) {
                // seq_rm succeeds only when both caches do

                if a != (0 as i64) && b != (0 as i64) {
                    ret = 1 as i64;
                }
            } else {
                if op == (3 as i64) {
                    // position observers read the MLA cache (a), not the indexer (b)
                    ret = a;
                } else {
                    if op == (4 as i64) {
                        // can_shift: both caches shift and their sizes match
                        let mla_shift: i64 = get(packet, 0, 9) as i64;
                        let lid_shift: i64 = get(packet, 0, 10) as i64;
                        let mla_size: i64 = get(packet, 0, 11) as i64;
                        let lid_size: i64 = get(packet, 0, 12) as i64;

                        if mla_shift != (0 as i64) && lid_shift != (0 as i64) && mla_size == lid_size {
                            ret = 1 as i64;
                        }
                    } else {
                        if op == (5 as i64) {
                            // state_read transaction: a = lid read throws
                            let lid_fail: i64 = a;
                            var mla_read: i64 = 1 as i64;
                            var lid_read: i64 = 1 as i64;
                            var mla_cleared: i64 = 0 as i64;

                            if lid_fail != (0 as i64) {
                                lid_read = 0 as i64;
                                mla_cleared = 1 as i64;
                            }
                            st(out, 0 as i64, mla_read);
                            st(out, (1 as i64) * (8 as i64), lid_read);
                            st(out, (2 as i64) * (8 as i64), mla_cleared);
                            ret = 1 as i64;
                        } else {
                            ensure(false, 61);
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

