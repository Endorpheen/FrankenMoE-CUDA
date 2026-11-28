// EXP168s scalar gated-delta-net device recurrence;imports are raw F32 access,expf ISA,warp
// shuffle and PDL effects. Mirrors gated_delta_net.cu gated_delta_net_cuda<S_v,KDA,keep_rs_t>
// with cols_per_warp=4 for every S_v the native host switch instantiates {16,32,64,128};any
// other size (incl. 256) hits native GGML_ABORT and is rejected by host.goose before launch.
// Warp sharding (lanes_per_col/rows_per_lane),transposed state shard loads (M[col][i]=S[i][col],
// row col contiguous),per-token ring recurrence,delta/attn formulas,expf placement,snapshot
// slot mapping (keep_rs_t,K),grouped head iq1=h%neqk1 and sequence iq3=seq/rq3 mapping,and the
// exact floating accumulation and shuffle-butterfly order run here;no original C++ numerical
// helper is called. Plan reads are projections only;all element offsets were validated in
// host.goose and stay inside the checked ggml_nbytes spans on every accepted shape. Input state
// holds s0 only as [S_v,S_v,H,n_seqs] and shares the native per-(seq,head) base offset with the
// output state;state_out is the dst tail or the fused-cache base (host.goose decides).
extern "fg174s_expf" fn expf(x: f32) -> f32;
extern "fg174s_fma" fn fma(a: f32, b: f32, c: f32) -> f32;
extern "fg174s_mul_rn" fn mul_rn(a: f32, b: f32) -> f32;
extern "fg174s_shfl_xor" fn shuffle(v: f32, offset: i32, width: i32) -> f32;
extern "fg174s_sync" fn sync();
// Fast-div coefficient row 0=neqk1,row 1=rq3;all plan pointers are raw bytes.

fn fastdiv(p: i64, n: u32, base: i64) -> u32 {
    let hi = gdn_mulhi(n, gdn_packet_coefficient(p, base, 0));
    let q = ( ( (hi as u64) + (n as u64)) >> (gdn_packet_coefficient(p, base, 1) as u64)) as !u32;
    return q;
}

fn modulo(p: i64, n: u32, base: i64) -> u32 {
    return n - fastdiv(p, n, base) * gdn_packet_coefficient(p, base, 2);
}
// warp_reduce_sum<lanes_per_col> butterfly:offset=lpc/2..1,shuffle width=lpc (float overload).

fn warp_sum(v: f32, width: i32) -> f32 {
    var value = v;
    var offset: i32 = width / 2;

    while offset > 0 {
        value += shuffle(value, offset, width);
        offset >>= 1;
    }
    return value;
}
// One column-segment worker;the parent kernel wrapper maps blockIdx.xyz/threadIdx.xy and
// passes the plan by value. All loops are the native recurrence in native order.

fn gdn_body(p: i64, bidx: i32, bidy: i32, bidz: i32, tidx: i32, tidy: i32, decode: bool) {
    let q = gdn_packet_word(p, 0, 0);
    let k = gdn_packet_word(p, 0, 1);
    let v = gdn_packet_word(p, 0, 2);
    let g = gdn_packet_word(p, 0, 3);
    let beta = gdn_packet_word(p, 0, 4);
    let state_in = gdn_packet_word(p, 0, 5);
    let dst = gdn_packet_word(p, 0, 6);
    let state_out_base = gdn_packet_word(p, 0, 7);
    let sv = if decode { 128 as u64 } else { gdn_packet_word(p, 3, 0) };
    let H = gdn_packet_word(p, 3, 1);
    let n_tokens = if decode { 1 as u64 } else { gdn_packet_word(p, 3, 2) };
    let keep_rs = !decode && gdn_packet_word(p, 3, 7) == (1 as u64);
    let kda = !decode && gdn_packet_word(p, 3, 16) == (1 as u64);
    let warp_size = if decode { 32 as u64 } else { gdn_packet_word(p, 3, 13) };
    let num_warps = if decode { 4 as u64 } else { gdn_packet_word(p, 3, 12) };
    let eff = if warp_size < sv {
        warp_size
    } else {
        sv
    }
    ;
    let lpc64 = eff / (4 as u64);
    let lpc = lpc64 as i32;
    let rpl = (sv / lpc64) as i32;
    let lane = tidx;
    let ciw = lane / lpc;
    let lic = lane - ciw * lpc;
    let col = ( (bidz as u64) * num_warps + (tidy as u64)) * (4 as u64) + (ciw as u64);
    let h_idx = (bidx as u64) as !u32;
    let sequence = (bidy as u64) as !u32;
    let iq1 = modulo(p, h_idx, 0) as u64;
    let iq3 = fastdiv(p, sequence, 1) as u64;
    let sq1 = gdn_packet_word(p, 2, 1);
    let sq2 = gdn_packet_word(p, 2, 2);
    let sq3 = gdn_packet_word(p, 2, 3);
    let sv1 = gdn_packet_word(p, 2, 9);
    let sv2 = gdn_packet_word(p, 2, 10);
    let sv3 = gdn_packet_word(p, 2, 11);
    let sb1 = gdn_packet_word(p, 2, 17);
    let sb2 = gdn_packet_word(p, 2, 18);
    let sb3 = gdn_packet_word(p, 2, 19);
    let slot_stride = gdn_packet_word(p, 3, 15);
    let scale = gdn_packet_scale(p, 0);
    let so = ( (sequence as u64) * H + (h_idx as u64)) * (sv * sv);
    let attn0 = ( (sequence as u64) * n_tokens * H + (h_idx as u64)) * sv;
    let cur = state_in + (so + col * sv) * 4;
    let state_out = state_out_base + so * 4;
    var attn = dst + attn0 * 4;
    var s: f32[32] = default < f32[32] > ();
    sync();

    for r in rpl {
        let i = (r * lpc + lic) as u64;
        s[r] = gdn_load(cur, i);
    }

    for token in (n_tokens as i64) {
        let t = token as u64;
        let qb = iq3 * sq3 + t * sq2 + iq1 * sq1;
        let qt = q + qb * 4;
        let kt = k + qb * 4;
        let vt = v + ( (sequence as u64) * sv3 + t * sv2 + (h_idx as u64) * sv1) * 4;
        let gbo = (sequence as u64) * sb3 + t * sb2 + (h_idx as u64) * sb1;
        let bt = beta + gbo * 4;
        let beta_val = gdn_load(bt, 0 as u64);
        var kr: f32[32] = default < f32[32] > ();
        var qr: f32[32] = default < f32[32] > ();
        for r in rpl {
            let i = (r * lpc + lic) as u64;
            kr[r] = gdn_load(kt, i);
            qr[r] = gdn_load(qt, i);
        }

        if !kda {
            let g_val = expf(gdn_load(g + gbo * 4, 0 as u64));
            var kv_shard: f32 = 0.0;
            for r in rpl {
                kv_shard = fma(s[r], kr[r], kv_shard);
            }
            let kv_col = warp_sum(kv_shard, lpc);
            let delta_col = (gdn_load(vt, col) - g_val * kv_col) * beta_val;
            var attn_partial: f32 = 0.0;
            for r in rpl {
                s[r] = fma(g_val, s[r], mul_rn(kr[r], delta_col));
                attn_partial = fma(s[r], qr[r], attn_partial);
            }
            let attn_col = warp_sum(attn_partial, lpc);

            if lic == 0 {
                gdn_store(attn, col, attn_col * scale);
            }
        } else {
            let gk = g + gbo * sv * 4;
            var kv_shard: f32 = 0.0;
            for r in rpl {
                let i = (r * lpc + lic) as u64;
                kv_shard = fma(expf(gdn_load(gk, i)) * s[r], kr[r], kv_shard);
            }
            let kv_col = warp_sum(kv_shard, lpc);
            let delta_col = (gdn_load(vt, col) - kv_col) * beta_val;
            var attn_partial: f32 = 0.0;
            for r in rpl {
                let i = (r * lpc + lic) as u64;
                s[r] = fma(expf(gdn_load(gk, i)), s[r], mul_rn(kr[r], delta_col));
                attn_partial = fma(s[r], qr[r], attn_partial);
            }
            let attn_col = warp_sum(attn_partial, lpc);

            if lic == 0 {
                gdn_store(attn, col, attn_col * scale);
            }
        }
        attn += sv * H * 4;

        if keep_rs {
            let target = n_tokens - 1 - t;

            if target < gdn_packet_word(p, 3, 6) {
                let cs = state_out + target * slot_stride * 4;
                for r in rpl {
                    let i = (r * lpc + lic) as u64;
                    gdn_store(cs, col * sv + i, s[r]);
                }
            }
        }
    }

    if !keep_rs {
        for r in rpl {
            let i = (r * lpc + lic) as u64;
            gdn_store(state_out, col * sv + i, s[r]);
        }
    }
}

export fn gdn(p: i64, bidx: i32, bidy: i32, bidz: i32, tidx: i32, tidy: i32) {
    gdn_body(p, bidx, bidy, bidz, tidx, tidy, false);
}

export fn gdn_decode(p: i64, bidx: i32, bidy: i32, bidz: i32, tidx: i32, tidy: i32) {
    gdn_body(p, bidx, bidy, bidz, tidx, tidy, true);
}

