fn iq4_value(code: u32) -> f32 {
    let word = code >> 2u;
    var packed_levels = 0xbfad9881u;
    if word == 1u { packed_levels = 0xf6eaddcfu; }
    if word == 2u { packed_levels = 0x26190d01u; }
    if word == 3u { packed_levels = 0x71594535u; }
    let bits = (packed_levels >> ((code & 3u) * 8u)) & 255u;
    return (((bits ^ 128u) as i32) - 128) as f32;
}
