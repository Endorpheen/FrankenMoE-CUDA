fn decode_weight(row: u32, column: u32) -> f32 {
    let block = (row * (p.width / 32u) + (column >> 5u)) * 18u;
    let index = column & 31u;
    let raw = read_byte(block + 2u + (index & 15u));
    let nibble = (raw >> ((index >> 4u) * 4u)) & 15u;
    return (((nibble as i32) - 8) as f32) * read_half(block);
}
