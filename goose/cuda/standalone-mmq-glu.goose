extern "fg_mmq_swiglu_reference" fn pack_swiglu(gate: f32, up: f32) -> f32;

export fn fg_mmq_glu_pack(gate: u64, up: u64, packed: u64, layout: i32, group: i64, lane: i64) {
    let offset = ((128 * group + 4 * lane) * 4) as u64;
    let result = fg180q_quantize4(
        pack_swiglu(ld_f4x(gate, offset), ld_f4x(up, offset)),
        pack_swiglu(ld_f4y(gate, offset), ld_f4y(up, offset)),
        pack_swiglu(ld_f4z(gate, offset), ld_f4z(up, offset)),
        pack_swiglu(ld_f4w(gate, offset), ld_f4w(up, offset)), layout);
    let base = (144 * group) as u64;
    st_i32(packed, base + 16 + ((4 * lane) as u64), result.word);
    if lane % 8 == 0 {
        let scale = base + ((lane / 8 * 4) as u64);
        if layout == 1 || layout == 4 { st_half2(packed, scale, result.scale, result.sum); }
        else { st_f32(packed, scale, result.scale); }
    }
}
