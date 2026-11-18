// Pinned Edge0 PLE E4M3fn row decoding. The source engine maps its two
// NaN encodings to signed zero, then applies the one table-wide F32 scale.
// Borrowed byte/output slices are not retained; no host FFI or allocation.

fn ple_value(code: u8) -> f32 {
    let value: i64 = code as i64;
    let exponent: i64 = (value / 8) % 16;
    let mantissa: i64 = value % 8;
    var result: f32 = 0.0;
    if exponent == 15 && mantissa == 7 {
        result = 0.0;
    } else if exponent == 0 {
        result = (mantissa as f32) * 0.001953125;
    } else {
        result = 1.0 + (mantissa as f32) * 0.125;
        var power: i64 = exponent - 7;
        while power > 0 {
            result = result * 2.0;
            power = power - 1;
        }
        while power < 0 {
            result = result * 0.5;
            power = power + 1;
        }
    }
    return if value >= 128 { -result } else { result };
}

export fn e0_ple_supported_scale() -> f32 {
    return 0.00019931793212890625;
}

export fn e0_ple_decode_row(raw: u8[:], scale: f32, out: f32[:]) -> i64 {
    if raw.len != 160 || out.len < 160 || scale != e0_ple_supported_scale() {
        return -1;
    }
    var i: i64 = 0;
    while i < 160 {
        out[i] = ple_value(raw[i]) * scale;
        i = i + 1;
    }
    return 0;
}
