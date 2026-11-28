fn decode_weight(row: u32, column: u32) -> f32 {
    let block = (row * (p.width / 128u) + (column >> 7u)) * 34u;
    let index = column & 127u;
    let code = (read_byte(block + 2u + (index >> 2u)) >> (2u * (index & 3u))) & 3u;
    return (((code as i32) - 1) as f32) * read_half(block);
}
