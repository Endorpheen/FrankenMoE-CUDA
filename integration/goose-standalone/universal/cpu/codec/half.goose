namespace half_codec;

extern "fg143_codec_from_bits" fn half_from_bits(x: u32) -> f32;

fn expand_half(x: u16) -> f32 {
    let bits = x as u32;
    let magnitude = bits & 0x7fff;
    let sign = (bits & 0x8000) << 16;
    if ((magnitude as i64 - 0x400) as !u32) < 0x7800 {
        return half_from_bits(sign | ((magnitude + 0x1c000) << 13));
    }
    if magnitude < 0x400 {
        if magnitude == 0 { return half_from_bits(sign); }
        let value = (magnitude as f32) * (1.0 / 16777216.0);
        return if sign == 0 { value } else { -value };
    }
    let quiet: u32 = if magnitude == 0x7c00 { 0 } else { 0x400000 };
    return half_from_bits(sign | 0x7f800000 | quiet | ((magnitude & 0x3ff) << 13));
}

extern "fg143_bits" fn half_to_bits(x: f32) -> u32;

fn narrow_half(x: f32) -> u16 {
    let bits = half_to_bits(x);
    let magnitude = bits & 0x7fffffff;
    let sign = (bits >> 16) & 0x8000;
    if ((magnitude as i64 - 0x38800000) as !u32) < 0x0efff000 {
        let rounded = magnitude + 0xfff + ((magnitude >> 13) & 1);
        return (sign | ((rounded - 0x38000000) >> 13)) as u16;
    }
    if magnitude >= 0x477ff000 {
        return (sign | (if magnitude > 0x7f800000 { 0x7e00 } else { 0x7c00 })) as u16;
    }
    if magnitude <= 0x33000000 { return sign as u16; }
    let shift = (126 - (magnitude >> 23)) as i64;
    let significand = (magnitude & 0x7fffff) | 0x800000;
    let rounded = significand + ((1 as u32) << (shift - 1)) - 1 + ((significand >> shift) & 1);
    return (sign | (rounded >> shift)) as u16;
}
