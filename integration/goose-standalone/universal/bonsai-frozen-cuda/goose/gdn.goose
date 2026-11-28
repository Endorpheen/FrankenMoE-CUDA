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
    let q = gdn_packet_word(p, 0, 0);
    let k = gdn_packet_word(p, 0, 1);
    let v = gdn_packet_word(p, 0, 2);
    let g = gdn_packet_word(p, 0, 3);
    let beta = gdn_packet_word(p, 0, 4);
    let state = gdn_packet_word(p, 0, 5);
    let attention = gdn_packet_word(p, 0, 6);
    let state_out = gdn_packet_word(p, 0, 7);
    let h = (bidx as u64) as! u32;
    let sequence = (bidy as u64) as! u32;
    let head_count = gdn_packet_word(p, 3, 1);
    let column = ((bidz as u64) * 4 + (tidy as u64)) * 4 + ((tidx / 8) as u64);
    let lane = (tidx % 8) as u64;
    let head = (sequence as u64) * head_count + (h as u64);
    let state_offset = 4 * (head * 16384 + column * 128);
    let qk = 4 * ((fastdiv(p, sequence, 1) as u64) * gdn_packet_word(p, 2, 3)
                  + (modulo(p, h, 0) as u64) * gdn_packet_word(p, 2, 1));
    let value_offset = 4 * ((sequence as u64) * gdn_packet_word(p, 2, 11) + (h as u64) * gdn_packet_word(p, 2, 9));
    let gate_offset = 4 * ((sequence as u64) * gdn_packet_word(p, 2, 19) + (h as u64) * gdn_packet_word(p, 2, 17));
    let cur = state + state_offset;
    let next = state_out + state_offset;
    let g_value = expf(gdn_load(g + gate_offset, 0));
    let beta_value = gdn_load(beta + gate_offset, 0);
    sync();
    var kv: f32 = 0.0;
    let s0 = gdn_load(cur, lane + 0);
    let k0 = gdn_load(k + qk, lane + 0);
    kv = fma(s0, k0, kv);
    let s1 = gdn_load(cur, lane + 8);
    let k1 = gdn_load(k + qk, lane + 8);
    kv = fma(s1, k1, kv);
    let s2 = gdn_load(cur, lane + 16);
    let k2 = gdn_load(k + qk, lane + 16);
    kv = fma(s2, k2, kv);
    let s3 = gdn_load(cur, lane + 24);
    let k3 = gdn_load(k + qk, lane + 24);
    kv = fma(s3, k3, kv);
    let s4 = gdn_load(cur, lane + 32);
    let k4 = gdn_load(k + qk, lane + 32);
    kv = fma(s4, k4, kv);
    let s5 = gdn_load(cur, lane + 40);
    let k5 = gdn_load(k + qk, lane + 40);
    kv = fma(s5, k5, kv);
    let s6 = gdn_load(cur, lane + 48);
    let k6 = gdn_load(k + qk, lane + 48);
    kv = fma(s6, k6, kv);
    let s7 = gdn_load(cur, lane + 56);
    let k7 = gdn_load(k + qk, lane + 56);
    kv = fma(s7, k7, kv);
    let s8 = gdn_load(cur, lane + 64);
    let k8 = gdn_load(k + qk, lane + 64);
    kv = fma(s8, k8, kv);
    let s9 = gdn_load(cur, lane + 72);
    let k9 = gdn_load(k + qk, lane + 72);
    kv = fma(s9, k9, kv);
    let s10 = gdn_load(cur, lane + 80);
    let k10 = gdn_load(k + qk, lane + 80);
    kv = fma(s10, k10, kv);
    let s11 = gdn_load(cur, lane + 88);
    let k11 = gdn_load(k + qk, lane + 88);
    kv = fma(s11, k11, kv);
    let s12 = gdn_load(cur, lane + 96);
    let k12 = gdn_load(k + qk, lane + 96);
    kv = fma(s12, k12, kv);
    let s13 = gdn_load(cur, lane + 104);
    let k13 = gdn_load(k + qk, lane + 104);
    kv = fma(s13, k13, kv);
    let s14 = gdn_load(cur, lane + 112);
    let k14 = gdn_load(k + qk, lane + 112);
    kv = fma(s14, k14, kv);
    let s15 = gdn_load(cur, lane + 120);
    let k15 = gdn_load(k + qk, lane + 120);
    kv = fma(s15, k15, kv);
    let delta = (gdn_load(v + value_offset, column) - g_value * warp_sum(kv, 8)) * beta_value;
    var value: f32 = 0.0;
    let a0 = fma(g_value, s0, mul_rn(k0, delta));
    value = fma(a0, gdn_load(q + qk, lane + 0), value);
    gdn_store(next, lane + 0, a0);
    let a1 = fma(g_value, s1, mul_rn(k1, delta));
    value = fma(a1, gdn_load(q + qk, lane + 8), value);
    gdn_store(next, lane + 8, a1);
    let a2 = fma(g_value, s2, mul_rn(k2, delta));
    value = fma(a2, gdn_load(q + qk, lane + 16), value);
    gdn_store(next, lane + 16, a2);
    let a3 = fma(g_value, s3, mul_rn(k3, delta));
    value = fma(a3, gdn_load(q + qk, lane + 24), value);
    gdn_store(next, lane + 24, a3);
    let a4 = fma(g_value, s4, mul_rn(k4, delta));
    value = fma(a4, gdn_load(q + qk, lane + 32), value);
    gdn_store(next, lane + 32, a4);
    let a5 = fma(g_value, s5, mul_rn(k5, delta));
    value = fma(a5, gdn_load(q + qk, lane + 40), value);
    gdn_store(next, lane + 40, a5);
    let a6 = fma(g_value, s6, mul_rn(k6, delta));
    value = fma(a6, gdn_load(q + qk, lane + 48), value);
    gdn_store(next, lane + 48, a6);
    let a7 = fma(g_value, s7, mul_rn(k7, delta));
    value = fma(a7, gdn_load(q + qk, lane + 56), value);
    gdn_store(next, lane + 56, a7);
    let a8 = fma(g_value, s8, mul_rn(k8, delta));
    value = fma(a8, gdn_load(q + qk, lane + 64), value);
    gdn_store(next, lane + 64, a8);
    let a9 = fma(g_value, s9, mul_rn(k9, delta));
    value = fma(a9, gdn_load(q + qk, lane + 72), value);
    gdn_store(next, lane + 72, a9);
    let a10 = fma(g_value, s10, mul_rn(k10, delta));
    value = fma(a10, gdn_load(q + qk, lane + 80), value);
    gdn_store(next, lane + 80, a10);
    let a11 = fma(g_value, s11, mul_rn(k11, delta));
    value = fma(a11, gdn_load(q + qk, lane + 88), value);
    gdn_store(next, lane + 88, a11);
    let a12 = fma(g_value, s12, mul_rn(k12, delta));
    value = fma(a12, gdn_load(q + qk, lane + 96), value);
    gdn_store(next, lane + 96, a12);
    let a13 = fma(g_value, s13, mul_rn(k13, delta));
    value = fma(a13, gdn_load(q + qk, lane + 104), value);
    gdn_store(next, lane + 104, a13);
    let a14 = fma(g_value, s14, mul_rn(k14, delta));
    value = fma(a14, gdn_load(q + qk, lane + 112), value);
    gdn_store(next, lane + 112, a14);
    let a15 = fma(g_value, s15, mul_rn(k15, delta));
    value = fma(a15, gdn_load(q + qk, lane + 120), value);
    gdn_store(next, lane + 120, a15);
    let total = warp_sum(value, 8);
    if lane == 0 { gdn_store(attention + head * 512, column, total * gdn_packet_scale(p, 0)); }
}

