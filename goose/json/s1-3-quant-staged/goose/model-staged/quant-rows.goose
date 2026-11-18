// P7-04 S3a: the unweighted row quantizers in Goose.
//
// Ported from ggml/src/ggml-quants.c:
//   - quantize_row_q1_0_ref        (QK1_0 = 128, d = sum|x|/128, one sign bit)
//   - quantize_row_q2_0_ref        (QK2_0 =  64, d = amax, 2-bit levels -1..2)
//   - quantize_row_q4_0_ref        (QK4_0 =  32, d = max/-8, nibbles)
//   - quantize_row_q4_1_ref        (QK4_1 =  32, d = (max-min)/15, d+m+nibbles)
//   - quantize_row_q5_0_ref        (QK5_0 =  32, d = max/-16, 5th bit in qh)
//   - quantize_row_q5_1_ref        (QK5_1 =  32, d = (max-min)/31, 5th bit in qh)
//   - quantize_row_q8_0_ref        (QK8_0 =  32, d = amax/127, int8 quants)
// and from ggml/src/ggml.c:
//   - ggml_quantize_chunk with imatrix == nullptr, i.e. only the row path
//     (quantize_q{1,2,4,5,8}_0 with quant_weights == nullptr), including the
//     start/n_per_row geometry checks and the start_row * row_size destination
//     offset.
//
// Byte-identical output is the requirement: every stored field, the f16 scale
// (and min, where the type has one), the nibble order inside qs, the qh bit
// positions and the 2-bit packing order of q2_0 are reproduced exactly. The
// float arithmetic keeps the native expression shape (one rounding per
// operation, the same accumulation order for q1_0 sum|x|, the same divisor),
// because the generated code is built with fp-contract off.
//
// Boundaries, stated instead of hidden:
//   - the fp16 rounding primitive is native GGML_FP32_TO_FP16, reached through
//     qz_st_f16; Goose has no fp32 bit reinterpretation, and the established
//     canon (goose/kv/iq4xs-encode.goose) treats the same store as a primitive.
//     The float value passed to it is entirely the port's;
//   - K, IQ, TQ, MXFP4 and NVFP4 are S3b, the imatrix branches are S4;
//   - the native (int8_t)/(uint8_t) casts of a float are undefined for values
//     outside the target range. The port truncates toward zero and then keeps
//     the low eight bits, which is what gcc emits (cvttss2si + truncation) and
//     what the reference produces for every in-range input. NaN and inf rows
//     are outside the gate corpus for that reason and are called out in the
//     README;
//   - the native asserts (k % qk == 0, start % blck_size == 0,
//     start % n_per_row == 0, n_per_row % blck_size == 0) abort the process.
//     The port returns -1 (unknown type) or -2 (geometry) so a fixture can
//     assert the refusal without killing the host.
//
// Pointer form: buffers are u64 addresses and are read/written through the
// extern primitives below, matching geese/model/read-path.goose and
// goose/kv/iq4xs-encode.goose. Offsets are in bytes.

extern "qz_ld_f32" fn ld_f32(a: u64) -> f32;
extern "qz_st_f16" fn st_f16(a: u64, v: f32);
extern "qz_st_u8" fn st_u8(a: u64, v: i64);
extern "qz_st_u32" fn st_u32(a: u64, v: i64);
extern "qz_abs" fn fabs32(v: f32) -> f32;

// ---- ggml type ids and per-type geometry ----------------------------------

const T_Q4_0: i64 = 2;
const T_Q4_1: i64 = 3;
const T_Q5_0: i64 = 6;
const T_Q5_1: i64 = 7;
const T_Q8_0: i64 = 8;
const T_Q1_0: i64 = 41;
const T_Q2_0: i64 = 42;

const QK4_0: i64 = 32;
const QK4_1: i64 = 32;
const QK5_0: i64 = 32;
const QK5_1: i64 = 32;
const QK8_0: i64 = 32;
const QK1_0: i64 = 128;
const QK2_0: i64 = 64;

const B_Q4_0: i64 = 18; // f16 d + 16 nibble bytes
const B_Q4_1: i64 = 20; // f16 d + f16 m + 16 nibble bytes
const B_Q5_0: i64 = 22; // f16 d + 4 qh + 16 nibble bytes
const B_Q5_1: i64 = 24; // f16 d + f16 m + 4 qh + 16 nibble bytes
const B_Q8_0: i64 = 34; // f16 d + 32 int8
const B_Q1_0: i64 = 18; // f16 d + 16 sign-bit bytes
const B_Q2_0: i64 = 18; // f16 d + 16 two-bit bytes

// ggml_blck_size for the seven types, -1 for everything outside S3a.
export fn qz_blck(t: i64) -> i64 {
    if t == T_Q4_0 { return QK4_0; }
    if t == T_Q4_1 { return QK4_1; }
    if t == T_Q5_0 { return QK5_0; }
    if t == T_Q5_1 { return QK5_1; }
    if t == T_Q8_0 { return QK8_0; }
    if t == T_Q1_0 { return QK1_0; }
    if t == T_Q2_0 { return QK2_0; }
    return -1;
}

// ggml_type_size: bytes per block, -1 for everything outside S3a.
export fn qz_type_size(t: i64) -> i64 {
    if t == T_Q4_0 { return B_Q4_0; }
    if t == T_Q4_1 { return B_Q4_1; }
    if t == T_Q5_0 { return B_Q5_0; }
    if t == T_Q5_1 { return B_Q5_1; }
    if t == T_Q8_0 { return B_Q8_0; }
    if t == T_Q1_0 { return B_Q1_0; }
    if t == T_Q2_0 { return B_Q2_0; }
    return -1;
}

// ggml_row_size: ne % blck_size == 0 is required; native asserts, the port
// returns -2. Unknown types return -1.
export fn qz_row_size(t: i64, ne: i64) -> i64 {
    let blck = qz_blck(t);
    if blck < 0 { return -1; }
    let ts = qz_type_size(t);
    if ne <= 0 || ne % blck != 0 { return -2; }
    return ts * ne / blck;
}

// ---- small helpers --------------------------------------------------------

fn qz_min_i(a: i64, b: i64) -> i64 {
    if a < b { return a; }
    return b;
}

// (int8_t) of a truncated float: gcc takes the low eight bits.
fn qz_i8(v: i64) -> i64 {
    let u = v & 255;
    if u >= 128 { return u - 256; }
    return u;
}

// (uint8_t) of a truncated float.
fn qz_u8(v: i64) -> i64 {
    return v & 255;
}

// The native (int8_t)/(uint8_t) casts of a float go through an f32 -> int32
// conversion. On x86-64 gcc emits cvttss2si, whose "integer indefinite" result
// for NaN, +-inf and out-of-range inputs is 0x80000000, so the low byte of the
// cast is zero. Emulating that keeps the port byte-identical for subnormal
// blocks too, where 1/d overflows to inf (127/min_subnormal is far past
// FLT_MAX). Formally the C conversion is undefined there; the port matches this
// reference build, and README.md says so.
const I32_INDEF: i64 = -2147483648;

fn qz_f2i32(v: f32) -> i64 {
    if v != v { return I32_INDEF; }
    if v >= 2147483648.0 { return I32_INDEF; }
    if v < -2147483648.0 { return I32_INDEF; }
    return v as i64;
}

// roundf then the same conversion. Ties go away from zero, and the fractional
// part is compared instead of adding 0.5, which would double-round just below
// a tie.
fn qz_round_f2i32(v: f32) -> i64 {
    if v != v { return I32_INDEF; }
    if v >= 2147483648.0 { return I32_INDEF; }
    if v <= -2147483904.0 { return I32_INDEF; }
    let t = v as i64;
    var r = t;
    if v >= 0 as f32 {
        if v - (t as f32) >= 0.5 {
            r = t + 1;
        }
    } else {
        if (t as f32) - v >= 0.5 {
            r = t - 1;
        }
    }
    if r > 2147483647 { return I32_INDEF; }
    return r;
}

// q4_0/q5_0 head: amax = max |v| and max = the value that first reached it.
// Both are scanned with a strict <, so the sign that wins is the first one.
fn qz_scan_abs_max(x: u64, n: i64) -> f32, f32 {
    var amax = 0 as f32;
    var mx = 0 as f32;
    var j = 0 as i64;
    while j < n {
        let v = ld_f32(x + (j as u64) * 4);
        let a = fabs32(v);
        if amax < a {
            amax = a;
            mx = v;
        }
        j += 1 as i64;
    }
    return amax, mx;
}

// q4_1/q5_1 head: min and max over the block, initialized like the reference.
fn qz_scan_min_max(x: u64, n: i64) -> f32, f32 {
    var mn = 3.4028234663852886e38 as f32;  // FLT_MAX
    var mx = (0 as f32) - (3.4028234663852886e38 as f32);
    var j = 0 as i64;
    while j < n {
        let v = ld_f32(x + (j as u64) * 4);
        if v < mn {
            mn = v;
        }
        if v > mx {
            mx = v;
        }
        j += 1 as i64;
    }
    return mn, mx;
}

fn qz_inv(d: f32) -> f32 {
    if d != 0 as f32 {
        return (1 as f32) / d;
    }
    return 0 as f32;
}

// ---- q4_0 -----------------------------------------------------------------

fn qz_row_q4_0(x: u64, y: u64, k: i64) {
    let qk = QK4_0;
    let nb = k / qk;
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * qk) as u64 * 4;
        let yb = y + (i * B_Q4_0) as u64;
        let amax, mx = qz_scan_abs_max(xb, qk);
        let d = mx / -8.0;
        let id = qz_inv(d);
        st_f16(yb, d);
        var j = 0 as i64;
        while j < qk / 2 {
            let x0 = ld_f32(xb + (j as u64) * 4) * id;
            let x1 = ld_f32(xb + ((j + qk / 2) as u64) * 4) * id;
            let q0 = qz_min_i(15, qz_i8(qz_f2i32(x0 + 8.5)));
            let q1 = qz_min_i(15, qz_i8(qz_f2i32(x1 + 8.5)));
            st_u8(yb + 2 + (j as u64), qz_u8(q0) | (qz_u8(q1) << 4));
            j += 1 as i64;
        }
        i += 1 as i64;
    }
}

// ---- q4_1 -----------------------------------------------------------------

fn qz_row_q4_1(x: u64, y: u64, k: i64) {
    let qk = QK4_1;
    let nb = k / qk;
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * qk) as u64 * 4;
        let yb = y + (i * B_Q4_1) as u64;
        let mn, mx = qz_scan_min_max(xb, qk);
        let d = (mx - mn) / 15.0;
        let id = qz_inv(d);
        st_f16(yb, d);
        st_f16(yb + 2, mn);
        var j = 0 as i64;
        while j < qk / 2 {
            let x0 = (ld_f32(xb + (j as u64) * 4) - mn) * id;
            let x1 = (ld_f32(xb + ((j + qk / 2) as u64) * 4) - mn) * id;
            let q0 = qz_min_i(15, qz_i8(qz_f2i32(x0 + 0.5)));
            let q1 = qz_min_i(15, qz_i8(qz_f2i32(x1 + 0.5)));
            st_u8(yb + 4 + (j as u64), qz_u8(q0) | (qz_u8(q1) << 4));
            j += 1 as i64;
        }
        i += 1 as i64;
    }
}

// ---- q5_0 -----------------------------------------------------------------

fn qz_row_q5_0(x: u64, y: u64, k: i64) {
    let qk = QK5_0;
    let nb = k / qk;
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * qk) as u64 * 4;
        let yb = y + (i * B_Q5_0) as u64;
        let amax, mx = qz_scan_abs_max(xb, qk);
        let d = mx / -16.0;
        let id = qz_inv(d);
        st_f16(yb, d);
        var qh = 0 as i64;
        var j = 0 as i64;
        while j < qk / 2 {
            let x0 = ld_f32(xb + (j as u64) * 4) * id;
            let x1 = ld_f32(xb + ((j + qk / 2) as u64) * 4) * id;
            let q0 = qz_min_i(31, qz_i8(qz_f2i32(x0 + 16.5)));
            let q1 = qz_min_i(31, qz_i8(qz_f2i32(x1 + 16.5)));
            let u0 = qz_u8(q0);
            let u1 = qz_u8(q1);
            st_u8(yb + 6 + (j as u64), (u0 & 15) | ((u1 & 15) << 4));
            qh = qh | (((u0 & 16) >> 4) << j);
            qh = qh | (((u1 & 16) >> 4) << (j + qk / 2));
            j += 1 as i64;
        }
        st_u32(yb + 2, qh);
        i += 1 as i64;
    }
}

// ---- q5_1 -----------------------------------------------------------------

fn qz_row_q5_1(x: u64, y: u64, k: i64) {
    let qk = QK5_1;
    let nb = k / qk;
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * qk) as u64 * 4;
        let yb = y + (i * B_Q5_1) as u64;
        let mn, mx = qz_scan_min_max(xb, qk);
        let d = (mx - mn) / 31.0;
        let id = qz_inv(d);
        st_f16(yb, d);
        st_f16(yb + 2, mn);
        var qh = 0 as i64;
        var j = 0 as i64;
        while j < qk / 2 {
            let x0 = (ld_f32(xb + (j as u64) * 4) - mn) * id;
            let x1 = (ld_f32(xb + ((j + qk / 2) as u64) * 4) - mn) * id;
            let u0 = qz_u8(qz_f2i32(x0 + 0.5));
            let u1 = qz_u8(qz_f2i32(x1 + 0.5));
            st_u8(yb + 8 + (j as u64), (u0 & 15) | ((u1 & 15) << 4));
            qh = qh | (((u0 & 16) >> 4) << j);
            qh = qh | (((u1 & 16) >> 4) << (j + QK5_0 / 2));
            j += 1 as i64;
        }
        st_u32(yb + 4, qh);
        i += 1 as i64;
    }
}

// ---- q8_0 -----------------------------------------------------------------

fn qz_row_q8_0(x: u64, y: u64, k: i64) {
    let qk = QK8_0;
    let nb = k / qk;
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * qk) as u64 * 4;
        let yb = y + (i * B_Q8_0) as u64;
        var amax = 0 as f32;
        var j = 0 as i64;
        while j < qk {
            let a = fabs32(ld_f32(xb + (j as u64) * 4));
            if a > amax {
                amax = a;
            }
            j += 1 as i64;
        }
        let d = amax / 127.0;
        let id = qz_inv(d);
        st_f16(yb, d);
        j = 0 as i64;
        while j < qk {
            let x0 = ld_f32(xb + (j as u64) * 4) * id;
            st_u8(yb + 2 + (j as u64), qz_u8(qz_round_f2i32(x0)));
            j += 1 as i64;
        }
        i += 1 as i64;
    }
}

// ---- q1_0 -----------------------------------------------------------------

fn qz_row_q1_0(x: u64, y: u64, k: i64) {
    let qk = QK1_0;
    let nb = k / qk;
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * qk) as u64 * 4;
        let yb = y + (i * B_Q1_0) as u64;
        var sum_abs = 0 as f32;
        var j = 0 as i64;
        while j < qk {
            sum_abs += fabs32(ld_f32(xb + (j as u64) * 4));
            j += 1 as i64;
        }
        let d = sum_abs / 128.0;
        st_f16(yb, d);
        j = 0 as i64;
        while j < qk / 8 {
            st_u8(yb + 2 + (j as u64), 0);
            j += 1 as i64;
        }
        j = 0 as i64;
        while j < qk {
            let v = ld_f32(xb + (j as u64) * 4);
            if v >= 0 as f32 {
                let at = yb + 2 + ((j / 8) as u64);
                st_u8(at, qz_u8(ld_u8_at(at)) | (1 << (j % 8)));
            }
            j += 1 as i64;
        }
        i += 1 as i64;
    }
}

// The q1_0 bit loop needs the byte it has already written; qz_ld_u8 is the
// matching read primitive. Kept separate from the fp32 load above.
extern "qz_ld_u8" fn ld_u8_at(a: u64) -> i64;

// ---- q2_0 -----------------------------------------------------------------

fn qz_row_q2_0(x: u64, y: u64, k: i64) {
    let qk = QK2_0;
    let nb = k / qk;
    var i = 0 as i64;
    while i < nb {
        let xb = x + (i * qk) as u64 * 4;
        let yb = y + (i * B_Q2_0) as u64;
        var amax = 0 as f32;
        var j = 0 as i64;
        while j < qk {
            let a = fabs32(ld_f32(xb + (j as u64) * 4));
            if a > amax {
                amax = a;
            }
            j += 1 as i64;
        }
        let d = amax;
        let id = if d > 0 as f32 { (1 as f32) / d } else { 0 as f32 };
        st_f16(yb, d);
        j = 0 as i64;
        while j < qk / 4 {
            st_u8(yb + 2 + (j as u64), 0);
            j += 1 as i64;
        }
        j = 0 as i64;
        while j < qk {
            let w = ld_f32(xb + (j as u64) * 4);
            var q = qz_round_f2i32(w * id) + 1;
            if q < 0 {
                q = 0;
            }
            if q > 3 {
                q = 3;
            }
            let at = yb + 2 + ((j / 4) as u64);
            st_u8(at, qz_u8(ld_u8_at(at) | (q << ((j % 4) * 2))));
            j += 1 as i64;
        }
        i += 1 as i64;
    }
}

// ---- row and chunk entry points -------------------------------------------

// One type's rows. k is a multiple of the type's block size and the output is
// packed row after row, which is what the native row quantizers do when
// n_per_row is a block multiple (row_size == nb * type_size).
fn qz_rows(t: i64, src: u64, dst: u64, nrows: i64, n_per_row: i64) -> i64 {
    let rs = qz_row_size(t, n_per_row);
    var r = 0 as i64;
    while r < nrows {
        let x = src + (r * n_per_row) as u64 * 4;
        let y = dst + (r * rs) as u64;
        if t == T_Q4_0 { qz_row_q4_0(x, y, n_per_row); }
        else if t == T_Q4_1 { qz_row_q4_1(x, y, n_per_row); }
        else if t == T_Q5_0 { qz_row_q5_0(x, y, n_per_row); }
        else if t == T_Q5_1 { qz_row_q5_1(x, y, n_per_row); }
        else if t == T_Q8_0 { qz_row_q8_0(x, y, n_per_row); }
        else if t == T_Q1_0 { qz_row_q1_0(x, y, n_per_row); }
        else if t == T_Q2_0 { qz_row_q2_0(x, y, n_per_row); }
        r += 1 as i64;
    }
    return nrows * rs;
}

// quantize_qX_0(type, src, dst, nrow, n_per_row, nullptr) for the seven types.
export fn qz_quantize_row(t: i64, x: u64, y: u64, k: i64) -> i64 {
    let blck = qz_blck(t);
    if blck < 0 { return -1; }
    if k <= 0 || k % blck != 0 { return -2; }
    return qz_rows(t, x, y, k / blck, blck);
}

// ggml_quantize_chunk(type, src, dst, start, nrows, n_per_row, nullptr).
// The geometry checks the native version asserts on are refusals here.
export fn qz_quantize_chunk(t: i64, src: u64, dst: u64, start: i64, nrows: i64, n_per_row: i64) -> i64 {
    let blck = qz_blck(t);
    if blck < 0 { return -1; }
    if nrows <= 0 || n_per_row <= 0 || start < 0 { return -2; }
    if n_per_row % blck != 0 { return -2; }
    if start % blck != 0 { return -2; }
    if start % n_per_row != 0 { return -2; }
    let row_size = qz_row_size(t, n_per_row);
    if row_size < 0 { return -2; }
    let start_row = start / n_per_row;
    let s = src + start as u64 * 4;
    let d = dst + (start_row * row_size) as u64;
    return qz_rows(t, s, d, nrows, n_per_row);
}

fn main() {
    let t = qz_quantize_chunk(0 as i64, 0 as u64, 0 as u64, 0 as i64, 0 as i64, 0 as i64);
    let r = t;
}
