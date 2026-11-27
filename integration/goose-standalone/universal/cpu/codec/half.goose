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
