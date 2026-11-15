// P7-04 S3b: the remaining unweighted row quantizers in Goose.
//
// Ported from ggml/src/ggml-quants.c (llama.cpp-flint-cuda, 1b44e307):
//   - quantize_row_q2_K_ref        (QK_K = 256, 16 groups of 16)
//   - quantize_row_q3_K_ref        (QK_K = 256, 6-bit scales + hmask)
//   - quantize_row_q4_K_ref        (QK_K = 256, 8 groups of 32, 6-bit scales/mins)
//   - quantize_row_q5_K_ref        (QK_K = 256, as q4_K plus a 5th bit in qh)
//   - quantize_row_q6_K_ref        (QK_K = 256, 16 groups of 16, 8-bit scales)
//   - quantize_row_q8_K_ref        (QK_K = 256, int8 quants, float d, bsums)
//   - quantize_row_tq1_0_ref       (QK_K = 256, ternary, base-3 packing)
//   - quantize_row_tq2_0_ref       (QK_K = 256, ternary, 2 bits per element)
//   - quantize_row_mxfp4_ref       (QK_MXFP4 = 32, E8M0 scale, E2M1 values)
//   - quantize_row_nvfp4_ref       (QK_NVFP4 = 64, four UE4M3 sub-scales)
//   - quantize_row_iq4_nl_ref      (QK4_NL = 32, 16 non-linear values)
//   - quantize_row_iq4_xs_ref      (QK_K = 256, six-bit block scales)
//   - quantize_row_iq3_xxs_ref     (QK_K = 256, D4 grid, 3 bits per element)
//   - quantize_row_iq3_s_ref       (QK_K = 256, 512-point grid plus signs)
//   - quantize_row_iq2_s_ref       (QK_K = 256, 1024-point grid plus signs)
//   - quantize_row_iq1_m_ref path  (QK_K = 256, 2048-point grid, ternary)
// and from ggml/src/ggml.c:
//   - ggml_quantize_chunk with imatrix == nullptr, i.e. only the row path,
//     including its start/n_per_row geometry and the start_row * row_size
//     destination offset.
//
// Byte-identical output is the requirement. Every intermediate scalar keeps
// the native expression shape and evaluation order: the same divisor, the same
// order of the accumulation loops, the same `nearest_int` rounding (round half
// to even through the native magic constant), the same float -> int
// conversions, and the f16 stores followed by a read back when the reference
// re-reads y[i].d or y[i].dmin.
//
// Boundaries, stated instead of hidden:
//   - IQ2_XXS, IQ2_XS and IQ1_S have no unweighted native path: their impls
//     assert quant_weights != nullptr and ggml_quantize_chunk asserts through
//     ggml_quantize_requires_imatrix. They belong to S4 and are not ported.
//   - q8_K is not a case of ggml_quantize_chunk in this tree (the switch has no
//     GGML_TYPE_Q8_K, so the native entry aborts). It is gated against
//     quantize_row_q8_K_ref directly, and qz2_quantize_chunk refuses it.
//   - the IQ grid code tables, the code -> grid map and the neighbour lists are
//     built by ggml_quantize_init and are file-static in the native library.
//     They are host primitives here (qz2_iq*_grid, qz2_iq*_map, qz2_iq*_nb), the
//     same way the IQ decoders in goose/kv read grid and sign tables.
//   - fp16 rounding (qz2_st_f16/qz2_ld_f16), sqrtf/log2f/floorf/lroundf/ldexpf
//     and float <-> bit reinterpretation (qz2_bits/qz2_of_bits) are primitives:
//     hardware or libm operations, not decisions of the quantizer.
//   - the native (int8_t)/(uint8_t) cast of a float is undefined outside the
//     target range. The port truncates toward zero and keeps the low eight
//     bits, which is what the reference build emits (cvttss2si plus a byte
//     truncation). Blocks whose scale underflows to inf are therefore still
//     byte-identical, but NaN and +-inf rows are outside the corpus.
//   - IQ1_M sorts 16 records with qsort and a value-only comparator. The port
//     uses the host sort primitive qz2_sort16 (the same libc call, the same
//     comparator) because the order of equal keys is not defined by the C
//     library and cannot be reproduced by a comparison-only rewrite.
//   - the native asserts on bad geometry. The port returns -1 (unknown type)
//     or -2 (geometry) so a fixture can observe the refusal.
//
// Pointer form: buffers are u64 addresses, as in goose/model/read-path.goose.
// The port carries no arrays; intermediate vectors live in a host scratch block
// (qz2_tmp) at fixed byte offsets, which is the same boundary the decoders use
// for their tables.

extern "qz2_ld_f32" fn ld_f32(a: u64) -> f32;
extern "qz2_st_f32" fn st_f32(a: u64, v: f32);
extern "qz2_ld_f16" fn ld_f16(a: u64) -> f32;
extern "qz2_st_f16" fn st_f16(a: u64, v: f32);
extern "qz2_st_u8" fn st_u8(a: u64, v: i64);
extern "qz2_ld_u8" fn ld_u8(a: u64) -> i64;
extern "qz2_st_u16" fn st_u16(a: u64, v: i64);
extern "qz2_st_u32" fn st_u32(a: u64, v: i64);
extern "qz2_abs" fn fabs32(v: f32) -> f32;
extern "qz2_sqrtf" fn sqrt32(v: f32) -> f32;
extern "qz2_log2f" fn log232(v: f32) -> f32;
extern "qz2_floorf" fn floor32(v: f32) -> f32;
extern "qz2_lroundf" fn lround32(v: f32) -> i64;
extern "qz2_ldexpf" fn ldexp32(v: f32, e: i64) -> f32;
extern "qz2_bits" fn f32_bits(v: f32) -> i64;
extern "qz2_of_bits" fn f32_of_bits(b: i64) -> f32;
extern "qz2_nearest" fn nearest(v: f32) -> i64;
extern "qz2_tmp" fn tmp(off: i64) -> u64;
extern "qz2_fail" fn fail(code: i64);
extern "qz2_iq2_grid" fn iq2_grid(g: i64, i: i64) -> i64;
extern "qz2_iq2_qgrid" fn iq2_qgrid(g: i64, i: i64) -> i64;
extern "qz2_iq2_map" fn iq2_map(g: i64, u: i64) -> i64;
extern "qz2_iq2_nb" fn iq2_nb(g: i64, off: i64) -> i64;
extern "qz2_iq3_grid" fn iq3_grid(g: i64, i: i64) -> i64;
extern "qz2_iq3_grid" fn iq3_qgrid(g: i64, i: i64) -> i64;
extern "qz2_iq3_map" fn iq3_map(g: i64, u: i64) -> i64;
extern "qz2_iq3_nb" fn iq3_nb(g: i64, off: i64) -> i64;
extern "qz2_sort16" fn sort16(pairs: u64, n: i64);

// ---- ggml type ids and per-type geometry ----------------------------------

const T_Q2_K: i64 = 10;
const T_Q3_K: i64 = 11;
const T_Q4_K: i64 = 12;
const T_Q5_K: i64 = 13;
const T_Q6_K: i64 = 14;
const T_Q8_K: i64 = 15;
const T_IQ2_XXS: i64 = 16;
const T_IQ2_XS: i64 = 17;
const T_IQ3_XXS: i64 = 18;
const T_IQ1_S: i64 = 19;
const T_IQ4_NL: i64 = 20;
const T_IQ3_S: i64 = 21;
const T_IQ2_S: i64 = 22;
const T_IQ4_XS: i64 = 23;
const T_IQ1_M: i64 = 29;
const T_TQ1_0: i64 = 34;
const T_TQ2_0: i64 = 35;
const T_MXFP4: i64 = 39;
const T_NVFP4: i64 = 40;

const QK_K: i64 = 256;
const QK4_NL: i64 = 32;
const QK_MXFP4: i64 = 32;
const QK_NVFP4: i64 = 64;
const QK_NVFP4_SUB: i64 = 16;

const B_Q2_K: i64 = 84;   // 16 scales + 64 qs + d + dmin
const B_Q3_K: i64 = 110;  // 32 hmask + 64 qs + 12 scales + d
const B_Q4_K: i64 = 144;  // d + dmin + 12 scales + 128 qs
const B_Q5_K: i64 = 176;  // d + dmin + 12 scales + 32 qh + 128 qs
const B_Q6_K: i64 = 210;  // 128 ql + 64 qh + 16 scales + d
const B_Q8_K: i64 = 292;  // float d + 256 int8 + 16 int16
const B_TQ1_0: i64 = 54;  // 48 qs + 4 qh + d
const B_TQ2_0: i64 = 66;  // 64 qs + d
const B_MXFP4: i64 = 17;  // e + 16 qs
const B_NVFP4: i64 = 36;  // 4 scales + 32 qs
const B_IQ4_NL: i64 = 18; // d + 16 qs
const B_IQ4_XS: i64 = 136; // d + scales_h + 4 scales_l + 128 qs
const B_IQ3_XXS: i64 = 98; // d + 96 qs
const B_IQ3_S: i64 = 110;  // d + 64 qs + 8 qh + 32 signs + 8 scales
const B_IQ2_S: i64 = 82;   // d + 64 qs + 8 qh + 8 scales
const B_IQ1_M: i64 = 56;   // 32 qs + 16 qh + 8 scales

// ggml_blck_size for the types of this slice, -1 outside it.
export fn qz2_blck(t: i64) -> i64 {
    if t == T_Q2_K { return QK_K; }
    if t == T_Q3_K { return QK_K; }
    if t == T_Q4_K { return QK_K; }
    if t == T_Q5_K { return QK_K; }
    if t == T_Q6_K { return QK_K; }
    if t == T_Q8_K { return QK_K; }
    if t == T_IQ3_XXS { return QK_K; }
    if t == T_IQ3_S { return QK_K; }
    if t == T_IQ2_S { return QK_K; }
    if t == T_IQ4_XS { return QK_K; }
    if t == T_IQ1_M { return QK_K; }
    if t == T_TQ1_0 { return QK_K; }
    if t == T_TQ2_0 { return QK_K; }
    if t == T_IQ4_NL { return QK4_NL; }
    if t == T_MXFP4 { return QK_MXFP4; }
    if t == T_NVFP4 { return QK_NVFP4; }
    return -1;
}

export fn qz2_type_size(t: i64) -> i64 {
    if t == T_Q2_K { return B_Q2_K; }
    if t == T_Q3_K { return B_Q3_K; }
    if t == T_Q4_K { return B_Q4_K; }
    if t == T_Q5_K { return B_Q5_K; }
    if t == T_Q6_K { return B_Q6_K; }
    if t == T_Q8_K { return B_Q8_K; }
    if t == T_IQ3_XXS { return B_IQ3_XXS; }
    if t == T_IQ3_S { return B_IQ3_S; }
    if t == T_IQ2_S { return B_IQ2_S; }
    if t == T_IQ4_XS { return B_IQ4_XS; }
    if t == T_IQ1_M { return B_IQ1_M; }
    if t == T_TQ1_0 { return B_TQ1_0; }
    if t == T_TQ2_0 { return B_TQ2_0; }
    if t == T_IQ4_NL { return B_IQ4_NL; }
    if t == T_MXFP4 { return B_MXFP4; }
    if t == T_NVFP4 { return B_NVFP4; }
    return -1;
}

export fn qz2_row_size(t: i64, ne: i64) -> i64 {
    let blck = qz2_blck(t);
    if blck < 0 { return -1; }
    let ts = qz2_type_size(t);
    if ne <= 0 || ne % blck != 0 { return -2; }
    return ts * ne / blck;
}

// ---- scratch offsets ------------------------------------------------------

const S_L: i64 = 0;        // 256 int8/uint8 quants
const S_LAUX: i64 = 256;   // 32 int8
const S_SCALES: i64 = 320; // 16 f32
const S_MINS: i64 = 384;   // 16 f32
const S_WEIGHTS: i64 = 448; // 32 f32
const S_XVAL: i64 = 576;   // 32 f32
const S_WAUX: i64 = 704;   // 32 f32
const S_SIGNS: i64 = 832;  // 8 uint8
const S_PAIRS: i64 = 848;  // 32 f32 records (16 pairs)
const S_IDX: i64 = 1008;   // 16 i32 indices
const S_TMP: i64 = 1088;   // 16 f32

fn sa(off: i64) -> u64 {
    return tmp(off);
}

fn sgetf(off: i64) -> f32 {
    return ld_f32(tmp(off));
}

fn ssetf(off: i64, v: f32) {
    st_f32(tmp(off), v);
}

fn sgetb(off: i64) -> i64 {
    return ld_u8(tmp(off));
}

fn ssetb(off: i64, v: i64) {
    st_u8(tmp(off), v & 255);
}

// ---- small numeric helpers ------------------------------------------------

fn imin(a: i64, b: i64) -> i64 {
    if a < b { return a; }
    return b;
}

fn imax(a: i64, b: i64) -> i64 {
    if a > b { return a; }
    return b;
}

fn fmax32(a: f32, b: f32) -> f32 {
    if a > b { return a; }
    return b;
}

fn i8val(v: i64) -> i64 {
    let u = v & 255;
    if u >= 128 { return u - 256; }
    return u;
}

// The native (int8_t)/(uint8_t) cast of a truncated f32 on x86-64 gcc.
const I32_INDEF: i64 = -2147483648;

fn f2i32(v: f32) -> i64 {
    if v != v { return I32_INDEF; }
    if v >= 2147483648.0 { return I32_INDEF; }
    if v < -2147483648.0 { return I32_INDEF; }
    return v as i64;
}

fn u8of(v: f32) -> i64 {
    return f2i32(v) & 255;
}

// get_scale_min_k4 from ggml-quants.c. q is the address of scales[12].
fn scale_min_k4(j: i64, q: u64) -> i64, i64 {
    if j < 4 {
        return ld_u8(q + (j as u64)) & 63, ld_u8(q + ((j + 4) as u64)) & 63;
    }
    let d = (ld_u8(q + ((j + 4) as u64)) & 15) | ((ld_u8(q + ((j - 4) as u64)) >> 6) << 4);
    let m = (ld_u8(q + ((j + 4) as u64)) >> 4) | ((ld_u8(q + (j as u64)) >> 6) << 4);
    return d, m;
}

// ---- make_qkx2_quants -----------------------------------------------------
// n and nmax are compile-time small; x, w, L, Laux and the_min are addresses.
// The expression order follows the native function exactly, including the
// weighted sums and the selection rule `cur_error < best_error`.
fn make_qkx2(n: i64, nmax: i64, x: u64, w: u64, L: u64, laux: u64, the_min: u64,
             rmin: f32, rdelta: f32, nstep: i64, use_mad: bool) -> f32 {
    var mn = ld_f32(x);
    var mx = ld_f32(x);
    var sum_w = ld_f32(w);
    var sum_x = sum_w * ld_f32(x);
    var i = 1 as i64;
    while i < n {
        let xi = ld_f32(x + (i as u64) * 4);
        if xi < mn { mn = xi; }
        if xi > mx { mx = xi; }
        let wi = ld_f32(w + (i as u64) * 4);
        sum_w += wi;
        sum_x += wi * xi;
        i += 1 as i64;
    }
    if mn > 0 as f32 { mn = 0 as f32; }
    if mx == mn {
        i = 0 as i64;
        while i < n {
            st_u8(L + (i as u64), 0);
            i += 1 as i64;
        }
        st_f32(the_min, -mn);
        return 0 as f32;
    }
    var iscale = nmax as f32 / (mx - mn);
    var scale = (1 as f32) / iscale;
    var best_error = 0 as f32;
    i = 0 as i64;
    while i < n {
        let l0 = nearest(iscale * (ld_f32(x + (i as u64) * 4) - mn));
        let l = imax(0 as i64, imin(nmax, l0));
        st_u8(L + (i as u64), l);
        var diff = scale * (l as f32) + mn - ld_f32(x + (i as u64) * 4);
        if use_mad { diff = fabs32(diff); } else { diff = diff * diff; }
        let wi = ld_f32(w + (i as u64) * 4);
        best_error += wi * diff;
        i += 1 as i64;
    }
    if nstep < 1 {
        st_f32(the_min, -mn);
        return scale;
    }
    var is = 0 as i64;
    while is <= nstep {
        iscale = (rmin + rdelta * (is as f32) + (nmax as f32)) / (mx - mn);
        var sum_l = 0 as f32;
        var sum_l2 = 0 as f32;
        var sum_xl = 0 as f32;
        i = 0 as i64;
        while i < n {
            let l0 = nearest(iscale * (ld_f32(x + (i as u64) * 4) - mn));
            let l = imax(0 as i64, imin(nmax, l0));
            st_u8(laux + (i as u64), l);
            let wi = ld_f32(w + (i as u64) * 4);
            sum_l += wi * (l as f32);
            sum_l2 += wi * (l as f32) * (l as f32);
            sum_xl += wi * (l as f32) * ld_f32(x + (i as u64) * 4);
            i += 1 as i64;
        }
        let D = sum_w * sum_l2 - sum_l * sum_l;
        if D > 0 as f32 {
            var this_scale = (sum_w * sum_xl - sum_x * sum_l) / D;
            var this_min = (sum_l2 * sum_x - sum_l * sum_xl) / D;
            if this_min > 0 as f32 {
                this_min = 0 as f32;
                this_scale = sum_xl / sum_l2;
            }
            var cur_error = 0 as f32;
            i = 0 as i64;
            while i < n {
                var diff = this_scale * (ld_u8(laux + (i as u64)) as f32) + this_min - ld_f32(x + (i as u64) * 4);
                if use_mad { diff = fabs32(diff); } else { diff = diff * diff; }
                let wi = ld_f32(w + (i as u64) * 4);
                cur_error += wi * diff;
                i += 1 as i64;
            }
            if cur_error < best_error {
                i = 0 as i64;
                while i < n {
                    st_u8(L + (i as u64), ld_u8(laux + (i as u64)));
                    i += 1 as i64;
                }
                best_error = cur_error;
                scale = this_scale;
                mn = this_min;
            }
        }
        is += 1 as i64;
    }
    st_f32(the_min, -mn);
    return scale;
}

// ---- make_q3_quants -------------------------------------------------------
// The do_rmse == true branch is the only call in this slice (q3_K).
fn make_q3(n: i64, nmax: i64, x: u64, L: u64) -> f32 {
    var mx = 0 as f32;
    var amax = 0 as f32;
    var i = 0 as i64;
    while i < n {
        let ax = fabs32(ld_f32(x + (i as u64) * 4));
        if ax > amax {
            amax = ax;
            mx = ld_f32(x + (i as u64) * 4);
        }
        i += 1 as i64;
    }
    if amax < 1.0e-15 {
        i = 0 as i64;
        while i < n {
            st_u8(L + (i as u64), 0);
            i += 1 as i64;
        }
        return 0 as f32;
    }
    var iscale = -(nmax as f32) / mx;
    var sumlx = 0 as f32;
    var suml2 = 0 as f32;
    i = 0 as i64;
    while i < n {
        let xi = ld_f32(x + (i as u64) * 4);
        let l0 = nearest(iscale * xi);
        let l = imax(0 as i64 - nmax, imin(nmax - 1, l0));
        st_u8(L + (i as u64), l);
        let w = xi * xi;
        sumlx += w * xi * (l as f32);
        suml2 += w * (l as f32) * (l as f32);
        i += 1 as i64;
    }
    var itry = 0 as i64;
    while itry < 5 {
        var n_changed = 0 as i64;
        i = 0 as i64;
        while i < n {
            let xi = ld_f32(x + (i as u64) * 4);
            let li = i8val(ld_u8(L + (i as u64)));
            let w = xi * xi;
            let slx = sumlx - w * xi * (li as f32);
            if slx > 0 as f32 {
                let sl2 = suml2 - w * (li as f32) * (li as f32);
                var new_l = nearest(xi * sl2 / slx);
                new_l = imax(0 as i64 - nmax, imin(nmax - 1, new_l));
                if new_l != li {
                    let slx2 = slx + w * xi * (new_l as f32);
                    let sl22 = sl2 + w * (new_l as f32) * (new_l as f32);
                    if sl22 > 0 as f32 && slx2 * slx2 * suml2 > sumlx * sumlx * sl22 {
                        st_u8(L + (i as u64), new_l);
                        sumlx = slx2;
                        suml2 = sl22;
                        n_changed += 1 as i64;
                    }
                }
            }
            i += 1 as i64;
        }
        if n_changed == 0 { break; }
        itry += 1 as i64;
    }
    i = 0 as i64;
    while i < n {
        st_u8(L + (i as u64), ld_u8(L + (i as u64)) + nmax);
        i += 1 as i64;
    }
    if suml2 > 0 as f32 { return sumlx / suml2; }
    return 0 as f32;
}

// ---- make_qx_quants -------------------------------------------------------
// rmse_type 1 with qw == 0 is the q6_K call; the other branches are kept so the
// function is the native one, not a narrowed copy.
fn make_qx(n: i64, nmax: i64, x: u64, L: u64, rmse_type: i64, qw: u64) -> f32 {
    var mx = 0 as f32;
    var amax = 0 as f32;
    var i = 0 as i64;
    while i < n {
        let ax = fabs32(ld_f32(x + (i as u64) * 4));
        if ax > amax {
            amax = ax;
            mx = ld_f32(x + (i as u64) * 4);
        }
        i += 1 as i64;
    }
    if amax < 1.0e-15 {
        i = 0 as i64;
        while i < n {
            st_u8(L + (i as u64), 0);
            i += 1 as i64;
        }
        return 0 as f32;
    }
    var iscale = -(nmax as f32) / mx;
    var rtype = rmse_type;
    if rtype == 0 {
        i = 0 as i64;
        while i < n {
            let l = nearest(iscale * ld_f32(x + (i as u64) * 4));
            st_u8(L + (i as u64), nmax + imax(0 as i64 - nmax, imin(nmax - 1, l)));
            i += 1 as i64;
        }
        return (1 as f32) / iscale;
    }
    var return_early = false;
    if rtype < 0 {
        rtype = 0 - rtype;
        return_early = true;
    }
    var sumlx = 0 as f32;
    var suml2 = 0 as f32;
    i = 0 as i64;
    while i < n {
        let xi = ld_f32(x + (i as u64) * 4);
        let l0 = nearest(iscale * xi);
        let l = imax(0 as i64 - nmax, imin(nmax - 1, l0));
        st_u8(L + (i as u64), l + nmax);
        var w = xi * xi;
        if qw != 0 { w = ld_f32(qw + (i as u64) * 4); }
        else if rtype == 2 { w = 1 as f32; }
        else if rtype == 3 { w = fabs32(xi); }
        sumlx += w * xi * (l as f32);
        suml2 += w * (l as f32) * (l as f32);
        i += 1 as i64;
    }
    var scale = 0 as f32;
    if suml2 != 0 as f32 { scale = sumlx / suml2; }
    if return_early {
        if suml2 > 0 as f32 { return 0.5 * (scale + (1 as f32) / iscale); }
        return (1 as f32) / iscale;
    }
    var best = scale * sumlx;
    var is = 0 as i64 - 9;
    while is <= 9 {
        if is != 0 {
            iscale = -((nmax as f32) + 0.1 * (is as f32)) / mx;
            sumlx = 0 as f32;
            suml2 = 0 as f32;
            i = 0 as i64;
            while i < n {
                let xi = ld_f32(x + (i as u64) * 4);
                let l = imax(0 as i64 - nmax, imin(nmax - 1, nearest(iscale * xi)));
                var w = xi * xi;
                if qw != 0 { w = ld_f32(qw + (i as u64) * 4); }
                else if rtype == 2 { w = 1 as f32; }
                else if rtype == 3 { w = fabs32(xi); }
                sumlx += w * xi * (l as f32);
                suml2 += w * (l as f32) * (l as f32);
                i += 1 as i64;
            }
            if suml2 > 0 as f32 && sumlx * sumlx > best * suml2 {
                i = 0 as i64;
                while i < n {
                    let l = nearest(iscale * ld_f32(x + (i as u64) * 4));
                    st_u8(L + (i as u64), nmax + imax(0 as i64 - nmax, imin(nmax - 1, l)));
                    i += 1 as i64;
                }
                scale = sumlx / suml2;
                best = scale * sumlx;
            }
        }
        is += 1 as i64;
    }
    return scale;
}

// ---- q2_K -----------------------------------------------------------------

fn qz2_row_q2_K(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    let L = tmp(S_L);
    let laux = tmp(S_LAUX);
    let wa = tmp(S_WEIGHTS);
    let mnbuf = tmp(S_TMP);
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_Q2_K) as u64;
        var max_scale = 0 as f32;
        var max_min = 0 as f32;
        var j = 0 as i64;
        while j < QK_K / 16 {
            var l = 0 as i64;
            while l < 16 {
                st_f32(wa + (l as u64) * 4, fabs32(ld_f32(xb + ((j * 16 + l) as u64) * 4)));
                l += 1 as i64;
            }
            let sc = make_qkx2(16, 3, xb + ((j * 16) as u64) * 4, wa, L + (j * 16) as u64, laux, mnbuf,
                               0 as f32 - 0.5, 0.1, 15, true);
            ssetf(S_SCALES + j * 4, sc);
            ssetf(S_MINS + j * 4, ld_f32(mnbuf));
            if sc > max_scale { max_scale = sc; }
            if ld_f32(mnbuf) > max_min { max_min = ld_f32(mnbuf); }
            j += 1 as i64;
        }
        if max_scale > 0 as f32 {
            let isc = 15.0 / max_scale;
            j = 0 as i64;
            while j < QK_K / 16 {
                st_u8(yb + (j as u64), nearest(isc * sgetf(S_SCALES + j * 4)));
                j += 1 as i64;
            }
            st_f16(yb + 80, max_scale / 15.0);
        } else {
            j = 0 as i64;
            while j < QK_K / 16 {
                st_u8(yb + (j as u64), 0);
                j += 1 as i64;
            }
            st_f16(yb + 80, 0 as f32);
        }
        if max_min > 0 as f32 {
            let isc = 15.0 / max_min;
            j = 0 as i64;
            while j < QK_K / 16 {
                let l = nearest(isc * sgetf(S_MINS + j * 4));
                st_u8(yb + (j as u64), ld_u8(yb + (j as u64)) | (l << 4));
                j += 1 as i64;
            }
            st_f16(yb + 82, max_min / 15.0);
        } else {
            st_f16(yb + 82, 0 as f32);
        }
        j = 0 as i64;
        while j < QK_K / 16 {
            let d = ld_f16(yb + 80) * ((ld_u8(yb + (j as u64)) & 15) as f32);
            if d != 0 as f32 {
                let dm = ld_f16(yb + 82) * ((ld_u8(yb + (j as u64)) >> 4) as f32);
                var ii = 0 as i64;
                while ii < 16 {
                    let l0 = nearest((ld_f32(xb + ((j * 16 + ii) as u64) * 4) + dm) / d);
                    st_u8(L + ((j * 16 + ii) as u64), imax(0 as i64, imin(3, l0)));
                    ii += 1 as i64;
                }
            }
            j += 1 as i64;
        }
        var jo = 0 as i64;
        while jo < QK_K {
            var l = 0 as i64;
            while l < 32 {
                let q0 = ld_u8(L + ((jo + l) as u64));
                let q1 = ld_u8(L + ((jo + l + 32) as u64));
                let q2 = ld_u8(L + ((jo + l + 64) as u64));
                let q3 = ld_u8(L + ((jo + l + 96) as u64));
                st_u8(yb + 16 + ((jo / 4 + l) as u64), q0 | (q1 << 2) | (q2 << 4) | (q3 << 6));
                l += 1 as i64;
            }
            jo += 128 as i64;
        }
        i += 1 as i64;
    }
}

// ---- q3_K -----------------------------------------------------------------

fn qz2_row_q3_K(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    let L = tmp(S_L);
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_Q3_K) as u64;
        var max_scale = 0 as f32;
        var amax = 0 as f32;
        var j = 0 as i64;
        while j < QK_K / 16 {
            let sc = make_q3(16, 4, xb + ((j * 16) as u64) * 4, L + (j * 16) as u64);
            ssetf(S_SCALES + j * 4, sc);
            let asc = fabs32(sc);
            if asc > amax {
                amax = asc;
                max_scale = sc;
            }
            j += 1 as i64;
        }
        j = 0 as i64;
        while j < 12 {
            st_u8(yb + 96 + (j as u64), 0);
            j += 1 as i64;
        }
        if max_scale != 0 as f32 {
            let isc = -32.0 / max_scale;
            j = 0 as i64;
            while j < QK_K / 16 {
                var l = nearest(isc * sgetf(S_SCALES + j * 4));
                l = imax(0 as i64 - 32, imin(31, l)) + 32;
                if j < 8 {
                    st_u8(yb + 96 + (j as u64), l & 15);
                } else {
                    st_u8(yb + 96 + ((j - 8) as u64), ld_u8(yb + 96 + ((j - 8) as u64)) | ((l & 15) << 4));
                }
                let hi = l >> 4;
                let at = yb + 96 + ((j % 4 + 8) as u64);
                st_u8(at, ld_u8(at) | (hi << (2 * (j / 4))));
                j += 1 as i64;
            }
            st_f16(yb + 108, (1 as f32) / isc);
        } else {
            st_f16(yb + 108, 0 as f32);
        }
        j = 0 as i64;
        while j < QK_K / 16 {
            var sc = 0 as i64;
            if j < 8 { sc = ld_u8(yb + 96 + (j as u64)) & 15; }
            else { sc = ld_u8(yb + 96 + ((j - 8) as u64)) >> 4; }
            sc = (sc | (((ld_u8(yb + 96 + ((j % 4 + 8) as u64)) >> (2 * (j / 4))) & 3) << 4)) - 32;
            let d = ld_f16(yb + 108) * (sc as f32);
            if d != 0 as f32 {
                var ii = 0 as i64;
                while ii < 16 {
                    let l0 = nearest(ld_f32(xb + ((j * 16 + ii) as u64) * 4) / d);
                    st_u8(L + ((j * 16 + ii) as u64), imax(0 as i64 - 4, imin(3, l0)) + 4);
                    ii += 1 as i64;
                }
            }
            j += 1 as i64;
        }
        var m = 0 as i64;
        while m < QK_K / 8 {
            st_u8(yb + (m as u64), 0);
            m += 1 as i64;
        }
        var mb = 0 as i64;
        var hm = 1 as i64;
        j = 0 as i64;
        while j < QK_K {
            if ld_u8(L + (j as u64)) > 3 {
                st_u8(yb + (mb as u64), ld_u8(yb + (mb as u64)) | hm);
                st_u8(L + (j as u64), ld_u8(L + (j as u64)) - 4);
            }
            mb += 1 as i64;
            if mb == QK_K / 8 {
                mb = 0 as i64;
                hm = hm << 1;
            }
            j += 1 as i64;
        }
        var jo = 0 as i64;
        while jo < QK_K {
            var l = 0 as i64;
            while l < 32 {
                let q0 = ld_u8(L + ((jo + l) as u64));
                let q1 = ld_u8(L + ((jo + l + 32) as u64));
                let q2 = ld_u8(L + ((jo + l + 64) as u64));
                let q3 = ld_u8(L + ((jo + l + 96) as u64));
                st_u8(yb + 32 + ((jo / 4 + l) as u64), q0 | (q1 << 2) | (q2 << 4) | (q3 << 6));
                l += 1 as i64;
            }
            jo += 128 as i64;
        }
        i += 1 as i64;
    }
}

// ---- q4_K -----------------------------------------------------------------

fn qz2_row_q4_K(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    let L = tmp(S_L);
    let laux = tmp(S_LAUX);
    let wa = tmp(S_WEIGHTS);
    let mnbuf = tmp(S_TMP);
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_Q4_K) as u64;
        var max_scale = 0 as f32;
        var max_min = 0 as f32;
        var j = 0 as i64;
        while j < QK_K / 32 {
            var sum_x2 = 0 as f32;
            var l = 0 as i64;
            while l < 32 {
                let xv = ld_f32(xb + ((j * 32 + l) as u64) * 4);
                sum_x2 += xv * xv;
                l += 1 as i64;
            }
            let av_x = sqrt32(sum_x2 / 32.0);
            l = 0 as i64;
            while l < 32 {
                st_f32(wa + (l as u64) * 4, av_x + fabs32(ld_f32(xb + ((j * 32 + l) as u64) * 4)));
                l += 1 as i64;
            }
            let sc = make_qkx2(32, 15, xb + ((j * 32) as u64) * 4, wa, L + (j * 32) as u64, laux, mnbuf,
                               0 as f32 - 1.0, 0.1, 20, false);
            ssetf(S_SCALES + j * 4, sc);
            ssetf(S_MINS + j * 4, ld_f32(mnbuf));
            if sc > max_scale { max_scale = sc; }
            if ld_f32(mnbuf) > max_min { max_min = ld_f32(mnbuf); }
            j += 1 as i64;
        }
        var inv_scale = 0 as f32;
        var inv_min = 0 as f32;
        if max_scale > 0 as f32 { inv_scale = 63.0 / max_scale; }
        if max_min > 0 as f32 { inv_min = 63.0 / max_min; }
        j = 0 as i64;
        while j < QK_K / 32 {
            var ls = nearest(inv_scale * sgetf(S_SCALES + j * 4)) & 255;
            var lm = nearest(inv_min * sgetf(S_MINS + j * 4)) & 255;
            ls = imin(63, ls);
            lm = imin(63, lm);
            if j < 4 {
                st_u8(yb + 4 + (j as u64), ls);
                st_u8(yb + 4 + ((j + 4) as u64), lm);
            } else {
                st_u8(yb + 4 + ((j + 4) as u64), (ls & 15) | ((lm & 15) << 4));
                st_u8(yb + 4 + ((j - 4) as u64), ld_u8(yb + 4 + ((j - 4) as u64)) | ((ls >> 4) << 6));
                st_u8(yb + 4 + (j as u64), ld_u8(yb + 4 + (j as u64)) | ((lm >> 4) << 6));
            }
            j += 1 as i64;
        }
        st_f16(yb, max_scale / 63.0);
        st_f16(yb + 2, max_min / 63.0);
        j = 0 as i64;
        while j < QK_K / 32 {
            let sc0, m0 = scale_min_k4(j, yb + 4);
            let d = ld_f16(yb) * (sc0 as f32);
            if d != 0 as f32 {
                let dm = ld_f16(yb + 2) * (m0 as f32);
                var ii = 0 as i64;
                while ii < 32 {
                    let l0 = nearest((ld_f32(xb + ((j * 32 + ii) as u64) * 4) + dm) / d);
                    st_u8(L + ((j * 32 + ii) as u64), imax(0 as i64, imin(15, l0)));
                    ii += 1 as i64;
                }
            }
            j += 1 as i64;
        }
        var jo = 0 as i64;
        while jo < QK_K {
            var l = 0 as i64;
            while l < 32 {
                let q0 = ld_u8(L + ((jo + l) as u64));
                let q1 = ld_u8(L + ((jo + l + 32) as u64));
                st_u8(yb + 16 + ((jo / 2 + l) as u64), q0 | (q1 << 4));
                l += 1 as i64;
            }
            jo += 64 as i64;
        }
        i += 1 as i64;
    }
}

// ---- q5_K -----------------------------------------------------------------

fn qz2_row_q5_K(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    let L = tmp(S_L);
    let laux = tmp(S_LAUX);
    let wa = tmp(S_WEIGHTS);
    let mnbuf = tmp(S_TMP);
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_Q5_K) as u64;
        var max_scale = 0 as f32;
        var max_min = 0 as f32;
        var j = 0 as i64;
        while j < QK_K / 32 {
            var sum_x2 = 0 as f32;
            var l = 0 as i64;
            while l < 32 {
                let xv = ld_f32(xb + ((j * 32 + l) as u64) * 4);
                sum_x2 += xv * xv;
                l += 1 as i64;
            }
            let av_x = sqrt32(sum_x2 / 32.0);
            l = 0 as i64;
            while l < 32 {
                st_f32(wa + (l as u64) * 4, av_x + fabs32(ld_f32(xb + ((j * 32 + l) as u64) * 4)));
                l += 1 as i64;
            }
            let sc = make_qkx2(32, 31, xb + ((j * 32) as u64) * 4, wa, L + (j * 32) as u64, laux, mnbuf,
                               0 as f32 - 0.5, 0.1, 15, false);
            ssetf(S_SCALES + j * 4, sc);
            ssetf(S_MINS + j * 4, ld_f32(mnbuf));
            if sc > max_scale { max_scale = sc; }
            if ld_f32(mnbuf) > max_min { max_min = ld_f32(mnbuf); }
            j += 1 as i64;
        }
        var inv_scale = 0 as f32;
        var inv_min = 0 as f32;
        if max_scale > 0 as f32 { inv_scale = 63.0 / max_scale; }
        if max_min > 0 as f32 { inv_min = 63.0 / max_min; }
        j = 0 as i64;
        while j < QK_K / 32 {
            var ls = nearest(inv_scale * sgetf(S_SCALES + j * 4)) & 255;
            var lm = nearest(inv_min * sgetf(S_MINS + j * 4)) & 255;
            ls = imin(63, ls);
            lm = imin(63, lm);
            if j < 4 {
                st_u8(yb + 4 + (j as u64), ls);
                st_u8(yb + 4 + ((j + 4) as u64), lm);
            } else {
                st_u8(yb + 4 + ((j + 4) as u64), (ls & 15) | ((lm & 15) << 4));
                st_u8(yb + 4 + ((j - 4) as u64), ld_u8(yb + 4 + ((j - 4) as u64)) | ((ls >> 4) << 6));
                st_u8(yb + 4 + (j as u64), ld_u8(yb + 4 + (j as u64)) | ((lm >> 4) << 6));
            }
            j += 1 as i64;
        }
        st_f16(yb, max_scale / 63.0);
        st_f16(yb + 2, max_min / 63.0);
        j = 0 as i64;
        while j < QK_K / 32 {
            let sc0, m0 = scale_min_k4(j, yb + 4);
            let d = ld_f16(yb) * (sc0 as f32);
            if d != 0 as f32 {
                let dm = ld_f16(yb + 2) * (m0 as f32);
                var ii = 0 as i64;
                while ii < 32 {
                    let l0 = nearest((ld_f32(xb + ((j * 32 + ii) as u64) * 4) + dm) / d);
                    st_u8(L + ((j * 32 + ii) as u64), imax(0 as i64, imin(31, l0)));
                    ii += 1 as i64;
                }
            }
            j += 1 as i64;
        }
        var m = 0 as i64;
        while m < QK_K / 8 {
            st_u8(yb + 16 + (m as u64), 0);
            m += 1 as i64;
        }
        var m1 = 1 as i64;
        var m2 = 2 as i64;
        var n = 0 as i64;
        while n < QK_K {
            var l = 0 as i64;
            while l < 32 {
                var l1 = ld_u8(L + ((n + l) as u64));
                if l1 > 15 {
                    l1 -= 16;
                    st_u8(yb + 16 + (l as u64), ld_u8(yb + 16 + (l as u64)) | m1);
                }
                var l2 = ld_u8(L + ((n + l + 32) as u64));
                if l2 > 15 {
                    l2 -= 16;
                    st_u8(yb + 16 + (l as u64), ld_u8(yb + 16 + (l as u64)) | m2);
                }
                st_u8(yb + 48 + ((n / 2 + l) as u64), l1 | (l2 << 4));
                l += 1 as i64;
            }
            m1 = m1 << 2;
            m2 = m2 << 2;
            n += 64 as i64;
        }
        i += 1 as i64;
    }
}

// ---- q6_K -----------------------------------------------------------------

fn qz2_row_q6_K(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    let L = tmp(S_L);
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_Q6_K) as u64;
        var max_scale = 0 as f32;
        var max_abs_scale = 0 as f32;
        var ib = 0 as i64;
        while ib < QK_K / 16 {
            let sc = make_qx(16, 32, xb + ((ib * 16) as u64) * 4, L + (ib * 16) as u64, 1, 0 as u64);
            ssetf(S_SCALES + ib * 4, sc);
            let asc = fabs32(sc);
            if asc > max_abs_scale {
                max_abs_scale = asc;
                max_scale = sc;
            }
            ib += 1 as i64;
        }
        if max_abs_scale < 1.0e-15 {
            var z = 0 as i64;
            while z < B_Q6_K {
                st_u8(yb + (z as u64), 0);
                z += 1 as i64;
            }
            st_f16(yb + 208, 0 as f32);
            i += 1 as i64;
            continue;
        }
        let iscale = -128.0 / max_scale;
        st_f16(yb + 208, (1 as f32) / iscale);
        ib = 0 as i64;
        while ib < QK_K / 16 {
            st_u8(yb + 192 + (ib as u64), imin(127, nearest(iscale * sgetf(S_SCALES + ib * 4))));
            ib += 1 as i64;
        }
        var j = 0 as i64;
        while j < QK_K / 16 {
            let d = ld_f16(yb + 208) * (i8val(ld_u8(yb + 192 + (j as u64))) as f32);
            if d != 0 as f32 {
                var ii = 0 as i64;
                while ii < 16 {
                    let l0 = nearest(ld_f32(xb + ((j * 16 + ii) as u64) * 4) / d);
                    st_u8(L + ((j * 16 + ii) as u64), imax(0 as i64 - 32, imin(31, l0)) + 32);
                    ii += 1 as i64;
                }
            }
            j += 1 as i64;
        }
        var jo = 0 as i64;
        while jo < QK_K {
            var l = 0 as i64;
            while l < 32 {
                let q1 = ld_u8(L + ((jo + l) as u64)) & 15;
                let q2 = ld_u8(L + ((jo + l + 32) as u64)) & 15;
                let q3 = ld_u8(L + ((jo + l + 64) as u64)) & 15;
                let q4 = ld_u8(L + ((jo + l + 96) as u64)) & 15;
                st_u8(yb + (jo / 2 + l) as u64, q1 | (q3 << 4));
                st_u8(yb + (jo / 2 + l + 32) as u64, q2 | (q4 << 4));
                st_u8(yb + 128 + (jo / 4 + l) as u64,
                      (ld_u8(L + ((jo + l) as u64)) >> 4)
                      | ((ld_u8(L + ((jo + l + 32) as u64)) >> 4) << 2)
                      | ((ld_u8(L + ((jo + l + 64) as u64)) >> 4) << 4)
                      | ((ld_u8(L + ((jo + l + 96) as u64)) >> 4) << 6));
                l += 1 as i64;
            }
            jo += 128 as i64;
        }
        i += 1 as i64;
    }
}

// ---- q8_K -----------------------------------------------------------------

fn qz2_row_q8_K(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_Q8_K) as u64;
        var mx = 0 as f32;
        var amax = 0 as f32;
        var j = 0 as i64;
        while j < QK_K {
            let ax = fabs32(ld_f32(xb + (j as u64) * 4));
            if ax > amax {
                amax = ax;
                mx = ld_f32(xb + (j as u64) * 4);
            }
            j += 1 as i64;
        }
        if amax == 0 as f32 {
            st_f32(yb, 0 as f32);
            j = 0 as i64;
            while j < QK_K {
                st_u8(yb + 4 + (j as u64), 0);
                j += 1 as i64;
            }
            i += 1 as i64;
            continue;
        }
        let iscale = -127.0 / mx;
        j = 0 as i64;
        while j < QK_K {
            var v = nearest(iscale * ld_f32(xb + (j as u64) * 4));
            if v > 127 { v = 127; }
            st_u8(yb + 4 + (j as u64), v);
            j += 1 as i64;
        }
        j = 0 as i64;
        while j < QK_K / 16 {
            var sum = 0 as i64;
            var ii = 0 as i64;
            while ii < 16 {
                sum += i8val(ld_u8(yb + 4 + ((j * 16 + ii) as u64)));
                ii += 1 as i64;
            }
            st_u16(yb + 260 + (j * 2) as u64, sum & 65535);
            j += 1 as i64;
        }
        st_f32(yb, (1 as f32) / iscale);
        i += 1 as i64;
    }
}

// ---- TQ1_0 and TQ2_0 ------------------------------------------------------

fn qz2_row_tq1_0(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_TQ1_0) as u64;
        var amax = 0 as f32;
        var j = 0 as i64;
        while j < QK_K {
            amax = fmax32(amax, fabs32(ld_f32(xb + (j as u64) * 4)));
            j += 1 as i64;
        }
        let d = amax;
        var id = 0 as f32;
        if d != 0 as f32 { id = 1.0 / d; }
        st_f16(yb + 52, d);
        var m = 0 as i64;
        while m < 32 {
            var q = 0 as i64;
            var n = 0 as i64;
            while n < 5 {
                let xi = lround32(ld_f32(xb + ((m + n * 32) as u64) * 4) * id) + 1;
                q = (q * 3) & 255;
                q = (q + xi) & 255;
                n += 1 as i64;
            }
            q = (q * 256 + 242) / 243;
            st_u8(yb + (m as u64), q);
            m += 1 as i64;
        }
        m = 0 as i64;
        while m < 16 {
            var q = 0 as i64;
            var n = 0 as i64;
            while n < 5 {
                let xi = lround32(ld_f32(xb + ((160 + m + n * 16) as u64) * 4) * id) + 1;
                q = (q * 3) & 255;
                q = (q + xi) & 255;
                n += 1 as i64;
            }
            q = (q * 256 + 242) / 243;
            st_u8(yb + (32 + m) as u64, q);
            m += 1 as i64;
        }
        m = 0 as i64;
        while m < 4 {
            var q = 0 as i64;
            var n = 0 as i64;
            while n < 4 {
                let xi = lround32(ld_f32(xb + ((240 + m + n * 4) as u64) * 4) * id) + 1;
                q = (q * 3) & 255;
                q = (q + xi) & 255;
                n += 1 as i64;
            }
            q = (q * 3) & 255;
            q = (q * 256 + 242) / 243;
            st_u8(yb + 48 + (m as u64), q);
            m += 1 as i64;
        }
        i += 1 as i64;
    }
}

fn qz2_row_tq2_0(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_TQ2_0) as u64;
        var amax = 0 as f32;
        var j = 0 as i64;
        while j < QK_K {
            amax = fmax32(amax, fabs32(ld_f32(xb + (j as u64) * 4)));
            j += 1 as i64;
        }
        let d = amax;
        var id = 0 as f32;
        if d != 0 as f32 { id = 1.0 / d; }
        st_f16(yb + 64, d);
        var e = 0 as i64;
        while e < 2 {
            var m = 0 as i64;
            while m < 32 {
                var q = 0 as i64;
                var n = 0 as i64;
                while n < 4 {
                    let xi = lround32(ld_f32(xb + ((e * 128 + m + n * 32) as u64) * 4) * id) + 1;
                    q = q | ((xi & 3) << (2 * n));
                    n += 1 as i64;
                }
                st_u8(yb + (e * 32 + m) as u64, q & 255);
                m += 1 as i64;
            }
            e += 1 as i64;
        }
        i += 1 as i64;
    }
}

// ---- MXFP4 and NVFP4 ------------------------------------------------------

fn kf4(i: i64) -> i64 {
    if i == 0 { return 0; }
    if i == 1 { return 1; }
    if i == 2 { return 2; }
    if i == 3 { return 3; }
    if i == 4 { return 4; }
    if i == 5 { return 6; }
    if i == 6 { return 8; }
    if i == 7 { return 12; }
    if i == 8 { return 0; }
    if i == 9 { return -1; }
    if i == 10 { return -2; }
    if i == 11 { return -3; }
    if i == 12 { return -4; }
    if i == 13 { return -6; }
    if i == 14 { return -8; }
    return -12;
}

// GGML_E8M0_TO_FP32_HALF
fn e8m0_half(e: i64) -> f32 {
    var bits = 0 as i64;
    if e < 2 {
        bits = 2097152 << e;
    } else {
        bits = (e - 1) << 23;
    }
    return f32_of_bits(bits);
}

fn best_index_mxfp4(xv: f32, e: f32) -> i64 {
    var best = 0 as i64;
    var best_err = fabs32((kf4(0) as f32) * e - xv);
    var i = 1 as i64;
    while i < 16 {
        let err = fabs32((kf4(i) as f32) * e - xv);
        if err < best_err {
            best = i;
            best_err = err;
        }
        i += 1 as i64;
    }
    return best;
}

fn qz2_row_mxfp4(x: u64, y: u64, k: i64) {
    let nb = k / QK_MXFP4;
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_MXFP4) as u64 * 4;
        let yb = y + (i * B_MXFP4) as u64;
        var amax = 0 as f32;
        var j = 0 as i64;
        while j < QK_MXFP4 {
            let a = fabs32(ld_f32(xb + (j as u64) * 4));
            if amax < a { amax = a; }
            j += 1 as i64;
        }
        var e = 0 as i64;
        if amax > 0 as f32 {
            e = u8of(floor32(log232(amax)) - 2.0 + 127.0);
        }
        let d = e8m0_half(e);
        st_u8(yb, e);
        j = 0 as i64;
        while j < QK_MXFP4 / 2 {
            let x0 = best_index_mxfp4(ld_f32(xb + (j as u64) * 4), d);
            let x1 = best_index_mxfp4(ld_f32(xb + ((j + QK_MXFP4 / 2) as u64) * 4), d);
            st_u8(yb + 1 + (j as u64), x0 | (x1 << 4));
            j += 1 as i64;
        }
        i += 1 as i64;
    }
}

fn ue4m3_from_f32(x0: f32) -> i64 {
    var x = x0;
    if !(x > 0 as f32) { return 0; }
    if x > 448.0 { x = 448.0; }
    let bits = f32_bits(x);
    let fp32_exp = ((bits >> 23) & 255) - 127;
    let fp32_man = (bits >> 20) & 7;
    var ue_exp = fp32_exp + 7;
    if ue_exp <= 0 {
        var man = f2i32(x * 512.0 + 0.5);
        if man > 7 { man = 7; }
        if man < 1 { return 0; }
        return man;
    }
    if ue_exp >= 15 { return 126; }
    let round_bit = (bits >> 19) & 1;
    var ue_man = fp32_man + round_bit;
    if ue_man > 7 {
        ue_man = 0;
        ue_exp += 1;
        if ue_exp >= 15 { return 126; }
    }
    return (ue_exp << 3) | ue_man;
}

fn ue4m3_to_f32(ue: i64) -> f32 {
    if ue == 0 || ue == 127 { return 0 as f32; }
    let e = (ue >> 3) & 15;
    let man = ue & 7;
    var raw = 0 as f32;
    if e == 0 {
        raw = ldexp32(man as f32, 0 as i64 - 9);
    } else {
        raw = ldexp32(1.0 + (man as f32) / 8.0, e - 7);
    }
    return raw * 0.5;
}

fn qz2_row_nvfp4(x: u64, y: u64, k: i64) {
    let nb = k / QK_NVFP4;
    let nsub = QK_NVFP4 / QK_NVFP4_SUB;
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_NVFP4) as u64 * 4;
        let yb = y + (i * B_NVFP4) as u64;
        var s = 0 as i64;
        while s < nsub {
            let xs = xb + (s * QK_NVFP4_SUB) as u64 * 4;
            var amax = 0 as f32;
            var j = 0 as i64;
            while j < QK_NVFP4_SUB {
                let a = fabs32(ld_f32(xs + (j as u64) * 4));
                if amax < a { amax = a; }
                j += 1 as i64;
            }
            let ue = ue4m3_from_f32(amax / 6.0);
            st_u8(yb + (s as u64), ue);
            let d = ue4m3_to_f32(ue);
            j = 0 as i64;
            while j < QK_NVFP4_SUB / 2 {
                let x0 = best_index_mxfp4(ld_f32(xs + (j as u64) * 4), d);
                let x1 = best_index_mxfp4(ld_f32(xs + ((j + QK_NVFP4_SUB / 2) as u64) * 4), d);
                st_u8(yb + 4 + (s * (QK_NVFP4_SUB / 2) + j) as u64, x0 | (x1 << 4));
                j += 1 as i64;
            }
            s += 1 as i64;
        }
        i += 1 as i64;
    }
}

// ---- IQ4_NL / IQ4_XS ------------------------------------------------------

fn iq4nl_val(i: i64) -> i64 {
    if i == 0 { return -127; }
    if i == 1 { return -104; }
    if i == 2 { return -83; }
    if i == 3 { return -65; }
    if i == 4 { return -49; }
    if i == 5 { return -35; }
    if i == 6 { return -22; }
    if i == 7 { return -10; }
    if i == 8 { return 1; }
    if i == 9 { return 13; }
    if i == 10 { return 25; }
    if i == 11 { return 38; }
    if i == 12 { return 53; }
    if i == 13 { return 69; }
    if i == 14 { return 89; }
    return 113;
}

fn best_index_int8(xv: f32) -> i64 {
    if xv <= (iq4nl_val(0) as f32) { return 0; }
    if xv >= (iq4nl_val(15) as f32) { return 15; }
    var ml = 0 as i64;
    var mu = 15 as i64;
    while mu - ml > 1 {
        let mav = (ml + mu) / 2;
        if xv < (iq4nl_val(mav) as f32) { mu = mav; } else { ml = mav; }
    }
    if xv - (iq4nl_val(mu - 1) as f32) < (iq4nl_val(mu) as f32) - xv { return mu - 1; }
    return mu;
}

// One 32-element group search of quantize_row_iq4_nl_impl. xb is the group,
// yb the start of the destination block; ntry is 7 on the chunk path.
// Returns the selected scale; L holds the 32 level indices.
fn iq4_nl_block(xb: u64, L: u64, wa: u64, ntry: i64) -> f32 {
    var j = 0 as i64;
    while j < 32 {
        let xv = ld_f32(xb + (j as u64) * 4);
        st_f32(wa + (j as u64) * 4, xv * xv);
        j += 1 as i64;
    }
    var amax = 0 as f32;
    var mx = 0 as f32;
    j = 0 as i64;
    while j < 32 {
        let xv = ld_f32(xb + (j as u64) * 4);
        let ax = fabs32(xv);
        if ax > amax {
            amax = ax;
            mx = xv;
        }
        j += 1 as i64;
    }
    if amax < 1.0e-15 {
        return 0 as f32;
    }
    var d = -mx / (iq4nl_val(0) as f32);
    if ntry <= 0 { d = mx / (iq4nl_val(0) as f32); }
    var id = (1 as f32) / d;
    var sumqx = 0 as f32;
    var sumq2 = 0 as f32;
    j = 0 as i64;
    while j < 32 {
        let xv = ld_f32(xb + (j as u64) * 4);
        let l = best_index_int8(id * xv);
        st_u8(L + (j as u64), l);
        let q = iq4nl_val(l) as f32;
        let w = ld_f32(wa + (j as u64) * 4);
        sumqx += w * q * xv;
        sumq2 += w * q * q;
        j += 1 as i64;
    }
    d = 0 as f32;
    if sumq2 > 0 as f32 { d = sumqx / sumq2; }
    var best = d * sumqx;
    var itry = 0 as i64 - ntry;
    while itry <= ntry {
        id = ((itry + iq4nl_val(0)) as f32) / mx;
        sumqx = 0 as f32;
        sumq2 = 0 as f32;
        j = 0 as i64;
        while j < 32 {
            let xv = ld_f32(xb + (j as u64) * 4);
            let l = best_index_int8(id * xv);
            let q = iq4nl_val(l) as f32;
            let w = ld_f32(wa + (j as u64) * 4);
            sumqx += w * q * xv;
            sumq2 += w * q * q;
            j += 1 as i64;
        }
        if sumq2 > 0 as f32 && sumqx * sumqx > best * sumq2 {
            d = sumqx / sumq2;
            best = d * sumqx;
        }
        itry += 1 as i64;
    }
    return d;
}

fn qz2_row_iq4_nl(x: u64, y: u64, k: i64) {
    let nb = k / QK4_NL;
    let L = tmp(S_L);
    let wa = tmp(S_WEIGHTS);
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK4_NL) as u64 * 4;
        let yb = y + (i * B_IQ4_NL) as u64;
        var j = 0 as i64;
        while j < QK4_NL / 2 {
            st_u8(yb + 2 + (j as u64), 0);
            j += 1 as i64;
        }
        st_f16(yb, 0 as f32);
        let d = iq4_nl_block(xb, L, wa, 7);
        st_f16(yb, d);
        var id = 0 as f32;
        if d != 0 as f32 { id = (1 as f32) / d; }
        j = 0 as i64;
        while j < 32 {
            st_u8(L + (j as u64), best_index_int8(id * ld_f32(xb + (j as u64) * 4)));
            j += 1 as i64;
        }
        j = 0 as i64;
        while j < 16 {
            st_u8(yb + 2 + (j as u64), ld_u8(L + (j as u64)) | (ld_u8(L + ((j + 16) as u64)) << 4));
            j += 1 as i64;
        }
        i += 1 as i64;
    }
}

fn qz2_row_iq4_xs(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    let L = tmp(S_L);
    let wa = tmp(S_WEIGHTS);
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_IQ4_XS) as u64;
        var j = 0 as i64;
        while j < QK_K / 2 {
            st_u8(yb + 8 + (j as u64), 0);
            j += 1 as i64;
        }
        st_f16(yb, 0 as f32);
        var max_scale = 0 as f32;
        var amax_scale = 0 as f32;
        var ib = 0 as i64;
        while ib < 8 {
            let d = iq4_nl_block(xb + (ib * 32) as u64 * 4, L + (ib * 32) as u64, wa, 7);
            ssetf(S_SCALES + ib * 4, d);
            let ad = fabs32(d);
            if ad > amax_scale {
                amax_scale = ad;
                max_scale = d;
            }
            ib += 1 as i64;
        }
        // multi-block branch (super_block_size / block_size == 8)
        var sh = 0 as i64;
        j = 0 as i64;
        while j < 4 {
            st_u8(yb + 4 + (j as u64), 0);
            j += 1 as i64;
        }
        let sd = -max_scale / 32.0;
        st_f16(yb, sd);
        var id = 0 as f32;
        if sd != 0 as f32 { id = (1 as f32) / sd; }
        ib = 0 as i64;
        while ib < 8 {
            var l = nearest(id * sgetf(S_SCALES + ib * 4));
            l = imax(0 as i64 - 32, imin(31, l));
            let dl = sd * (l as f32);
            var idl = 0 as f32;
            if dl != 0 as f32 { idl = (1 as f32) / dl; }
            var jj = 0 as i64;
            while jj < 32 {
                st_u8(L + ((ib * 32 + jj) as u64), best_index_int8(idl * ld_f32(xb + ((ib * 32 + jj) as u64) * 4)));
                jj += 1 as i64;
            }
            l += 32;
            let ll = l & 15;
            let lh = l >> 4;
            if ib % 2 == 0 {
                st_u8(yb + 4 + ((ib / 2) as u64), ll);
            } else {
                st_u8(yb + 4 + ((ib / 2) as u64), ld_u8(yb + 4 + ((ib / 2) as u64)) | (ll << 4));
            }
            sh = sh | (lh << (2 * (ib % 8)));
            ib += 1 as i64;
        }
        st_u16(yb + 2, sh);
        j = 0 as i64;
        while j < 128 {
            let q0 = ld_u8(L + ((32 * (j / 16) + j % 16) as u64));
            let q1 = ld_u8(L + ((32 * (j / 16) + 16 + j % 16) as u64));
            st_u8(yb + 8 + (j as u64), q0 | (q1 << 4));
            j += 1 as i64;
        }
        i += 1 as i64;
    }
}

// ---- row and chunk entry points -------------------------------------------

fn qz2_rows(t: i64, src: u64, dst: u64, nrows: i64, n_per_row: i64) -> i64 {
    let rs = qz2_row_size(t, n_per_row);
    var r = 0 as i64;
    while r < nrows {
        let x = src + (r * n_per_row) as u64 * 4;
        let y = dst + (r * rs) as u64;
        if t == T_Q2_K { qz2_row_q2_K(x, y, n_per_row); }
        else if t == T_Q3_K { qz2_row_q3_K(x, y, n_per_row); }
        else if t == T_Q4_K { qz2_row_q4_K(x, y, n_per_row); }
        else if t == T_Q5_K { qz2_row_q5_K(x, y, n_per_row); }
        else if t == T_Q6_K { qz2_row_q6_K(x, y, n_per_row); }
        else if t == T_Q8_K { qz2_row_q8_K(x, y, n_per_row); }
        else if t == T_TQ1_0 { qz2_row_tq1_0(x, y, n_per_row); }
        else if t == T_TQ2_0 { qz2_row_tq2_0(x, y, n_per_row); }
        else if t == T_MXFP4 { qz2_row_mxfp4(x, y, n_per_row); }
        else if t == T_NVFP4 { qz2_row_nvfp4(x, y, n_per_row); }
        else if t == T_IQ4_NL { qz2_row_iq4_nl(x, y, n_per_row); }
        else if t == T_IQ4_XS { qz2_row_iq4_xs(x, y, n_per_row); }
        else if t == T_IQ3_XXS { qz2_row_iq3_xxs(x, y, n_per_row); }
        else if t == T_IQ3_S { qz2_row_iq3_s(x, y, n_per_row); }
        else if t == T_IQ2_S { qz2_row_iq2_s(x, y, n_per_row); }
        else if t == T_IQ1_M { qz2_row_iq1_m(x, y, n_per_row); }
        r += 1 as i64;
    }
    return nrows * rs;
}

// quantize_row_XXX_ref for every type of this slice. q8_K is included: the
// native row entry exists even though the chunk switch does not list it.
export fn qz2_quantize_row(t: i64, x: u64, y: u64, k: i64) -> i64 {
    let blck = qz2_blck(t);
    if blck < 0 { return -1; }
    if k <= 0 || k % blck != 0 { return -2; }
    return qz2_rows(t, x, y, k / blck, blck);
}

// ggml_quantize_chunk(type, src, dst, start, nrows, n_per_row, nullptr).
// -1: type outside this slice. -2: geometry the native asserts on. -3: q8_K,
// which the native chunk switch does not handle at all (its default branch
// asserts).
export fn qz2_quantize_chunk(t: i64, src: u64, dst: u64, start: i64, nrows: i64, n_per_row: i64) -> i64 {
    let blck = qz2_blck(t);
    if blck < 0 { return -1; }
    if t == T_Q8_K { return -3; }
    if nrows <= 0 || n_per_row <= 0 || start < 0 { return -2; }
    if n_per_row % blck != 0 { return -2; }
    if start % blck != 0 { return -2; }
    if start % n_per_row != 0 { return -2; }
    let row_size = qz2_row_size(t, n_per_row);
    if row_size < 0 { return -2; }
    let start_row = start / n_per_row;
    let s = src + start as u64 * 4;
    let d = dst + (start_row * row_size) as u64;
    return qz2_rows(t, s, d, nrows, n_per_row);
}

fn main() {
    let t = qz2_quantize_chunk(0 as i64, 0 as u64, 0 as u64, 0 as i64, 0 as i64, 0 as i64);
    let r = t;
}

// ---- IQ grid search helpers ----------------------------------------------

const S_ONGRID: i64 = 1088; // 8 bool bytes
const S_ONAUX: i64 = 1096;  // 8 bool bytes
const S_PX: i64 = 1104;     // 2 x 17 f32 prefix sums
const S_PW: i64 = 1240;     // 2 x 17 f32 prefix weights
const S_SHIFTS: i64 = 1376; // 16 int8 shifts
const S_XP: i64 = 1392;     // 3 f32 x_p
const S_XM: i64 = 1404;     // 3 f32 x_m

// iq3_find_best_neighbour. off is the neighbour-list offset, i.e.
// -map[u] - 1 in the native pointer arithmetic.
fn iq3_best_nb(g: i64, off: i64, xv: u64, wv: u64, scale: f32, L: u64) -> i64 {
    let n = iq3_nb(g, off);
    if n <= 0 { fail(1201); }
    var best_d2 = 3.4028234663852886e38 as f32;
    var gi = 0 as i64 - 1;
    var j = 1 as i64;
    while j <= n {
        let idx = iq3_nb(g, off + j);
        let gv = iq3_qgrid(g, idx);
        var d2 = 0 as f32;
        var i = 0 as i64;
        while i < 4 {
            let q = i8val((gv >> (8 * i)) & 255);
            let diff = scale * (q as f32) - ld_f32(xv + (i as u64) * 4);
            d2 += ld_f32(wv + (i as u64) * 4) * diff * diff;
            i += 1 as i64;
        }
        if d2 < best_d2 {
            best_d2 = d2;
            gi = idx;
        }
        j += 1 as i64;
    }
    if gi < 0 { fail(1202); }
    let gv = iq3_qgrid(g, gi);
    var i = 0 as i64;
    while i < 4 {
        let q = i8val((gv >> (8 * i)) & 255);
        st_u8(L + (i as u64), (q - 1) / 2);
        i += 1 as i64;
    }
    return gi;
}

fn iq2_best_nb(g: i64, off: i64, xv: u64, wv: u64, scale: f32, L: u64) -> i64 {
    let n = iq2_nb(g, off);
    if n <= 0 { fail(1211); }
    var best_d2 = 3.4028234663852886e38 as f32;
    var gi = 0 as i64 - 1;
    var j = 1 as i64;
    while j <= n {
        let idx = iq2_nb(g, off + j);
        let gv = iq2_qgrid(g, idx);
        var d2 = 0 as f32;
        var i = 0 as i64;
        while i < 8 {
            let q = i8val((gv >> (8 * i)) & 255);
            let diff = scale * (q as f32) - ld_f32(xv + (i as u64) * 4);
            d2 += ld_f32(wv + (i as u64) * 4) * diff * diff;
            i += 1 as i64;
        }
        if d2 < best_d2 {
            best_d2 = d2;
            gi = idx;
        }
        j += 1 as i64;
    }
    if gi < 0 { fail(1212); }
    let gv = iq2_qgrid(g, gi);
    var i = 0 as i64;
    while i < 8 {
        let q = i8val((gv >> (8 * i)) & 255);
        st_u8(L + (i as u64), (q - 1) / 2);
        i += 1 as i64;
    }
    return gi;
}

// ---- IQ3_XXS --------------------------------------------------------------
// grid 256 (iq3_data index 0); destination is d + 96 bytes.

fn qz2_row_iq3_xxs(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    let L = tmp(S_L);
    let laux = tmp(S_LAUX);
    let wa = tmp(S_WEIGHTS);
    let xv = tmp(S_XVAL);
    let waux = tmp(S_WAUX);
    let sgn = tmp(S_SIGNS);
    let og = tmp(S_ONGRID);
    let oa = tmp(S_ONAUX);
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_IQ3_XXS) as u64;
        st_f16(yb, 0 as f32);
        var z0 = 0 as i64;
        while z0 < 96 {
            st_u8(yb + 2 + (z0 as u64), 0);
            z0 += 1 as i64;
        }
        var max_scale = 0 as f32;
        var sumx2 = 0 as f32;
        var ii = 0 as i64;
        while ii < QK_K {
            let xval = ld_f32(xb + (ii as u64) * 4);
            sumx2 += xval * xval;
            ii += 1 as i64;
        }
        let sigma2 = 2.0 * sumx2 / (QK_K as f32);
        var ib = 0 as i64;
        while ib < QK_K / 32 {
            let xg = xb + (ib * 32) as u64 * 4;
            var i2 = 0 as i64;
            var i4 = 0 as i64;
            while i2 < 32 {
                let xval = ld_f32(xg + (i2 as u64) * 4);
                let w = xval * xval;
                st_f32(wa + (i2 as u64) * 4, w);
                st_f32(waux + (i2 as u64) * 4, sqrt32(w));
                i2 += 1 as i64;
            }
            var kk = 0 as i64;
            while kk < 4 {
                var nflip = 0 as i64;
                var s = 0 as i64;
                var i3 = 0 as i64;
                while i3 < 8 {
                    let xval = ld_f32(xg + ((8 * kk + i3) as u64) * 4);
                    if xval >= 0 as f32 {
                        st_f32(xv + ((8 * kk + i3) as u64) * 4, xval);
                    } else {
                        st_f32(xv + ((8 * kk + i3) as u64) * 4, -xval);
                        nflip += 1 as i64;
                        s = s | (1 << i3);
                    }
                    i3 += 1 as i64;
                }
                if nflip % 2 == 1 {
                    var imin = 0 as i64;
                    let x0 = ld_f32(xg + ((8 * kk) as u64) * 4);
                    var mn = ld_f32(wa + ((8 * kk) as u64) * 4) * x0 * x0;
                    i3 = 1 as i64;
                    while i3 < 8 {
                        let xval = ld_f32(xg + ((8 * kk + i3) as u64) * 4);
                        let ax = ld_f32(wa + ((8 * kk + i3) as u64) * 4) * xval * xval;
                        if ax < mn {
                            mn = ax;
                            imin = i3;
                        }
                        i3 += 1 as i64;
                    }
                    let at = xv + ((8 * kk + imin) as u64) * 4;
                    st_f32(at, -ld_f32(at));
                    s = s ^ (1 << imin);
                }
                st_u8(sgn + (kk as u64), s & 127);
                kk += 1 as i64;
            }
            var mx = ld_f32(xv);
            i2 = 1 as i64;
            while i2 < 32 {
                mx = fmax32(mx, ld_f32(xv + (i2 as u64) * 4));
                i2 += 1 as i64;
            }
            i2 = 0 as i64;
            while i2 < 32 {
                st_u8(L + (i2 as u64), 0);
                i2 += 1 as i64;
            }
            if mx < 1.0e-8 {
                ssetf(S_SCALES + ib * 4, 0 as f32);
                ib += 1 as i64;
                continue;
            }
            var best = 0 as f32;
            var scale = mx / 15.0;
            var kk2 = 0 as i64;
            while kk2 < 8 {
                st_u8(og + (kk2 as u64), 1);
                kk2 += 1 as i64;
            }
            var is = 0 as i64 - 15;
            while is <= 15 {
                let id = (15.0 + (is as f32) * 0.2) / mx;
                let this_scale = (1 as f32) / id;
                var kk3 = 0 as i64;
                while kk3 < 8 {
                    i4 = 0 as i64;
                    while i4 < 4 {
                        let l0 = nearest(0.5 * (id * ld_f32(xv + ((4 * kk3 + i4) as u64) * 4) - 1.0));
                        st_u8(laux + ((4 * kk3 + i4) as u64), imax(0 as i64, imin(7, l0)));
                        i4 += 1 as i64;
                    }
                    var u = 0 as i64;
                    i4 = 0 as i64;
                    while i4 < 4 {
                        u = u | (ld_u8(laux + ((4 * kk3 + i4) as u64)) << (3 * i4));
                        i4 += 1 as i64;
                    }
                    let gi = iq3_map(0, u);
                    st_u8(oa + (kk3 as u64), 1);
                    if gi < 0 {
                        st_u8(oa + (kk3 as u64), 0);
                        iq3_best_nb(0, 0 as i64 - gi - 1, xv + ((4 * kk3) as u64) * 4,
                                    waux + ((4 * kk3) as u64) * 4, this_scale, laux + ((4 * kk3) as u64));
                    }
                    kk3 += 1 as i64;
                }
                var sumqx = 0 as f32;
                var sumq2 = 0 as f32;
                i4 = 0 as i64;
                while i4 < 32 {
                    let w = ld_f32(wa + (i4 as u64) * 4);
                    let q = 2.0 * (ld_u8(laux + (i4 as u64)) as f32) + 1.0;
                    sumqx += w * ld_f32(xv + (i4 as u64) * 4) * q;
                    sumq2 += w * q * q;
                    i4 += 1 as i64;
                }
                if sumq2 > 0 as f32 && sumqx * sumqx > best * sumq2 {
                    scale = sumqx / sumq2;
                    best = scale * sumqx;
                    i4 = 0 as i64;
                    while i4 < 32 {
                        st_u8(L + (i4 as u64), ld_u8(laux + (i4 as u64)));
                        i4 += 1 as i64;
                    }
                    kk3 = 0 as i64;
                    while kk3 < 8 {
                        st_u8(og + (kk3 as u64), ld_u8(oa + (kk3 as u64)));
                        kk3 += 1 as i64;
                    }
                }
                is += 1 as i64;
            }
            var n_not = 0 as i64;
            kk2 = 0 as i64;
            while kk2 < 8 {
                if ld_u8(og + (kk2 as u64)) == 0 { n_not += 1 as i64; }
                kk2 += 1 as i64;
            }
            if n_not > 0 && scale > 0 as f32 {
                let id = (1 as f32) / scale;
                kk2 = 0 as i64;
                while kk2 < 8 {
                    if ld_u8(og + (kk2 as u64)) == 0 {
                        var u = 0 as i64;
                        i4 = 0 as i64;
                        while i4 < 4 {
                            let l0 = nearest(0.5 * (id * ld_f32(xv + ((4 * kk2 + i4) as u64) * 4) - 1.0));
                            let l = imax(0 as i64, imin(7, l0));
                            u = u | (l << (3 * i4));
                            i4 += 1 as i64;
                        }
                        let gi = iq3_map(0, u);
                        var gidx = gi;
                        if gi < 0 {
                            gidx = iq3_best_nb(0, 0 as i64 - gi - 1, xv + ((4 * kk2) as u64) * 4,
                                               waux + ((4 * kk2) as u64) * 4, scale, L + ((4 * kk2) as u64));
                        }
                        let gv = iq3_qgrid(0, gidx);
                        i4 = 0 as i64;
                        while i4 < 4 {
                            st_u8(L + ((4 * kk2 + i4) as u64), (i8val((gv >> (8 * i4)) & 255) - 1) / 2);
                            i4 += 1 as i64;
                        }
                    }
                    kk2 += 1 as i64;
                }
                var sumqx = 0 as f32;
                var sumq2 = 0 as f32;
                i4 = 0 as i64;
                while i4 < 32 {
                    let w = ld_f32(wa + (i4 as u64) * 4);
                    let q = 2.0 * (ld_u8(L + (i4 as u64)) as f32) + 1.0;
                    sumqx += w * ld_f32(xv + (i4 as u64) * 4) * q;
                    sumq2 += w * q * q;
                    i4 += 1 as i64;
                }
                if sumq2 > 0 as f32 { scale = sumqx / sumq2; }
            }
            if scale < 0 as f32 {
                scale = -scale;
                kk2 = 0 as i64;
                while kk2 < 4 {
                    st_u8(sgn + (kk2 as u64), (ld_u8(sgn + (kk2 as u64)) ^ 127) & 127);
                    kk2 += 1 as i64;
                }
            }
            kk2 = 0 as i64;
            while kk2 < 8 {
                var u = 0 as i64;
                var i4 = 0 as i64;
                while i4 < 4 {
                    u = u | (ld_u8(L + ((4 * kk2 + i4) as u64)) << (3 * i4));
                    i4 += 1 as i64;
                }
                let gi = iq3_map(0, u);
                if gi < 0 { fail(1221); }
                st_u8(yb + 2 + ((8 * ib + kk2) as u64), gi);
                kk2 += 1 as i64;
            }
            let signs = ld_u8(sgn) | (ld_u8(sgn + 1) << 7) | (ld_u8(sgn + 2) << 14) | (ld_u8(sgn + 3) << 21);
            st_u32(yb + 2 + ((64 + 4 * ib) as u64), signs);
            ssetf(S_SCALES + ib * 4, scale);
            max_scale = fmax32(max_scale, scale);
            ib += 1 as i64;
        }
        if max_scale == 0 as f32 {
            var z = 0 as i64;
            while z < 96 {
                st_u8(yb + 2 + (z as u64), 0);
                z += 1 as i64;
            }
            i += 1 as i64;
            continue;
        }
        let d = max_scale / 31.0;
        st_f16(yb, d * 1.0125);
        let id = (1 as f32) / d;
        ib = 0 as i64;
        while ib < QK_K / 32 {
            var l = nearest(0.5 * (id * sgetf(S_SCALES + ib * 4) - 1.0));
            l = imax(0 as i64, imin(15, l));
            let at = yb + 2 + ((64 + 4 * ib) as u64);
            var signs = 0 as i64;
            var b = 0 as i64;
            while b < 4 {
                signs = signs | (ld_u8(at + (b as u64)) << (8 * b));
                b += 1 as i64;
            }
            signs = signs | (l << 28);
            st_u32(at, signs);
            ib += 1 as i64;
        }
        i += 1 as i64;
    }
}

// ---- IQ3_S ----------------------------------------------------------------
// grid 512 (iq3_data index 1); d + qs[64] + qh[8] + signs[32] + scales[8].

fn qz2_row_iq3_s(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    let L = tmp(S_L);
    let laux = tmp(S_LAUX);
    let wa = tmp(S_WEIGHTS);
    let xv = tmp(S_XVAL);
    let waux = tmp(S_WAUX);
    let sgn = tmp(S_SIGNS);
    let og = tmp(S_ONGRID);
    let oa = tmp(S_ONAUX);
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_IQ3_S) as u64;
        var z = 0 as i64;
        while z < B_IQ3_S {
            st_u8(yb + (z as u64), 0);
            z += 1 as i64;
        }
        st_f16(yb, 0 as f32);
        var max_scale = 0 as f32;
        var sumx2 = 0 as f32;
        var ii = 0 as i64;
        while ii < QK_K {
            let xval = ld_f32(xb + (ii as u64) * 4);
            sumx2 += xval * xval;
            ii += 1 as i64;
        }
        let sigma2 = 2.0 * sumx2 / (QK_K as f32);
        var ib = 0 as i64;
        while ib < QK_K / 32 {
            let xg = xb + (ib * 32) as u64 * 4;
            var i2 = 0 as i64;
            var i4 = 0 as i64;
            while i2 < 32 {
                let xval = ld_f32(xg + (i2 as u64) * 4);
                let w = xval * xval;
                st_f32(wa + (i2 as u64) * 4, w);
                st_f32(waux + (i2 as u64) * 4, sqrt32(w));
                i2 += 1 as i64;
            }
            var kk = 0 as i64;
            while kk < 4 {
                var s = 0 as i64;
                var i3 = 0 as i64;
                while i3 < 8 {
                    let xval = ld_f32(xg + ((8 * kk + i3) as u64) * 4);
                    if xval >= 0 as f32 {
                        st_f32(xv + ((8 * kk + i3) as u64) * 4, xval);
                    } else {
                        st_f32(xv + ((8 * kk + i3) as u64) * 4, -xval);
                        s = s | (1 << i3);
                    }
                    i3 += 1 as i64;
                }
                st_u8(sgn + (kk as u64), s & 255);
                kk += 1 as i64;
            }
            var mx = ld_f32(xv);
            i2 = 1 as i64;
            while i2 < 32 {
                mx = fmax32(mx, ld_f32(xv + (i2 as u64) * 4));
                i2 += 1 as i64;
            }
            i2 = 0 as i64;
            while i2 < 32 {
                st_u8(L + (i2 as u64), 0);
                i2 += 1 as i64;
            }
            if mx == 0 as f32 {
                ssetf(S_SCALES + ib * 4, 0 as f32);
                ib += 1 as i64;
                continue;
            }
            var best = 0 as f32;
            var scale = mx / 15.0;
            var kk2 = 0 as i64;
            while kk2 < 8 {
                st_u8(og + (kk2 as u64), 0);
                kk2 += 1 as i64;
            }
            var is = 0 as i64 - 9;
            while is <= 9 {
                let id = (15.0 + (is as f32) * 0.2) / mx;
                let this_scale = (1 as f32) / id;
                var kk3 = 0 as i64;
                while kk3 < 8 {
                    i4 = 0 as i64;
                    while i4 < 4 {
                        let l0 = nearest(0.5 * (id * ld_f32(xv + ((4 * kk3 + i4) as u64) * 4) - 1.0));
                        st_u8(laux + ((4 * kk3 + i4) as u64), imax(0 as i64, imin(7, l0)));
                        i4 += 1 as i64;
                    }
                    var u = 0 as i64;
                    i4 = 0 as i64;
                    while i4 < 4 {
                        u = u | (ld_u8(laux + ((4 * kk3 + i4) as u64)) << (3 * i4));
                        i4 += 1 as i64;
                    }
                    let gi = iq3_map(1, u);
                    st_u8(oa + (kk3 as u64), 1);
                    if gi < 0 {
                        st_u8(oa + (kk3 as u64), 0);
                        iq3_best_nb(1, 0 as i64 - gi - 1, xv + ((4 * kk3) as u64) * 4,
                                    waux + ((4 * kk3) as u64) * 4, this_scale, laux + ((4 * kk3) as u64));
                    }
                    kk3 += 1 as i64;
                }
                var sumqx = 0 as f32;
                var sumq2 = 0 as f32;
                i4 = 0 as i64;
                while i4 < 32 {
                    let w = ld_f32(wa + (i4 as u64) * 4);
                    let q = 2.0 * (ld_u8(laux + (i4 as u64)) as f32) + 1.0;
                    sumqx += w * ld_f32(xv + (i4 as u64) * 4) * q;
                    sumq2 += w * q * q;
                    i4 += 1 as i64;
                }
                if sumq2 > 0 as f32 && sumqx * sumqx > best * sumq2 {
                    scale = sumqx / sumq2;
                    best = scale * sumqx;
                    i4 = 0 as i64;
                    while i4 < 32 {
                        st_u8(L + (i4 as u64), ld_u8(laux + (i4 as u64)));
                        i4 += 1 as i64;
                    }
                    kk3 = 0 as i64;
                    while kk3 < 8 {
                        st_u8(og + (kk3 as u64), ld_u8(oa + (kk3 as u64)));
                        kk3 += 1 as i64;
                    }
                }
                is += 1 as i64;
            }
            var n_not = 0 as i64;
            kk2 = 0 as i64;
            while kk2 < 8 {
                if ld_u8(og + (kk2 as u64)) == 0 { n_not += 1 as i64; }
                kk2 += 1 as i64;
            }
            if n_not > 0 && scale > 0 as f32 {
                let id = (1 as f32) / scale;
                kk2 = 0 as i64;
                while kk2 < 8 {
                    var u = 0 as i64;
                    i4 = 0 as i64;
                    while i4 < 4 {
                        let l0 = nearest(0.5 * (id * ld_f32(xv + ((4 * kk2 + i4) as u64) * 4) - 1.0));
                        u = u | (imax(0 as i64, imin(7, l0)) << (3 * i4));
                        i4 += 1 as i64;
                    }
                    let gi = iq3_map(1, u);
                    var gidx = gi;
                    if gi < 0 {
                        gidx = iq3_best_nb(1, 0 as i64 - gi - 1, xv + ((4 * kk2) as u64) * 4,
                                           waux + ((4 * kk2) as u64) * 4, scale, L + ((4 * kk2) as u64));
                    }
                    let gv = iq3_qgrid(1, gidx);
                    i4 = 0 as i64;
                    while i4 < 4 {
                        st_u8(L + ((4 * kk2 + i4) as u64), (i8val((gv >> (8 * i4)) & 255) - 1) / 2);
                        i4 += 1 as i64;
                    }
                    kk2 += 1 as i64;
                }
                var sumqx = 0 as f32;
                var sumq2 = 0 as f32;
                i4 = 0 as i64;
                while i4 < 32 {
                    let w = ld_f32(wa + (i4 as u64) * 4);
                    let q = 2.0 * (ld_u8(L + (i4 as u64)) as f32) + 1.0;
                    sumqx += w * ld_f32(xv + (i4 as u64) * 4) * q;
                    sumq2 += w * q * q;
                    i4 += 1 as i64;
                }
                if sumq2 > 0 as f32 { scale = sumqx / sumq2; }
            }
            if scale < 0 as f32 {
                scale = -scale;
                kk2 = 0 as i64;
                while kk2 < 4 {
                    st_u8(sgn + (kk2 as u64), ld_u8(sgn + (kk2 as u64)) ^ 255);
                    kk2 += 1 as i64;
                }
            }
            kk2 = 0 as i64;
            while kk2 < 8 {
                var u = 0 as i64;
                var i4 = 0 as i64;
                while i4 < 4 {
                    u = u | (ld_u8(L + ((4 * kk2 + i4) as u64)) << (3 * i4));
                    i4 += 1 as i64;
                }
                let gi = iq3_map(1, u);
                if gi < 0 { fail(1231); }
                st_u8(yb + 2 + ((8 * ib + kk2) as u64), gi & 255);
                let at = yb + 66 + (ib as u64);
                st_u8(at, ld_u8(at) | ((gi >> 8) << kk2));
                kk2 += 1 as i64;
            }
            kk2 = 0 as i64;
            while kk2 < 4 {
                st_u8(yb + 74 + ((4 * ib + kk2) as u64), ld_u8(sgn + (kk2 as u64)));
                kk2 += 1 as i64;
            }
            ssetf(S_SCALES + ib * 4, scale);
            max_scale = fmax32(max_scale, scale);
            ib += 1 as i64;
        }
        if max_scale == 0 as f32 {
            i += 1 as i64;
            continue;
        }
        let d = max_scale / 31.0;
        st_f16(yb, d * 1.033);
        let id = (1 as f32) / d;
        ib = 0 as i64;
        while ib < QK_K / 32 {
            var l1 = nearest(0.5 * (id * sgetf(S_SCALES + ib * 4) - 1.0));
            l1 = imax(0 as i64, imin(15, l1));
            var l2 = nearest(0.5 * (id * sgetf(S_SCALES + (ib + 1) * 4) - 1.0));
            l2 = imax(0 as i64, imin(15, l2));
            st_u8(yb + 106 + ((ib / 2) as u64), l1 | (l2 << 4));
            ib += 2 as i64;
        }
        i += 1 as i64;
    }
}

// ---- IQ2_S ----------------------------------------------------------------
// iq2_data index 3 (grid 1024); d + qs[64] + qh[8] + scales[8].

fn qz2_row_iq2_s(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    let L = tmp(S_L);
    let laux = tmp(S_LAUX);
    let wa = tmp(S_WEIGHTS);
    let xv = tmp(S_XVAL);
    let waux = tmp(S_WAUX);
    let sgn = tmp(S_SIGNS);
    let og = tmp(S_ONGRID);
    let oa = tmp(S_ONAUX);
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_IQ2_S) as u64;
        var z = 0 as i64;
        while z < B_IQ2_S {
            st_u8(yb + (z as u64), 0);
            z += 1 as i64;
        }
        st_f16(yb, 0 as f32);
        var max_scale = 0 as f32;
        var sumx2 = 0 as f32;
        var ii = 0 as i64;
        while ii < QK_K {
            let xval = ld_f32(xb + (ii as u64) * 4);
            sumx2 += xval * xval;
            ii += 1 as i64;
        }
        let sigma2 = 2.0 * sumx2 / (QK_K as f32);
        var ib = 0 as i64;
        while ib < QK_K / 16 {
            let xg = xb + (ib * 16) as u64 * 4;
            var i2 = 0 as i64;
            var i4 = 0 as i64;
            while i2 < 16 {
                let xval = ld_f32(xg + (i2 as u64) * 4);
                let w = 0.25 * sigma2 + xval * xval;
                st_f32(wa + (i2 as u64) * 4, w);
                st_f32(waux + (i2 as u64) * 4, sqrt32(w));
                i2 += 1 as i64;
            }
            var kk = 0 as i64;
            while kk < 2 {
                var s = 0 as i64;
                var i3 = 0 as i64;
                while i3 < 8 {
                    let xval = ld_f32(xg + ((8 * kk + i3) as u64) * 4);
                    if xval >= 0 as f32 {
                        st_f32(xv + ((8 * kk + i3) as u64) * 4, xval);
                    } else {
                        st_f32(xv + ((8 * kk + i3) as u64) * 4, -xval);
                        s = s | (1 << i3);
                    }
                    i3 += 1 as i64;
                }
                st_u8(sgn + (kk as u64), s & 255);
                kk += 1 as i64;
            }
            var mx = ld_f32(xv);
            i2 = 1 as i64;
            while i2 < 16 {
                mx = fmax32(mx, ld_f32(xv + (i2 as u64) * 4));
                i2 += 1 as i64;
            }
            i2 = 0 as i64;
            while i2 < 16 {
                st_u8(L + (i2 as u64), 0);
                i2 += 1 as i64;
            }
            if mx < 1.0e-8 {
                ssetf(S_SCALES + ib * 4, 0 as f32);
                ib += 1 as i64;
                continue;
            }
            var best = 0 as f32;
            var scale = mx / 5.0;
            st_u8(og, 1);
            st_u8(og + 1, 1);
            var is = 0 as i64 - 9;
            while is <= 9 {
                let id = (5.0 + (is as f32) * 0.1) / mx;
                let this_scale = (1 as f32) / id;
                var kk3 = 0 as i64;
                while kk3 < 2 {
                    i4 = 0 as i64;
                    while i4 < 8 {
                        let l0 = nearest(0.5 * (id * ld_f32(xv + ((8 * kk3 + i4) as u64) * 4) - 1.0));
                        st_u8(laux + ((8 * kk3 + i4) as u64), imax(0 as i64, imin(2, l0)));
                        i4 += 1 as i64;
                    }
                    var u = 0 as i64;
                    i4 = 0 as i64;
                    while i4 < 8 {
                        u = u | (ld_u8(laux + ((8 * kk3 + i4) as u64)) << (2 * i4));
                        i4 += 1 as i64;
                    }
                    let gi = iq2_map(3, u);
                    st_u8(oa + (kk3 as u64), 1);
                    if gi < 0 {
                        st_u8(oa + (kk3 as u64), 0);
                        iq2_best_nb(3, 0 as i64 - gi - 1, xv + ((8 * kk3) as u64) * 4,
                                    waux + ((8 * kk3) as u64) * 4, this_scale, laux + ((8 * kk3) as u64));
                    }
                    kk3 += 1 as i64;
                }
                var sumqx = 0 as f32;
                var sumq2 = 0 as f32;
                i4 = 0 as i64;
                while i4 < 16 {
                    let w = ld_f32(wa + (i4 as u64) * 4);
                    let q = 2.0 * (ld_u8(laux + (i4 as u64)) as f32) + 1.0;
                    sumqx += w * ld_f32(xv + (i4 as u64) * 4) * q;
                    sumq2 += w * q * q;
                    i4 += 1 as i64;
                }
                if sumq2 > 0 as f32 && sumqx * sumqx > best * sumq2 {
                    scale = sumqx / sumq2;
                    best = scale * sumqx;
                    i4 = 0 as i64;
                    while i4 < 16 {
                        st_u8(L + (i4 as u64), ld_u8(laux + (i4 as u64)));
                        i4 += 1 as i64;
                    }
                    st_u8(og, ld_u8(oa));
                    st_u8(og + 1, ld_u8(oa + 1));
                }
                is += 1 as i64;
            }
            var n_not = 0 as i64;
            if ld_u8(og) == 0 { n_not += 1 as i64; }
            if ld_u8(og + 1) == 0 { n_not += 1 as i64; }
            if n_not > 0 && scale > 0 as f32 {
                let id = (1 as f32) / scale;
                var kk2 = 0 as i64;
                while kk2 < 2 {
                    if ld_u8(og + (kk2 as u64)) == 0 {
                        var u = 0 as i64;
                        i4 = 0 as i64;
                        while i4 < 8 {
                            let l0 = nearest(0.5 * (id * ld_f32(xv + ((8 * kk2 + i4) as u64) * 4) - 1.0));
                            let l = imax(0 as i64, imin(2, l0));
                            u = u | (l << (2 * i4));
                            st_u8(L + ((8 * kk2 + i4) as u64), l);
                            i4 += 1 as i64;
                        }
                        let gi = iq2_map(3, u);
                        if gi < 0 {
                            iq2_best_nb(3, 0 as i64 - gi - 1, xv + ((8 * kk2) as u64) * 4,
                                        waux + ((8 * kk2) as u64) * 4, scale, L + ((8 * kk2) as u64));
                        }
                    }
                    kk2 += 1 as i64;
                }
                var sumqx = 0 as f32;
                var sumq2 = 0 as f32;
                i4 = 0 as i64;
                while i4 < 16 {
                    let w = ld_f32(wa + (i4 as u64) * 4);
                    let q = 2.0 * (ld_u8(L + (i4 as u64)) as f32) + 1.0;
                    sumqx += w * ld_f32(xv + (i4 as u64) * 4) * q;
                    sumq2 += w * q * q;
                    i4 += 1 as i64;
                }
                if sumq2 > 0 as f32 { scale = sumqx / sumq2; }
            }
            if scale < 0 as f32 {
                scale = -scale;
                st_u8(sgn, (ld_u8(sgn) ^ 255) & 255);
                st_u8(sgn + 1, (ld_u8(sgn + 1) ^ 255) & 255);
            }
            var kk4 = 0 as i64;
            while kk4 < 2 {
                var u = 0 as i64;
                i4 = 0 as i64;
                while i4 < 8 {
                    u = u | (ld_u8(L + ((8 * kk4 + i4) as u64)) << (2 * i4));
                    i4 += 1 as i64;
                }
                let gi = iq2_map(3, u);
                if gi < 0 { fail(1241); }
                let i8v = 2 * ib + kk4;
                st_u8(yb + 2 + (i8v as u64), gi & 255);
                let at = yb + 66 + ((i8v / 4) as u64);
                st_u8(at, ld_u8(at) | ((gi >> 8) << (2 * (i8v % 4))));
                st_u8(yb + 34 + (i8v as u64), ld_u8(sgn + (kk4 as u64)));
                kk4 += 1 as i64;
            }
            ssetf(S_SCALES + ib * 4, scale);
            max_scale = fmax32(max_scale, scale);
            ib += 1 as i64;
        }
        if max_scale == 0 as f32 {
            i += 1 as i64;
            continue;
        }
        let d = max_scale / 31.0;
        st_f16(yb, d * 0.9875);
        let id = (1 as f32) / d;
        ib = 0 as i64;
        while ib < QK_K / 16 {
            var l = nearest(0.5 * (id * sgetf(S_SCALES + ib * 4) - 1.0));
            l = imax(0 as i64, imin(15, l));
            if ib % 2 == 0 {
                st_u8(yb + 74 + ((ib / 2) as u64), l);
            } else {
                st_u8(yb + 74 + ((ib / 2) as u64), ld_u8(yb + 74 + ((ib / 2) as u64)) | (l << 4));
            }
            ib += 1 as i64;
        }
        i += 1 as i64;
    }
}

// ---- IQ1_M ----------------------------------------------------------------
// iq2_data index 2 (grid 2048). block_iq1_m is qs[32], qh[16], scales[8]; the
// four uint16 words of scales hold one 3-bit scale per 16-element group plus
// the fp16 bits of the super-scale in their high nibbles.

// iq1_find_best_neighbour2, including its fallback loop.
fn iq1_best_nb2(off: i64, xv: u64, wv: u64, scale: f32, xg: u64, L: u64, ngrid: i64) -> i64 {
    let n = iq2_nb(2, off);
    if n <= 0 { fail(1251); }
    var best_score = 3.4028234663852886e38 as f32;
    var gi = 0 as i64 - 1;
    var j = 1 as i64;
    while j <= n {
        let idx = iq2_nb(2, off + j);
        let gv = iq2_qgrid(2, idx);
        var d2 = 0 as f32;
        var i = 0 as i64;
        while i < 8 {
            let q = ld_f32(xg + ((((i8val((gv >> (8 * i)) & 255)) - 1) / 2) as u64) * 4);
            let diff = scale * q - ld_f32(xv + (i as u64) * 4);
            d2 += ld_f32(wv + (i as u64) * 4) * diff * diff;
            i += 1 as i64;
        }
        if d2 < best_score {
            best_score = d2;
            gi = idx;
        }
        j += 1 as i64;
    }
    if gi < 0 {
        var g = 0 as i64;
        while g < ngrid {
            let gv = iq2_qgrid(2, g);
            var d2 = 0 as f32;
            var jj = 0 as i64;
            while jj < 8 {
                let w = ld_f32(wv + (jj as u64) * 4);
                let q = ld_f32(xg + ((((i8val((gv >> (8 * jj)) & 255)) - 1) / 2) as u64) * 4);
                let diff = scale * q - ld_f32(xv + (g as u64) * 4);
                d2 += w * diff * diff;
                jj += 1 as i64;
            }
            if d2 < best_score {
                best_score = d2;
                gi = g;
            }
            g += 1 as i64;
        }
    }
    if gi < 0 { fail(1252); }
    let gv = iq2_qgrid(2, gi);
    var i = 0 as i64;
    while i < 8 {
        st_u8(L + (i as u64), (i8val((gv >> (8 * i)) & 255) - 1) / 2);
        i += 1 as i64;
    }
    return gi;
}

fn qz2_row_iq1_m(x: u64, y: u64, k: i64) {
    let nb = k / QK_K;
    let L = tmp(S_L);
    let wa = tmp(S_WEIGHTS);
    let xv = tmp(S_XVAL);
    let waux = tmp(S_WAUX);
    let pairs = tmp(S_PAIRS);
    let px = tmp(S_PX);
    let pw = tmp(S_PW);
    let xp = tmp(S_XP);
    let xm = tmp(S_XM);
    st_f32(xp, -0.875);
    st_f32(xp + 4, 0.125);
    st_f32(xp + 8, 1.125);
    st_f32(xm, -1.125);
    st_f32(xm + 4, 0 as f32 - 0.125);
    st_f32(xm + 8, 0.875);
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * QK_K) as u64 * 4;
        let yb = y + (i * B_IQ1_M) as u64;
        var z = 0 as i64;
        while z < B_IQ1_M {
            st_u8(yb + (z as u64), 0);
            z += 1 as i64;
        }
        var max_scale = 0 as f32;
        var sumx2 = 0 as f32;
        var ii = 0 as i64;
        while ii < QK_K {
            let xval = ld_f32(xb + (ii as u64) * 4);
            sumx2 += xval * xval;
            ii += 1 as i64;
        }
        let sigma2 = 2.0 * sumx2 / (QK_K as f32);
        var ib = 0 as i64;
        while ib < QK_K / 16 {
            let xg = xb + (ib * 16) as u64 * 4;
            var i2 = 0 as i64;
            while i2 < 16 {
                let xval = ld_f32(xg + (i2 as u64) * 4);
                st_f32(wa + (i2 as u64) * 4, xval * xval);
                i2 += 1 as i64;
            }
            var mx = fabs32(ld_f32(xg));
            i2 = 1 as i64;
            while i2 < 16 {
                mx = fmax32(mx, fabs32(ld_f32(xg + (i2 as u64) * 4)));
                i2 += 1 as i64;
            }
            if mx < 1.0e-7 {
                ssetf(S_SCALES + ib * 4, 0 as f32);
                st_u8(tmp(S_SHIFTS) + (ib as u64), 0);
                i2 = 0 as i64;
                while i2 < 16 {
                    st_u8(L + (i2 as u64), 1);
                    i2 += 1 as i64;
                }
                ib += 1 as i64;
                continue;
            }
            i2 = 0 as i64;
            while i2 < 16 {
                st_f32(pairs + (i2 as u64) * 8, ld_f32(xg + (i2 as u64) * 4));
                st_u32(pairs + (i2 as u64) * 8 + 4, i2);
                i2 += 1 as i64;
            }
            sort16(pairs, 16);
            var best_score = 0 as f32 - 3.4028234663852886e38;
            var scale = mx;
            var besti1 = 0 as i64 - 1;
            var besti2 = 0 as i64 - 1;
            var bestk = 0 as i64 - 1;
            st_f32(px, 0 as f32);
            st_f32(px + 68, 0 as f32);
            st_f32(pw, 0 as f32);
            st_f32(pw + 68, 0 as f32);
            var j = 0 as i64;
            while j < 16 {
                let ii2 = ld_u8(pairs + (j as u64) * 8 + 4);
                var h = 1 as i64;
                if ii2 < 8 { h = 0 as i64; }
                let wx = ld_f32(wa + (ii2 as u64) * 4) * ld_f32(xg + (ii2 as u64) * 4);
                st_f32(px + ((h * 17 + j + 1) as u64) * 4,
                       ld_f32(px + ((h * 17 + j) as u64) * 4) + wx);
                st_f32(px + (((1 - h) * 17 + j + 1) as u64) * 4,
                       ld_f32(px + (((1 - h) * 17 + j) as u64) * 4));
                st_f32(pw + ((h * 17 + j + 1) as u64) * 4,
                       ld_f32(pw + ((h * 17 + j) as u64) * 4) + ld_f32(wa + (ii2 as u64) * 4));
                st_f32(pw + (((1 - h) * 17 + j + 1) as u64) * 4,
                       ld_f32(pw + (((1 - h) * 17 + j) as u64) * 4));
                j += 1 as i64;
            }
            let txs0 = ld_f32(px + 16 * 4);
            let txs1 = ld_f32(px + (17 + 16) * 4);
            let tws0 = ld_f32(pw + 16 * 4);
            let tws1 = ld_f32(pw + (17 + 16) * 4);
            let p2 = 1.125;
            let m2 = 0.875;
            let cp1 = 0.75;
            let cp2 = -1.25;
            let cm1 = 1.25;
            let cm2 = -0.75;
            var i1 = 0 as i64;
            while i1 <= 16 {
                var i2b = i1;
                while i2b <= 16 {
                    var qx_p0 = 0 as f32;
                    var qx_p1 = 0 as f32;
                    var qx_m0 = 0 as f32;
                    var qx_m1 = 0 as f32;
                    var q2_p0 = 0 as f32;
                    var q2_p1 = 0 as f32;
                    var q2_m0 = 0 as f32;
                    var q2_m1 = 0 as f32;
                    var h = 0 as i64;
                    while h < 2 {
                        let sx = ld_f32(px + ((h * 17 + i1) as u64) * 4) + ld_f32(px + ((h * 17 + i2b) as u64) * 4);
                        var txs = txs0;
                        var tws = tws0;
                        if h == 1 {
                            txs = txs1;
                            tws = tws1;
                        }
                        let qxp = txs * p2 - sx;
                        let qxm = txs * m2 - sx;
                        let q2p = tws * p2 * p2
                                  + ld_f32(pw + ((h * 17 + i1) as u64) * 4) * cp1
                                  + ld_f32(pw + ((h * 17 + i2b) as u64) * 4) * cp2;
                        let q2m = tws * m2 * m2
                                  + ld_f32(pw + ((h * 17 + i1) as u64) * 4) * cm1
                                  + ld_f32(pw + ((h * 17 + i2b) as u64) * 4) * cm2;
                        if h == 0 {
                            qx_p0 = qxp; qx_m0 = qxm; q2_p0 = q2p; q2_m0 = q2m;
                        } else {
                            qx_p1 = qxp; qx_m1 = qxm; q2_p1 = q2p; q2_m1 = q2m;
                        }
                        h += 1 as i64;
                    }
                    var k = 0 as i64;
                    while k < 4 {
                        var sumqx = qx_m0 + qx_p1;
                        var sumq2 = q2_m0 + q2_p1;
                        if k == 0 { sumqx = qx_p0 + qx_p1; sumq2 = q2_p0 + q2_p1; }
                        else if k == 1 { sumqx = qx_p0 + qx_m1; sumq2 = q2_p0 + q2_m1; }
                        else if k == 3 { sumqx = qx_m0 + qx_m1; sumq2 = q2_m0 + q2_m1; }
                        if sumq2 > 0 as f32 && sumqx * sumqx > best_score * sumq2 {
                            scale = sumqx / sumq2;
                            best_score = scale * sumqx;
                            besti1 = i1;
                            besti2 = i2b;
                            bestk = k;
                        }
                        k += 1 as i64;
                    }
                    i2b += 1 as i64;
                }
                i1 += 1 as i64;
            }
            if besti1 < 0 || besti2 < 0 || bestk < 0 {
                ssetf(S_SCALES + ib * 4, 0 as f32);
                st_u8(tmp(S_SHIFTS) + (ib as u64), 0);
                i2 = 0 as i64;
                while i2 < 16 {
                    st_u8(L + (i2 as u64), 1);
                    i2 += 1 as i64;
                }
                ib += 1 as i64;
                continue;
            }
            j = 0 as i64;
            while j < 16 {
                let ii2 = ld_u8(pairs + (j as u64) * 8 + 4);
                if j < besti1 { st_u8(L + (ii2 as u64), 0); }
                else if j < besti2 { st_u8(L + (ii2 as u64), 1); }
                else { st_u8(L + (ii2 as u64), 2); }
                j += 1 as i64;
            }
            if scale < 0 as f32 {
                j = 0 as i64;
                while j < 16 {
                    st_u8(L + (j as u64), 2 - ld_u8(L + (j as u64)));
                    j += 1 as i64;
                }
                scale = -scale;
                if bestk == 0 { bestk = 3; }
                else if bestk == 1 { bestk = 2; }
                else if bestk == 2 { bestk = 1; }
                else { bestk = 0; }
            }
            var all_on_grid = true;
            var kk = 0 as i64;
            while kk < 2 {
                var xx = xm;
                if kk == 0 {
                    if bestk < 2 { xx = xp; }
                } else {
                    if bestk % 2 == 0 { xx = xp; }
                }
                var u = 0 as i64;
                var i4 = 0 as i64;
                while i4 < 8 {
                    u = u | (ld_u8(L + ((8 * kk + i4) as u64)) << (2 * i4));
                    i4 += 1 as i64;
                }
                let gi = iq2_map(2, u);
                var gidx = gi;
                if gi < 0 {
                    all_on_grid = false;
                    gidx = iq1_best_nb2(0 as i64 - gi - 1, xg + ((8 * kk) as u64) * 4, wa + ((8 * kk) as u64) * 4,
                                        scale, xx, L + ((8 * kk) as u64), 2048);
                }
                st_u32(tmp(S_IDX) + (kk as u64) * 4, gidx);
                kk += 1 as i64;
            }
            if !all_on_grid {
                var sumqx_f = 0 as f32;
                var sumq2_f = 0 as f32;
                kk = 0 as i64;
                while kk < 2 {
                    var xx = xm;
                    if kk == 0 {
                        if bestk < 2 { xx = xp; }
                    } else {
                        if bestk % 2 == 0 { xx = xp; }
                    }
                    let gidx = ld_u32v(tmp(S_IDX) + (kk as u64) * 4);
                    let gv = iq2_qgrid(2, gidx);
                    var i4 = 0 as i64;
                    while i4 < 8 {
                        let w = ld_f32(wa + ((8 * kk + i4) as u64) * 4);
                        let q = ld_f32(xx + (((i8val((gv >> (8 * i4)) & 255) - 1) / 2) as u64) * 4);
                        sumqx_f += w * q * ld_f32(xg + ((8 * kk + i4) as u64) * 4);
                        sumq2_f += w * q * q;
                        i4 += 1 as i64;
                    }
                    kk += 1 as i64;
                }
                if sumqx_f > 0 as f32 && sumq2_f > 0 as f32 { scale = sumqx_f / sumq2_f; }
            }
            let g0 = ld_u32v(tmp(S_IDX));
            let g1 = ld_u32v(tmp(S_IDX) + 4);
            st_u8(yb + ((2 * ib) as u64), g0 & 255);
            st_u8(yb + ((2 * ib + 1) as u64), g1 & 255);
            st_u8(yb + 32 + (ib as u64), ((g0 >> 8) & 15) | (((g1 >> 8) & 15) << 4));
            ssetf(S_SCALES + ib * 4, scale);
            st_u8(tmp(S_SHIFTS) + (ib as u64), bestk);
            max_scale = fmax32(max_scale, scale);
            ib += 1 as i64;
        }
        if max_scale == 0 as f32 {
            i += 1 as i64;
            continue;
        }
        var sc0 = 0 as i64;
        var sc1 = 0 as i64;
        var sc2 = 0 as i64;
        var sc3 = 0 as i64;
        let d0 = max_scale / 15.0;
        let id = (1 as f32) / d0;
        var sumqx_f = 0 as f32;
        var sumq2_f = 0 as f32;
        ib = 0 as i64;
        while ib < QK_K / 16 {
            var l = nearest(0.5 * (id * sgetf(S_SCALES + ib * 4) - 1.0));
            l = imax(0 as i64, imin(7, l));
            if ib / 4 == 0 { sc0 = sc0 | (l << (3 * (ib % 4))); }
            else if ib / 4 == 1 { sc1 = sc1 | (l << (3 * (ib % 4))); }
            else if ib / 4 == 2 { sc2 = sc2 | (l << (3 * (ib % 4))); }
            else { sc3 = sc3 | (l << (3 * (ib % 4))); }
            let sh = ld_u8(tmp(S_SHIFTS) + (ib as u64));
            var mask = 0 as i64;
            if sh == 1 { mask = 128; }
            else if sh == 2 { mask = 8; }
            else if sh == 3 { mask = 136; }
            let qat = yb + 32 + (ib as u64);
            st_u8(qat, ld_u8(qat) | mask);
            var i2 = 0 as i64;
            while i2 < 16 {
                let xval = ld_f32(xb + ((ib * 16 + i2) as u64) * 4);
                st_f32(wa + (i2 as u64) * 4, xval * xval);
                i2 += 1 as i64;
            }
            var kk = 0 as i64;
            while kk < 2 {
                var xx = xm;
                if kk == 0 {
                    if sh < 2 { xx = xp; }
                } else {
                    if sh % 2 == 0 { xx = xp; }
                }
                let qsb = ld_u8(yb + ((2 * ib + kk) as u64));
                let qhb = ld_u8(yb + 32 + (ib as u64));
                let idx2 = qsb + ((qhb << (8 - 4 * kk)) & 1792);
                let gv = iq2_qgrid(2, idx2);
                var i4 = 0 as i64;
                while i4 < 8 {
                    let w = ld_f32(wa + ((8 * kk + i4) as u64) * 4);
                    let q = ld_f32(xx + (((i8val((gv >> (8 * i4)) & 255) - 1) / 2) as u64) * 4) * (2.0 * (l as f32) + 1.0);
                    sumqx_f += w * q * ld_f32(xb + ((ib * 16 + 8 * kk + i4) as u64) * 4);
                    sumq2_f += w * q * q;
                    i4 += 1 as i64;
                }
                kk += 1 as i64;
            }
            ib += 1 as i64;
        }
        var dd = d0;
        if sumq2_f > 0 as f32 { dd = sumqx_f / sumq2_f; }
        st_f16(tmp(S_TMP + 64), dd * 1.1125);
        let sb0 = ld_u8(tmp(S_TMP + 64));
        let sb1 = ld_u8(tmp(S_TMP + 65));
        let s16 = sb0 | (sb1 << 8);
        st_u16(yb + 48, sc0 | ((s16 & 15) << 12));
        st_u16(yb + 50, sc1 | ((s16 & 240) << 8));
        st_u16(yb + 52, sc2 | ((s16 & 3840) << 4));
        st_u16(yb + 54, sc3 | (s16 & 61440));
        i += 1 as i64;
    }
}

fn ld_u32v(a: u64) -> i64 {
    var v = 0 as i64;
    var b = 0 as i64;
    while b < 4 {
        v = v | (ld_u8(a + (b as u64)) << (8 * b));
        b += 1 as i64;
    }
    return v;
}
