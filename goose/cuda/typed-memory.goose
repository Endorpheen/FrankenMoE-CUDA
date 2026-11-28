fn memory_fg_dense_job(p: gpu_ptr<global, const u64>) -> u64 {
    return *p;
}

fn memory_fg248d_ld_u8(p: gpu_ptr<global, u8>) -> u32 {
    return *p as u32;
}

fn memory_fg248d_ld_i8(p: gpu_ptr<global, i8>) -> i32 {
    return *p as i32;
}

fn memory_fg248d_st_f32(p: gpu_ptr<shared, f32>, value: f32) {
    *p = value;
}

fn memory_fg_device_global_load(p: gpu_ptr<global, f32>) -> f32 {
    return *p;
}

fn memory_fg_device_global_store(p: gpu_ptr<global, f32>, value: f32) {
    *p = value;
}

fn memory_fg_device_shared_load(p: gpu_ptr<shared, f32>) -> f32 {
    return *p;
}

fn memory_fg_device_shared_store(p: gpu_ptr<shared, f32>, value: f32) {
    *p = value;
}

fn memory_fg_device_matmul_job_word(p: gpu_ptr<global, const u64>, p_index: u64) -> u64 {
    return p[p_index];
}

fn memory_fg175q_ld_u16(p: gpu_ptr<global, u16>) -> u32 {
    return *p as u32;
}

fn memory_fg_attn_word(p: gpu_ptr<global, i64>, p_index: i64) -> i64 {
    return p[p_index];
}

fn memory_fg_dsa_store_u16(p: gpu_ptr<global, u16>, p_index: i64, value: u32) {
    p[p_index] = value as u16;
}

fn memory_fg_dsa_load_i32(p: gpu_ptr<global, const i32>, p_index: i64) -> i32 {
    return p[p_index];
}

fn memory_fg_mla_mma_kpair(p: gpu_ptr<global, const u32>) -> u32 {
    return *p;
}

fn memory_fg_mla_score_put(p: gpu_ptr<shared, f32>, p_index: i64, value: f32) {
    p[p_index] = value;
}

fn memory_fg_mla_score_get(p: gpu_ptr<shared, const f32>, p_index: i64) -> f32 {
    return p[p_index];
}

fn memory_fg_dsa_top_bits(p: gpu_ptr<global, const u32>, p_index: i64) -> u32 {
    return p[p_index];
}

fn memory_fg_dsa_bins_clear(p: gpu_ptr<shared, u32>, p_index: i64) {
    p[p_index] = 0u;
}

fn memory_fg_dsa_bins_get(p: gpu_ptr<shared, const u32>, p_index: i64) -> u32 {
    return p[p_index];
}

fn memory_fg_dsa_top_put_key(p: gpu_ptr<shared, u64>, p_index: i64, value: u64) {
    p[p_index] = value;
}

fn memory_fg_dsa_top_get_key(p: gpu_ptr<shared, const u64>, p_index: i64) -> u64 {
    return p[p_index];
}

fn memory_fg_dsa_top_store(p: gpu_ptr<global, u32>, p_index: i64, value: u32) {
    p[p_index] = value;
}

fn memory_fg_mla_store_index(p: gpu_ptr<global, i32>, p_index: i64, value: i32) {
    p[p_index] = value;
}

fn memory_fg176r_st_u8(p: gpu_ptr<global, u8>, value: u32) {
    *p = value as u8;
}

fn memory_fg_rope_position(p: gpu_ptr<global, i32>, p_index: i64) -> i32 {
    return p[p_index];
}

fn memory_fg_rope_copy32(p: gpu_ptr<global, u32>, source: gpu_ptr<global, u32>) {
    *p = *source;
}

fn memory_fg_rope_copy16(p: gpu_ptr<global, u16>, source: gpu_ptr<global, u16>) {
    *p = *source;
}

fn memory_fg170s_shared_load(p: gpu_ptr<shared, f32>, p_index: u64) -> f32 {
    return p[p_index];
}

fn memory_fg170s_shared_store(p: gpu_ptr<shared, f32>, p_index: u64, value: f32) {
    p[p_index] = value;
}

fn memory_fg283d_st_i32(p: gpu_ptr<global, i32>, value: i32) {
    *p = value;
}

fn memory_fg283d_sload_i32(p: gpu_ptr<shared, i32>) -> i32 {
    return *p;
}

fn memory_fg283d_sstore_i32(p: gpu_ptr<shared, i32>, value: i32) {
    *p = value;
}

fn memory_fg_device_copy_u32(p: gpu_ptr<global, u32>, source: gpu_ptr<global, u32>) {
    *p = *source;
}

fn memory_fg183f_ld_f32(p: gpu_ptr<global, const f32>) -> f32 {
    return *p;
}

fn memory_apex_up_gate_word(p: gpu_ptr<global, const u64>, p_index: i64) -> u64 {
    return p[p_index];
}

fn memory_fg_flash_mask_half(p: gpu_ptr<shared, u16>, value: u32) {
    *p = value as u16;
}

fn memory_fg175c_fg176i_ldu(p: gpu_ptr<shared, u32>) -> u32 {
    return *p;
}

fn memory_fg175c_fg176i_stu(p: gpu_ptr<shared, u32>, value: u32) {
    *p = value;
}

fn memory_fg175c_fg175p_smld(p: gpu_ptr<shared, u32>, p_index: u64) -> u32 {
    return p[p_index];
}

fn memory_fg175c_fg175p_smst(p: gpu_ptr<shared, u32>, p_index: u64, value: u32) {
    p[p_index] = value;
}

fn memory_fg175c_fg175p_smldf(p: gpu_ptr<shared, f32>, p_index: u64) -> f32 {
    return p[p_index];
}

fn memory_fg175c_fg175p_smstf(p: gpu_ptr<shared, f32>, p_index: u64, value: f32) {
    p[p_index] = value;
}

fn memory_fg175v_ld_i32(p: gpu_ptr<global, i32>) -> i32 {
    return *p;
}

fn memory_fg175v_st_i32(p: gpu_ptr<global, i32>, value: i32) {
    *p = value;
}

fn memory_fg175v_sm_f32(p: gpu_ptr<shared, f32>, p_index: u64) -> f32 {
    return p[p_index];
}

fn memory_fg175v_sm_st_f32(p: gpu_ptr<shared, f32>, p_index: u64, value: f32) {
    p[p_index] = value;
}

fn memory_fg175v_sm_i32(p: gpu_ptr<shared, i32>, p_index: u64) -> i32 {
    return p[p_index];
}

fn memory_fg175v_sm_st_i32(p: gpu_ptr<shared, i32>, p_index: u64, value: i32) {
    p[p_index] = value;
}

fn memory_flash_vector_plan_word(p: gpu_ptr<global, const i64>, p_index: i32) -> i64 {
    return p[p_index];
}

fn memory_fg174m_sm_store_u32(p: gpu_ptr<shared, u32>, p_index: u64, value: u32) {
    p[p_index] = value;
}

fn memory_fg174m_sm_f32(p: gpu_ptr<shared, f32>, p_index: u64) -> f32 {
    return p[p_index];
}

fn memory_fg174m_sm_store_f32(p: gpu_ptr<shared, f32>, p_index: u64, value: f32) {
    p[p_index] = value;
}

fn memory_fg_mmq_aligned_word(p: gpu_ptr<global, u32>) -> u32 {
    return *p;
}

fn memory_fg_mmq_half_word(p: gpu_ptr<global, u16>) -> u32 {
    return *p as u32;
}

fn memory_fg_mmq_index(p: gpu_ptr<global, const u64>, p_index: i64) -> u64 {
    return p[p_index];
}

fn memory_fg_mmq_shared_store(p: gpu_ptr<shared, u32>, value: u32) {
    *p = value;
}

fn memory_fg_mmq_global_store_word(p: gpu_ptr<global, u32>, value: u32) {
    *p = value;
}

fn memory_fg_mmq_shared_float(p: gpu_ptr<shared, f32>) -> f32 {
    return *p;
}

fn memory_fg_mmq_shared_i8(p: gpu_ptr<shared, i8>) -> i32 {
    return *p as i32;
}

fn memory_fg_mmq_shared_store_float(p: gpu_ptr<shared, f32>, value: f32) {
    *p = value;
}

fn memory_fg_mmq_shared_word(p: gpu_ptr<shared, u32>) -> u32 {
    return *p;
}

fn memory_fg180q_ld_i32(p: gpu_ptr<global, i32>) -> i64 {
    return *p as i64;
}

fn memory_fg180q_st_i32(p: gpu_ptr<global, i32>, value: i32) {
    *p = value;
}

fn memory_fg185q_ld_i32(p: gpu_ptr<global, i32>) -> i32 {
    return *p;
}

fn memory_fg185q_ld_u16(p: gpu_ptr<global, u16>) -> u32 {
    return *p as u32;
}

fn memory_fg178q_st_i8(p: gpu_ptr<global, u8>, value: i32) {
    *p = value as u8;
}

fn memory_mmvq_job_word(p: gpu_ptr<global, const u64>, p_index: u64) -> u64 {
    return p[p_index];
}

fn memory_fg_rms_shared_load(p: gpu_ptr<shared, f32>, p_index: u64) -> f32 {
    return p[p_index];
}

fn memory_fg_rms_shared_store(p: gpu_ptr<shared, f32>, p_index: u64, value: f32) {
    p[p_index] = value;
}

fn memory_fg_dsa_int_word(ptr: gpu_ptr<global, const u32>) -> u32 {
    return *ptr;
}

fn memory_fg_dsa_pack_byte(ptr: gpu_ptr<global, i8>, value: i32) {
    *ptr = value as i8;
}

fn memory_i2d_load_f32(ptr: gpu_ptr<global, const f32>, index: i64) -> f32 {
    return ptr[index];
}

fn memory_i2d_load_i64(ptr: gpu_ptr<global, const i64>, index: i64) -> i64 {
    return ptr[index];
}

fn memory_i2d_store_f32(ptr: gpu_ptr<global, f32>, index: i64, value: f32) {
    ptr[index] = value;
}

fn memory_i2d_shared_f32(ptr: gpu_ptr<shared, f32>, index: i64) -> f32 {
    return ptr[index];
}

fn memory_i2d_set_shared_f32(ptr: gpu_ptr<shared, f32>, index: i64, value: f32) {
    ptr[index] = value;
}

fn memory_i2d_shared_f64(ptr: gpu_ptr<shared, f64>, index: i64) -> f64 {
    return ptr[index];
}

fn memory_i2d_set_shared_f64(ptr: gpu_ptr<shared, f64>, index: i64, value: f64) {
    ptr[index] = value;
}

fn memory_i2d_store_f64(ptr: gpu_ptr<global, f64>, index: i64, value: f64) {
    ptr[index] = value;
}

fn memory_i2d_load_f64(ptr: gpu_ptr<global, const f64>, index: i64) -> f64 {
    return ptr[index];
}

fn memory_input_load(ptr: gpu_ptr<global, const f32>, index: i64) -> f32 {
    return ptr[index];
}

fn memory_input_store(ptr: gpu_ptr<global, f32>, index: i64, value: f32) {
    ptr[index] = value;
}

fn memory_input_sum(ptr: gpu_ptr<shared, const f64>, index: i64) -> f64 {
    return ptr[index];
}

fn memory_input_set_sum(ptr: gpu_ptr<shared, f64>, index: i64, value: f64) {
    ptr[index] = value;
}

fn memory_i2o_load(ptr: gpu_ptr<global, f32>, index: i64) -> f32 {
    return ptr[index];
}

fn memory_i2o_position(ptr: gpu_ptr<global, i64>, index: i64) -> i64 {
    return ptr[index];
}

fn memory_i2o_shared_read(ptr: gpu_ptr<shared, f32>, index: i64) -> f32 {
    return ptr[index];
}

fn memory_i2o_shared_write(ptr: gpu_ptr<shared, f32>, index: i64, value: f32) {
    ptr[index] = value;
}

fn memory_i2_flash_load_byte(ptr: gpu_ptr<global, const u8>) -> u32 {
    return *ptr as u32;
}

fn memory_i2_flash_load_half_bits(ptr: gpu_ptr<global, const u16>) -> u32 {
    return *ptr as u32;
}

fn memory_i2_native_store(ptr: gpu_ptr<global, f32>, index: i64, value: f32) {
    ptr[index] = value;
}

fn memory_moe_word(ptr: gpu_ptr<global, const u64>, index: i64) -> u64 {
    return ptr[index];
}

fn memory_moe_store(ptr: gpu_ptr<global, f32>, index: i64, value: f32) {
    ptr[index] = value;
}

fn memory_lazy_load(ptr: gpu_ptr<global, const f32>, index: i64) -> f32 {
    return ptr[index];
}

fn memory_lazy_store(ptr: gpu_ptr<global, f32>, index: i64, value: f32) {
    ptr[index] = value;
}

fn memory_lazy_plane(ptr: gpu_ptr<global, const u64>, index: i64) -> u64 {
    return ptr[index];
}

fn memory_lazy_local_read(ptr: gpu_ptr<shared, const f32>, index: i64) -> f32 {
    return ptr[index];
}

fn memory_lazy_local_write(ptr: gpu_ptr<shared, f32>, index: i64, value: f32) {
    ptr[index] = value;
}

fn memory_lazy_sum_read(ptr: gpu_ptr<shared, const f64>, index: i64) -> f64 {
    return ptr[index];
}

fn memory_lazy_sum_write(ptr: gpu_ptr<shared, f64>, index: i64, value: f64) {
    ptr[index] = value;
}

fn memory_i2m_load_u8(ptr: gpu_ptr<global, const u8>) -> u32 {
    return *ptr as u32;
}

fn memory_i2m_load_i8(ptr: gpu_ptr<global, const i8>) -> i32 {
    return *ptr as i32;
}

fn memory_i2m_load_u16(ptr: gpu_ptr<global, const u16>) -> u32 {
    return *ptr as u32;
}

fn memory_i2m_load(ptr: gpu_ptr<global, const f32>) -> f32 {
    return *ptr;
}

fn memory_i2m_store(ptr: gpu_ptr<global, f32>, value: f32) {
    *ptr = value;
}

fn memory_i2m_job_word(ptr: gpu_ptr<global, const u64>, index: i64) -> u64 {
    return ptr[index];
}

fn memory_q8_load_f32(ptr: gpu_ptr<global, const f32>, index: i64) -> f32 {
    return ptr[index];
}

fn memory_q8_job_word(ptr: gpu_ptr<global, const u64>, index: i64) -> u64 {
    return ptr[index];
}

fn memory_q8_store_f32(ptr: gpu_ptr<global, f32>, value: f32) {
    *ptr = value;
}

fn memory_i2_epilogue_read(ptr: gpu_ptr<global, const f32>, index: i64) -> f32 {
    return ptr[index];
}

fn memory_i2_epilogue_write(ptr: gpu_ptr<global, f32>, index: i64, value: f32) {
    ptr[index] = value;
}

fn memory_pred_load(ptr: gpu_ptr<global, const f32>, index: i64) -> f32 {
    return ptr[index];
}

fn memory_pred_shared_load(ptr: gpu_ptr<shared, f32>, index: i32) -> f32 {
    return ptr[index];
}

fn memory_pred_shared_store(ptr: gpu_ptr<shared, f32>, index: i32, value: f32) {
    ptr[index] = value;
}

fn memory_pred_save_last(ptr: gpu_ptr<shared, i32>, value: i32) {
    *ptr = value;
}

fn memory_pred_load_last(ptr: gpu_ptr<shared, i32>) -> i32 {
    return *ptr;
}

fn memory_route_raw(ptr: gpu_ptr<global, f32>, index: i32) -> f32 {
    return ptr[index];
}

fn memory_route_soft_store(ptr: gpu_ptr<global, f32>, index: i32, value: f32) {
    ptr[index] = value;
}

fn memory_route_score(ptr: gpu_ptr<global, const f32>, index: i32) -> f32 {
    return ptr[index];
}

fn memory_route_write_score(ptr: gpu_ptr<global, f32>, index: i32, value: f32) {
    ptr[index] = value;
}

fn memory_route_write_id(ptr: gpu_ptr<global, i32>, index: i32, value: i32) {
    ptr[index] = value;
}
