fn decode_weight(row: u32, column: u32) -> f32 {
    let block = (row * (p.width / 32u) + (column >> 5u)) * 34u;
    let raw = read_byte(block + 2u + (column & 31u));
    let code = ((raw ^ 128u) as i32) - 128;
    return (code as f32) * read_half(block);
}
