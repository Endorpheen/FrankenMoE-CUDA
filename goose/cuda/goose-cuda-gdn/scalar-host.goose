// EXP168s scalar gated-delta-net host policy:native shape/type/stride validation,MMA-vs-scalar
// route and exact launch geometry. Replicates every ggml_cuda_op_gated_delta_net_impl assert
// plus the checks its kernels/launch require (checked spans,fastdiv magic domains,warp-shard
// divides,grid caps,input state layout,state slot span). The MMA attempt is a single declared
// external dispatch effect (fg174m_launch,parent-owned fg174m_ port of ggml_cuda_gdn_mma_launch
// consuming this plan);when it declines,the scalar packet below launches. No original C++
// arithmetic runs on the default scalar path. Strides are kept in FLOAT ELEMENTS like native
// (nb/sizeof(float));the input-state layout is the native hardcoded [S_v,S_v,H,n_seqs].
extern "fg174s_arg" fn arg(packet: i64, k: i64) -> i64;
extern "fg174s_plan_ptr" fn plan_ptr(packet: i64) -> i64;
extern "fg174s_hget" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg174s_hput" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg174s_hfloat" fn put_float(p: i64, i: i64, v: f32);
extern "fg174s_ti" fn ti(t: i64, k: i64, i: i64) -> i64;
extern "fg174s_contiguous" fn contiguous(t: i64) -> bool;
extern "fg174s_f32_type" fn f32_type() -> i64;
extern "fg174s_warp_size" fn warp_size() -> i64;
extern "fg174s_nsm" fn nsm() -> i64;
extern "fg174s_sqrt" fn sqrtf(x: f32) -> f32;
extern "fg174s_launch" fn launch(packet: i64);
extern "fg174s_fail" fn fail(code: i64);
extern "fg174m_launch" fn mma_launch(packet: i64) -> bool;

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn U() -> u64 {
    return 18446744073709551615 as u64;
}

fn mul(a: u64, b: u64) -> u64 {
    ensure(a == 0 || b <= U() / a, 1);
    return a * b;
}

fn add(a: u64, b: u64) -> u64 {
    ensure(a <= U() - b, 1);
    return a + b;
}

fn ceil(a: u64, b: u64) -> u64 {
    ensure(b != 0, 2);
    return a / b + if a % b != 0 {
        1 as u64
    } else {
        0 as u64
    }
    ;
}

fn minimum(a: u64, b: u64) -> u64 {
    return if a < b {
        a
    } else {
        b
    }
    ;
}

fn span(t: i64) -> u64 {
    var bytes: u64 = 4;
    var i: i64 = 0;

    while i < 4 {
        let n = ti(t, 3, i);
        let nb = ti(t, 4, i);
        ensure(n > 0 && nb >= 0, 18);
        bytes = add(bytes, mul( (n - 1) as u64, nb as u64));
        i += 1;
    }
    let ptr = ti(t, 1, 0) as u64;
    ensure(ptr != 0 && bytes <= 9223372036854775807 && ptr <= U() - bytes, 18);
    let declared = ti(t, 9, 0);
    ensure(declared > 0 && bytes <= declared as u64, 18);
    return bytes;
}

fn copy_tensor(p: i64, t: i64, which: i64) {
    ensure(t != 0, 5);
    ensure(ti(t, 2, 0) == f32_type(), 5);
    ensure(ti(t, 1, 0) != 0, 5);
    ensure(ti(t, 4, 0) == 4, 5);
    put(p, 0, which, ti(t, 1, 0) as u64);
    var i: i64 = 0;

    while i < 4 {
        let n = ti(t, 3, i);
        let stride = ti(t, 4, i);
        ensure(n > 0, 6);
        ensure(n <= 2147483647, 6);
        ensure(stride >= 0, 6);
        ensure(stride <= 2147483647, 6);
        ensure( (stride as u64) % 4 == 0, 6);
        put(p, 1, which * 4 + i, n as u64);
        put(p, 2, which * 4 + i, (stride as u64) / 4);
        i += 1;
    }
    span(t);
}

fn fits(t: i64, elems: u64, c: i64) {
    ensure(span(t) >= mul(elems, 4), c);
}
// init_fastdiv_values <mp,L,d> packed as magic[slot*3+{0,1,2}].

fn magic(p: i64, slot: i64, d: u64) {
    ensure(d > 0 && d <= 4294967295, 4);
    var l: u64 = 0;

    while l < 32 && ( (1 as u64) << l) < d {
        l += 1;
    }
    let mp = ( (1 as u64) << 32) * ( ( (1 as u64) << l) - d) / d + 1;
    ensure(mp <= 4294967295, 4);
    put(p, 4, slot * 3, mp);
    put(p, 4, slot * 3 + 1, l);
    put(p, 4, slot * 3 + 2, d);
}

fn overlap(a: u64, an: u64, b: u64, bn: u64) -> bool {
    return a < b + bn && b < a + an;
}

fn run(packet: i64) {
    let mode = arg(packet, 0);
    let dst = arg(packet, 2);
    let cache_data = arg(packet, 3);
    let cache_slot = arg(packet, 4);
    ensure(arg(packet, 1) != 0, 3);
    ensure(mode == 0 || mode == 1, 3);
    ensure(dst != 0, 3);

    if mode == 0 {
        ensure(cache_data == 0, 3);
    } else {
        ensure(cache_data != 0, 3);
    }
    ensure(ti(dst, 2, 0) == f32_type(), 5);
    ensure(ti(dst, 1, 0) != 0, 5);
    let q = ti(dst, 0, 0);
    let k = ti(dst, 0, 1);
    let v = ti(dst, 0, 2);
    let g = ti(dst, 0, 3);
    let beta = ti(dst, 0, 4);
    let st = ti(dst, 0, 5);
    ensure(q != 0 && k != 0 && v != 0 && g != 0 && beta != 0 && st != 0, 7);
    let p = plan_ptr(packet);
    copy_tensor(p, q, 0);
    copy_tensor(p, k, 1);
    copy_tensor(p, v, 2);
    copy_tensor(p, g, 3);
    copy_tensor(p, beta, 4);
    copy_tensor(p, st, 5);
    copy_tensor(p, dst, 6);
    // native GGML_ASSERT block
    ensure(get(p, 1, 1) == get(p, 1, 5), 8);
    let neq3 = get(p, 1, 3);
    let nek3 = get(p, 1, 7);
    let nev3 = get(p, 1, 11);
    ensure(neq3 > 0, 9);
    ensure(nev3 >= neq3, 9);
    ensure(nev3 % neq3 == 0, 9);
    ensure(nek3 == neq3, 9);
    let rq3 = nev3 / neq3;
    let neqk1 = get(p, 1, 1);
    let sv = get(p, 1, 8);
    let H = get(p, 1, 9);
    let nt = get(p, 1, 10);
    let n_seqs = nev3;
    // ggml_is_contiguous_rows(q/k/v): F32 innermost stride is one element
    ensure(get(p, 2, 0) == 1, 10);
    ensure(get(p, 2, 4) == 1, 10);
    ensure(get(p, 2, 8) == 1, 10);
    // ggml_are_same_stride(q,k)
    var i: i64 = 0;

    while i < 4 {
        ensure(get(p, 2, i) == get(p, 2, 4 + i), 11);
        i += 1;
    }
    let g0 = get(p, 1, 12);
    let kda = g0 == sv;
    ensure(g0 == 1 || kda, 12);
    ensure(contiguous(g), 13);
    ensure(contiguous(beta), 13);
    ensure(contiguous(st), 13);
    // kernel-indexed input state layout (native hardcodes these offsets)
    ensure(get(p, 1, 20) == sv && get(p, 1, 21) == sv && get(p, 1, 22) == H && get(p, 1, 23) == n_seqs, 14);
    let K = ti(dst, 8, 0);
    let keep_rs = K > 1;
    ensure(get(p, 1, 0) == sv && get(p, 1, 4) == sv && get(p, 1, 2) >= nt && get(p, 1, 6) >= nt, 22);
    // native scalar switch instantiates S_v in {16,32,64,128};others (incl. 256) abort
    ensure(sv == 16 || sv == 32 || sv == 64 || sv == 128, 20);
    // device-property effects (ggml_cuda_info physical warp size / nsm)
    let ws = warp_size();
    ensure(ws == 32 || ws == 64, 21);
    let nsm_v = nsm();
    ensure(nsm_v > 0, 21);
    // native num_warps halving loop: while(nw>1 && H*n_seqs*(S_v/(4*nw))<nsm) nw/=2
    var nw: u64 = 4;
    var shrink = true;

    while shrink {
        if nw > 1 {
            let seg = sv / mul( (4 as u64), nw);

            if mul(mul(H, n_seqs), seg) < (nsm_v as u64) {
                nw = nw / 2;
            } else {
                shrink = false;
            }
        } else {
            shrink = false;
        }
    }
    let seg4 = mul( (4 as u64), nw);
    ensure(sv % seg4 == 0, 15);
    let eff = if (ws as u64) < sv {
        (ws as u64)
    } else {
        sv
    }
    ;
    let lpc64 = eff / (4 as u64);
    ensure(lpc64 > 0 && sv % lpc64 == 0, 15);
    let gz = ceil(sv, seg4);
    ensure(n_seqs <= 65535, 16);
    ensure(gz <= 65535, 16);
    // scale via ISA effect: exactly native 1.0f/sqrtf((float)S_v)
    put_float(p, 0, 1.0 / sqrtf(sv as f32));
    magic(p, 0, neqk1);
    magic(p, 1, rq3);
    // state_out: dst tail after attention scores,or the fused-cache base
    let attn_elems = mul(mul(mul(sv, H), nt), n_seqs);
    let natural_stride = mul(mul(mul(sv, sv), H), n_seqs);

    if mode == 1 {
        ensure(cache_slot >= 0, 23);

        if keep_rs {
            ensure( (cache_slot as u64) >= natural_stride, 23);
        }
    }
    let slot_stride = if mode == 1 {
        (cache_slot as u64)
    } else {
        natural_stride
    }
    ;
    let slots = if keep_rs {
        minimum( (K as u64), nt)
    } else {
        1 as u64
    }
    ;
    let state_out = if mode == 1 {
        (cache_data as u64)
    } else {
        add( (ti(dst, 1, 0) as u64), mul(attn_elems, 4))
    }
    ;
    let state_bytes = mul(add(mul(slots - 1, slot_stride), natural_stride), 4);
    ensure(state_out <= U() - state_bytes, 24);
    ensure(span(dst) >= mul(attn_elems, 4), 17);

    if mode == 0 {
        ensure(span(dst) >= mul(add(attn_elems, mul(slots, slot_stride)), 4), 17);
    }
    // exact max-element-offset coverage for the strided inputs
    let iq1_max = if H < neqk1 {
        H - 1
    } else {
        neqk1 - 1
    }
    ;
    let iq3_max = (n_seqs - 1) / rq3;
    let qk_elems = add(add(mul(iq3_max, get(p, 2, 3)), mul(nt - 1, get(p, 2, 2))), add(mul(iq1_max, get(p, 2, 1)), sv));
    fits(q, qk_elems, 18);
    fits(k, qk_elems, 18);
    let v_elems = add(
        add(add(mul(n_seqs - 1, get(p, 2, 11)), mul(nt - 1, get(p, 2, 10))), mul(H - 1, get(p, 2, 9))),
        sv);
    fits(v, v_elems, 18);
    let gb = add(add(mul(n_seqs - 1, get(p, 2, 19)), mul(nt - 1, get(p, 2, 18))), mul(H - 1, get(p, 2, 17)));
    let gb_elems = add(gb, 1);
    fits(beta, gb_elems, 18);
    let g_elems = if kda {
        mul(gb_elems, sv)
    } else {
        gb_elems
    }
    ;
    fits(g, g_elems, 18);
    fits(st, mul(mul(mul(n_seqs, H), sv), sv), 19);
    let score_ptr = ti(dst, 1, 0) as u64;
    let score_bytes = mul(attn_elems, 4);
    ensure(!overlap(score_ptr, score_bytes, state_out, state_bytes), 25);
    var ai: i64 = 0;

    while ai < 6 {
        let input = ti(dst, 0, ai);
        let ip = ti(input, 1, 0) as u64;
        let ins = span(input);
        ensure(!overlap(score_ptr, score_bytes, ip, ins), 25);

        if ai == 5 && ip == state_out {
            ensure(ins == mul(natural_stride, 4), 25);
        } else {
            ensure(!overlap(state_out, state_bytes, ip, ins), 25);
        }
        ai += 1;
    }
    // native ggml_cuda_gdn_mma_args_from_tensor eligibility (route decision stays here)
    let eligible = (g0 == 1) && (K < (nt as i64)) && sv == 128 && get(p, 1, 0) == 128;
    put(p, 3, 0, sv);
    put(p, 3, 1, H);
    put(p, 3, 2, nt);
    put(p, 3, 3, n_seqs);
    put(p, 3, 4, neqk1);
    put(p, 3, 5, rq3);
    put(p, 3, 6, K as u64);
    put(p, 3, 7, if keep_rs {
        1 as u64
    } else {
        0 as u64
    }
    );
    put(p, 3, 8, H);
    put(p, 3, 9, n_seqs);
    put(p, 3, 10, gz);
    put(p, 3, 11, eff);
    put(p, 3, 12, nw);
    put(p, 3, 13, ws as u64);
    put(p, 3, 14, nsm_v as u64);
    put(p, 3, 15, slot_stride);
    put(p, 3, 16, if kda {
        1 as u64
    } else {
        0 as u64
    }
    );
    put(p, 3, 17, if eligible {
        1 as u64
    } else {
        0 as u64
    }
    );
    put(p, 3, 18, neqk1);
    put(p, 0, 7, state_out);

    if eligible {
        if mma_launch(packet) {
            return;
        }
    }
    launch(packet);
}

fn main() {
    run(0);
}

