// Metadata values as text, in Goose.
//
// The loader logs every key/value pair during load and that formatting lived in
// C++ (gguf_kv_to_str + gguf_data_to_str). The decisions are simple but they are
// decisions: which type a wire code means, how a bool prints, how a float prints
// (std::to_string gives six decimals), how an array is joined and how strings are
// quoted. They live here now.
//
// `kv` is the reader's key table (stride 8): name_off, name_len, wire_type,
// element_type, payload_off, count, bytes, hash. Scalars keep their payload at
// payload_off; strings are length-prefixed in the header; arrays keep their
// element payload at payload_off and their element type in element_type.

const GF_U8:   i64 = 0;
const GF_I8:   i64 = 1;
const GF_U16:  i64 = 2;
const GF_I16:  i64 = 3;
const GF_U32:  i64 = 4;
const GF_I32:  i64 = 5;
const GF_F32:  i64 = 6;
const GF_BOOL: i64 = 7;
const GF_STR:  i64 = 8;
const GF_ARR:  i64 = 9;
const GF_U64:  i64 = 10;
const GF_I64:  i64 = 11;
const GF_F64:  i64 = 12;

fn gf_fits(out: const u8[:], at: i64, n: i64) -> bool {
    return at >= 0 && at + n <= out.len;
}

fn gf_put(out: u8[:], at: i64, c: u8) -> i64 {
    if at >= out.len {
        return -1;
    }
    out[at] = c;
    return at + 1;
}

fn gf_put_lit(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    if at < 0 || at + s.len > out.len {
        return -1;
    }
    var i: i64 = 0;
    while i < s.len {
        out[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

// Unsigned decimal.
fn gf_put_u64(out: u8[:], at: i64, v: u64) -> i64 {
    if at < 0 {
        return -1;
    }
    var digits: u8[24];
    var n: i64 = 0;
    var x: u64 = v;
    if x == 0 {
        digits[0] = 48;
        n = 1;
    }
    while x > 0 {
        digits[n] = (48 + (x % 10)) as! u8;
        x = x / 10;
        n = n + 1;
    }
    var i: i64 = 0;
    while i < n {
        let r: i64 = gf_put(out, at + i, digits[n - 1 - i]);
        if r < 0 {
            return -1;
        }
        i = i + 1;
    }
    return at + n;
}

fn gf_put_i64(out: u8[:], at: i64, v: i64) -> i64 {
    if v < 0 {
        let r: i64 = gf_put(out, at, 45);
        if r < 0 {
            return -1;
        }
        return gf_put_u64(out, r, (0 - v) as u64);
    }
    return gf_put_u64(out, at, v as u64);
}

// std::to_string(float) is printf("%f"): six decimals, so the value is printed
// as round(v * 1e6) with a decimal point six places from the end.
fn gf_put_f64(out: u8[:], at: i64, v: f64) -> i64 {
    var x: f64 = v;
    var a: i64 = at;
    if x < 0.0 {
        let r: i64 = gf_put(out, a, 45);
        if r < 0 {
            return -1;
        }
        a = r;
        x = 0.0 - x;
    }
    let scaled: f64 = x * 1000000.0 + 0.5;
    if scaled > 9.0e18 {
        return -1;
    }
    let total: u64 = scaled as u64;
    let whole: u64 = total / 1000000;
    let frac: u64 = total % 1000000;
    a = gf_put_u64(out, a, whole);
    if a < 0 {
        return -1;
    }
    a = gf_put(out, a, 46);
    if a < 0 {
        return -1;
    }
    var div: u64 = 100000;
    while div > 0 {
        const d: u64 = (frac / div) % 10;
        a = gf_put(out, a, (48 + d) as! u8);
        if a < 0 {
            return -1;
        }
        div = div / 10;
    }
    return a;
}

fn gf_bits(data: const u8[:], at: i64, n: i64) -> u64 {
    if at < 0 || n < 0 || at + n > data.len {
        return 0;
    }
    var out: u64 = 0;
    var i: i64 = 0;
    while i < n {
        out = out | ((data[at + i] as u64) << (i * 8));
        i = i + 1;
    }
    return out;
}

fn gf_pow2(n: i64) -> f64 {
    var r: f64 = 1.0;
    var i: i64 = 0;
    if n >= 0 {
        while i < n {
            r = r * 2.0;
            i = i + 1;
        }
    } else {
        while i > n {
            r = r / 2.0;
            i = i - 1;
        }
    }
    return r;
}

// IEEE-754 binary32 from its bits, in Goose arithmetic: no host binding, so the
// formatter depends on nothing native.
fn gf_f32_from_bits(bits: u32) -> f64 {
    const sign: i64 = (bits >> 31) as i64;
    const exp: i64 = ((bits >> 23) & 255) as i64;
    const frac: i64 = (bits & 8388607) as i64;
    if exp == 255 {
        return 0.0;
    }
    var v: f64 = 0.0;
    if exp == 0 {
        v = (frac as f64) / 8388608.0 * gf_pow2(-126);
    } else {
        v = (1.0 + (frac as f64) / 8388608.0) * gf_pow2(exp - 127);
    }
    if sign != 0 {
        v = 0.0 - v;
    }
    return v;
}

fn gf_f32(data: const u8[:], at: i64) -> f64 {
    return gf_f32_from_bits(gf_bits(data, at, 4) as! u32);
}

// One scalar value at `at`.
fn gf_scalar(data: const u8[:], t: i64, at: i64, out: u8[:], o: i64) -> i64 {
    if t == GF_U8 {
        return gf_put_u64(out, o, gf_bits(data, at, 1));
    }
    if t == GF_I8 {
        return gf_put_i64(out, o, (gf_bits(data, at, 1) as! u8) as! i8 as i64);
    }
    if t == GF_U16 {
        return gf_put_u64(out, o, gf_bits(data, at, 2));
    }
    if t == GF_I16 {
        return gf_put_i64(out, o, (gf_bits(data, at, 2) as! u16) as! i16 as i64);
    }
    if t == GF_U32 {
        return gf_put_u64(out, o, gf_bits(data, at, 4));
    }
    if t == GF_I32 {
        return gf_put_i64(out, o, (gf_bits(data, at, 4) as! u32) as! i32 as i64);
    }
    if t == GF_U64 {
        return gf_put_u64(out, o, gf_bits(data, at, 8));
    }
    if t == GF_I64 {
        return gf_put_i64(out, o, (gf_bits(data, at, 8) as! u64) as! i64);
    }
    if t == GF_F32 {
        return gf_put_f64(out, o, gf_f32(data, at));
    }
    if t == GF_BOOL {
        if gf_bits(data, at, 1) != 0 {
            return gf_put_lit(out, o, "true");
        }
        return gf_put_lit(out, o, "false");
    }
    return -1;
}

// Payload size of one element of type `t`.
fn gf_elem_size(t: i64) -> i64 {
    if t == GF_U8 || t == GF_I8 || t == GF_BOOL {
        return 1;
    }
    if t == GF_U16 || t == GF_I16 {
        return 2;
    }
    if t == GF_U32 || t == GF_I32 || t == GF_F32 {
        return 4;
    }
    if t == GF_U64 || t == GF_I64 || t == GF_F64 {
        return 8;
    }
    return 0;
}

// Offset of string element `i` of an array whose payload starts at `at`, and its
// length through `out[0]`.
fn gf_str_at(data: const u8[:], at: i64, i: i64, out: i64[:]) -> i64 {
    var p: i64 = at;
    var k: i64 = 0;
    while k < i {
        const n: u64 = gf_bits(data, p, 8);
        if n > 9223372036854775807 {
            return -1;
        }
        p = p + 8 + (n as! i64);
        k = k + 1;
    }
    const len: u64 = gf_bits(data, p, 8);
    if len > 9223372036854775807 {
        return -1;
    }
    out[0] = p + 8;
    out[1] = len as! i64;
    return 0;
}

// Quoted string with the two escapes gguf_kv_to_str performs.
fn gf_put_quoted(data: const u8[:], at: i64, n: i64, out: u8[:], o: i64) -> i64 {
    var a: i64 = gf_put(out, o, 34);
    if a < 0 {
        return -1;
    }
    var i: i64 = 0;
    while i < n {
        const c: u8 = data[at + i];
        if c == 92 || c == 34 {
            a = gf_put(out, a, 92);
            if a < 0 {
                return -1;
            }
        }
        a = gf_put(out, a, c);
        if a < 0 {
            return -1;
        }
        i = i + 1;
    }
    return gf_put(out, a, 34);
}

// The whole value of entry `index`, formatted as gguf_kv_to_str does.
// Returns the length written, or -1 when `out` is too small.
// `max_len` is the caller's cap on the growing text, exactly as gguf_kv_to_str
// takes one: the array loop stops once the written length passes it, so a long
// token table produces a prefix rather than a whole dump.
export fn gr_kv_to_str(data: const u8[:], state: i64[:], kv: i64[:], index: i64, max_len: i64, out: u8[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 {
        return -1;
    }
    const t: i64 = kv[index * 8 + 2];
    const at: i64 = kv[index * 8 + 4];
    if t == GF_STR {
        // A string longer than `out` is written as a prefix: the caller caps the
        // text at max_len anyway, and refusing outright would make a long chat
        // template fail the whole line.
        var n: i64 = kv[index * 8 + 6];
        if n > out.len {
            n = out.len;
        }
        return gf_put_lit(out, 0, data[at .. at + n]);
    }
    if t != GF_ARR {
        return gf_scalar(data, t, at, out, 0);
    }
    const et: i64 = kv[index * 8 + 3];
    const count: i64 = kv[index * 8 + 5];
    var a: i64 = gf_put(out, 0, 91);
    if a < 0 {
        return -1;
    }
    if et == GF_ARR {
        return gf_put_lit(out, a, "???]");
    }
    var span: i64[2];
    var i: i64 = 0;
    while i < count && a <= max_len {
        if i > 0 {
            a = gf_put_lit(out, a, ", ");
            if a < 0 {
                return -1;
            }
        }
        if et == GF_STR {
            if gf_str_at(data, at, i, span[0 .. 2]) != 0 {
                return -1;
            }
            a = gf_put_quoted(data, span[0], span[1], out, a);
        } else {
            a = gf_scalar(data, et, at + i * gf_elem_size(et), out, a);
        }
        if a < 0 {
            return -1;
        }
        i = i + 1;
    }
    // An array that stopped early has no closing bracket, matching the native
    // behaviour when max_len cuts the iteration short.
    if i < count {
        return a;
    }
    return gf_put(out, a, 93);
}
