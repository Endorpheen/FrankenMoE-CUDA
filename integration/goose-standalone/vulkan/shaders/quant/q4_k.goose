fn decode_weight(row: u32, column: u32) -> f32 {
    let block = (row * (p.width >> 8u) + (column >> 8u)) * 144u;
    let index = column & 255u;
    let group = index >> 5u;
    let scales = block + 4u;
    var d: u32;
    var m: u32;
    if group < 4u {
        d = read_byte(scales + group) & 63u;
        m = read_byte(scales + group + 4u) & 63u;
    } else {
        let lower = read_byte(scales + group + 4u);
        d = (lower & 15u) | ((read_byte(scales + group - 4u) >> 6u) << 4u);
        m = (lower >> 4u) | ((read_byte(scales + group) >> 6u) << 4u);
    }
    let quants = read_byte(block + 16u + (index >> 6u) * 32u + (index & 31u));
    let quant = (quants >> ((group & 1u) * 4u)) & 15u;
    precise let decoded = (read_half(block) * (d as f32)) * (quant as f32) - read_half(block + 2u) * (m as f32);
    return decoded;
}
