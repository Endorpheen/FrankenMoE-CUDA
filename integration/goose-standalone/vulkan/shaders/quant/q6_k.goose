fn decode_weight(row: u32, column: u32) -> f32 {
    let block = (row * (p.width >> 8u) + (column >> 8u)) * 210u;
    let index = column & 255u;
    let half_block = index >> 7u;
    let group = (index >> 5u) & 3u;
    let lane = index & 31u;
    let low = read_byte(block + half_block * 64u + (group & 1u) * 32u + lane);
    let high = read_byte(block + 128u + half_block * 32u + lane);
    let quant = ((low >> ((group >> 1u) * 4u)) & 15u) | (((high >> (group * 2u)) & 3u) << 4u);
    let scale = ((read_byte(block + 192u + (index >> 4u)) ^ 128u) as i32) - 128;
    return (read_half(block + 208u) * (scale as f32)) * (((quant as i32) - 32) as f32);
}
