// Apex MSA KV cache policy in Goose (llama_kv_cache_msa).
//
// The MSA cache is also a pair: the main cache over the model's layers and an indexer cache over
// the layers chosen by a separate filter. The indexer runs with derived hyperparameters — one key
// head per layer, and the key head width replaced by the indexer head size, with the rope
// parameters left identical to the main cache.
//
// The delegation rules differ from the iswa pair in one place: the position observers read the
// main cache, not the indexer (iswa reads the SWA cache because the base is a superset). That
// contrast is part of what this module pins down.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 n_layer, 3 arr_in, 4 indexer_head_size, 5 unified, 6 n_seq_max,
//        7 filter, 8 filter_idx, 9 a, 10 b
//   k=3  0 done, 1 ret
extern "fg299k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg299k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg299k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg299k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg299k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(arr: u64, i: i64) -> i64 {
    return ld(arr, i * (8 as i64));
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let out: u64 = get(packet, 0, 1);
    let n_layer: i64 = get(packet, 0, 2) as i64;
    let arr_in: u64 = get(packet, 0, 3);
    let idx_head: i64 = get(packet, 0, 4) as i64;
    let unified: bool = get(packet, 0, 5) != (0 as u64);
    let n_seq_max: i64 = get(packet, 0, 6) as i64;
    let filt: u64 = get(packet, 0, 7);
    let filt_idx: u64 = get(packet, 0, 8);
    let a: i64 = get(packet, 0, 9) as i64;
    let b: i64 = get(packet, 0, 10) as i64;
    ensure(n_layer >= (0 as i64), 60);
    var ret: i64 = 0;

    if op == (0 as i64) {
        // derived hyperparameters for the indexer cache
        var il: i64 = 0;

        while il < n_layer {
            st(out, il * (8 as i64), 1 as i64);
            il = il + 1 as i64;
        }
        st(out, (n_layer * (8 as i64)), idx_head);
        var n_stream: i64 = n_seq_max;

        if unified {
            n_stream = 1 as i64;
        }
        st(out, ( (n_layer + (1 as i64)) * (8 as i64)), n_stream);
        ret = 1 as i64;
    } else {
        if op == (1 as i64) {
            // The two filters are independent: the main cache gets `filter`, the indexer `filter_idx`.
            var il: i64 = 0;

            while il < n_layer {
                st(out, (2 as i64 * il) * (8 as i64), at(filt, il));
                st(out, (2 as i64 * il + 1 as i64) * (8 as i64), at(filt_idx, il));
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
                    // position observers read the main cache (a), never the indexer (b)
                    ret = a;
                } else {
                    ensure(false, 61);
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

