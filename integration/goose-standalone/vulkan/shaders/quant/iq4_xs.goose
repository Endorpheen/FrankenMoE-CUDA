fn decode_weight(row: u32, column: u32) -> f32 {
    let block = (row * (p.width >> 8u) + (column >> 8u)) * 136u;
    let index = column & 255u;
    let group = index >> 5u;
    let high = read_byte(block + 2u + (group >> 2u)) >> ((group & 3u) * 2u);
    let low = read_byte(block + 4u + (group >> 1u)) >> ((group & 1u) * 4u);
    let scale = ((((high & 3u) << 4u) | (low & 15u)) as i32) - 32;
    let pair = read_byte(block + 8u + group * 16u + (index & 15u));
    let code = (pair >> (((index >> 4u) & 1u) * 4u)) & 15u;
    return (read_half(block) * (scale as f32)) * iq4_value(code);
}
