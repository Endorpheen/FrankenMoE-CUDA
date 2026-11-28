extern "fg_device_global_load" fn gdn_read_float(address: u64) -> f32;
extern "fg_device_global_store" fn gdn_write_float(address: u64, value: f32);
extern "fg_gdn_read_word" fn gdn_read_word(packet: i64, index: i64) -> u64;
extern "as_float" fn gdn_bits_float(bits: u32) -> f32;

fn gdn_load(address: u64, index: u64) -> f32 {
    return gdn_read_float(address + 4 * index);
}

fn gdn_store(address: u64, index: u64, value: f32) {
    gdn_write_float(address + 4 * index, value);
}

fn gdn_mulhi(a: u32, b: u32) -> u32 {
    return (((a as u64) * (b as u64)) >> 32) as u32;
}

export fn gdn_packet_word(packet: i64, table: i64, index: i64) -> u64 {
    let offset = if table == 0 { 0 } else { if table == 2 { 8 } else { 32 } };
    return gdn_read_word(packet, offset + index);
}

export fn gdn_packet_scale(packet: i64, index: i64) -> f32 {
    return gdn_bits_float(gdn_read_word(packet, 49 + index) as! u32);
}

fn gdn_packet_coefficient(packet: i64, row: i64, index: i64) -> u32 {
    return gdn_read_word(packet, 50 + 3 * row + index) as! u32;
}
