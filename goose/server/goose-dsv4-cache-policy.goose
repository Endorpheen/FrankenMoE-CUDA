// Apex DSV4 cache construction policy in Goose (llama_kv_cache_dsv4 constructor).
//
// The DSV4 cache is five pieces: a raw K-only KV cache, CSA and HCA compressed caches, a lightning
// indexer cache and three compressor states. This module transfers the decisions that shape them:
//
//   - every hparams copy has n_layer_nextn cleared;
//   - the raw cache stays per-sequence even when the public mode is unified (unified_raw = false),
//     and so do the compressed caches (unified_compressed = false);
//   - the raw, CSA, HCA and indexer copies are made K-only by setting both MLA head widths to the
//     key head width;
//   - the indexer runs one key head per layer at the indexer head size, on NeoX rope, and also
//     fixes the SWA head widths;
//   - the CSA filter keeps layers whose compress ratio is 4, the HCA filter keeps 128, the raw
//     filter is the external one alone;
//   - compressed cache sizes are PAD(comp_size(kv, ratio), 256);
//   - compressor states are (ratio 4, state 8, 2*head_k), (128, 128, head_k) and
//     (4, 8, 2*indexer_head).
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 n_layer, 3 head_k, 4 indexer_head, 5 ratios, 6 ext_filter, 7 ext_present,
//        8 kv_size, 9 n_rs_seq
//   k=3  0 done, 1 ret
extern "fg308k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg308k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg308k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg308k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg308k_fail" fn fail(code: i64);

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

fn pad256(x: i64) -> i64 {
    let q: i64 = (x + (255 as i64)) / (256 as i64);
    return q * (256 as i64);
}

fn comp_size(kv: i64, ratio: i64) -> i64 {
    ensure(ratio > (0 as i64), 60);
    let r: i64 = (kv + ratio - (1 as i64)) / ratio;

    if r < (1 as i64) {
        return 1 as i64;
    }
    return r;
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let out: u64 = get(packet, 0, 1);
    let n_layer: i64 = get(packet, 0, 2) as i64;
    let head_k: i64 = get(packet, 0, 3) as i64;
    let idx_head: i64 = get(packet, 0, 4) as i64;
    let ratios: u64 = get(packet, 0, 5);
    let ext: u64 = get(packet, 0, 6);
    let ext_present: bool = get(packet, 0, 7) != (0 as u64);
    let kv_size: i64 = get(packet, 0, 8) as i64;
    ensure(n_layer >= (0 as i64) && head_k > (0 as i64) && idx_head > (0 as i64), 61);
    var ret: i64 = 0;

    if op == (0 as i64) {
        // indexer hparams: one key head, indexer head widths, NeoX rope
        var il: i64 = 0;

        while il < n_layer {
            setat(out, il, 1 as i64);
            // n_head_kv_arr
            il = il + 1 as i64;
        }
        setat(out, 5 as i64 * n_layer + 0 as i64, idx_head);
        // k_full
        setat(out, 5 as i64 * n_layer + 1 as i64, idx_head);
        // v_full
        setat(out, 5 as i64 * n_layer + 2 as i64, idx_head);
        // k_swa
        setat(out, 5 as i64 * n_layer + 3 as i64, idx_head);
        // v_swa
        setat(out, 5 as i64 * n_layer + 4 as i64, 2 as i64);
        // rope NeoX
        ret = 1 as i64;
    } else {
        if op == (1 as i64) {
            // filters: raw = external only; csa = external && ratio 4; hca = external && ratio 128
            var il: i64 = 0;

            while il < n_layer {
                var pass: bool = true;

                if ext_present {
                    pass = at(ext, il) != (0 as i64);
                }
                let r: i64 = at(ratios, il);
                var f_raw: i64 = 0;
                var f_csa: i64 = 0;
                var f_hca: i64 = 0;

                if pass {
                    f_raw = 1 as i64;

                    if r == (4 as i64) {
                        f_csa = 1 as i64;
                    }

                    if r == (128 as i64) {
                        f_hca = 1 as i64;
                    }
                }
                setat(out, (3 as i64 * il) + 0 as i64, f_raw);
                setat(out, (3 as i64 * il) + 1 as i64, f_csa);
                setat(out, (3 as i64 * il) + 2 as i64, f_hca);
                il = il + 1 as i64;
            }
            ret = 1 as i64;
        } else {
            if op == (2 as i64) {
                // compressed cache sizes, padded to 256
                setat(out, 0 as i64, pad256(comp_size(kv_size, 4 as i64)));
                setat(out, 1 as i64, pad256(comp_size(kv_size, 128 as i64)));
                setat(out, 2 as i64, pad256(comp_size(kv_size, 4 as i64)));
                ret = 1 as i64;
            } else {
                if op == (3 as i64) {
                    // compressor states: (ratio, state_size, n_embd)
                    setat(out, 0 as i64, 4 as i64);
                    setat(out, 1 as i64, 8 as i64);
                    setat(out, 2 as i64, 2 as i64 * head_k);
                    setat(out, 3 as i64, 128 as i64);
                    setat(out, 4 as i64, 128 as i64);
                    setat(out, 5 as i64, head_k);
                    setat(out, 6 as i64, 4 as i64);
                    setat(out, 7 as i64, 8 as i64);
                    setat(out, 8 as i64, 2 as i64 * idx_head);
                    ret = 1 as i64;
                } else {
                    if op == (4 as i64) {
                        // unified flags: raw and compressed both stay per-sequence
                        setat(out, 0 as i64, 0 as i64);
                        setat(out, 1 as i64, 0 as i64);
                        // K-only tweak: both MLA widths become the key head width
                        setat(out, 2 as i64, head_k);
                        setat(out, 3 as i64, head_k);
                        ret = 1 as i64;
                    } else {
                        ensure(false, 62);
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

