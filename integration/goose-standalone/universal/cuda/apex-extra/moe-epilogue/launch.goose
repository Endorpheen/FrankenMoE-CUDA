export fn apex_moe_epilogue(routed: u64, shared: u64, gate: u64, residual: u64, output: u64,
    vectors: u32, group: u32, thread: u32) {
    let index = group * 128 + thread;
    if index >= vectors { return; }
    let offset = 16 * (index as u64);
    let factor = cv_load(gate + 4 * ((index / 512) as u64));
    let route = cv_load4(routed + offset);
    let expert = cv_load4(shared + offset);
    let skip = cv_load4(residual + offset);
    cv_store4(output + offset, CVVec4 {
        (route.x + cv_mul_rn(expert.x, factor)) + skip.x,
        (route.y + cv_mul_rn(expert.y, factor)) + skip.y,
        (route.z + cv_mul_rn(expert.z, factor)) + skip.z,
        (route.w + cv_mul_rn(expert.w, factor)) + skip.w
    });
}
