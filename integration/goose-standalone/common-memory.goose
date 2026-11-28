extern fn pthread_mutex_init(lock: c_ptr<void>, attributes: c_ptr<const void>) -> i32;
extern fn pthread_mutex_lock(lock: c_ptr<void>) -> i32;
extern fn pthread_mutex_unlock(lock: c_ptr<void>) -> i32;
extern "abort" fn common_memory_abort();
extern "gs_os_write_stderr" fn common_memory_error(text: const u8[:]);

var ggml_critical_lock: u64[8];

export fn fg_common_memory_init() -> i32 {
    return pthread_mutex_init(unsafe_c_address<c_ptr<void>>(ggml_critical_lock[0]), null);
}

export fn ggml_critical_section_start() {
    pthread_mutex_lock(unsafe_c_address<c_ptr<void>>(ggml_critical_lock[0]));
}

export fn ggml_critical_section_end() {
    pthread_mutex_unlock(unsafe_c_address<c_ptr<void>>(ggml_critical_lock[0]));
}

export fn slru_ld_i32(base: u64, byte: i64) -> i64 {
    return builder_i32(((base + (byte as! u64)) as! i64)) as! i64;
}

export fn slru_st_i32(base: u64, byte: i64, value: i64) {
    builder_write_i32((base + (byte as! u64)) as! i64, value);
}

export fn slru_fail(code: i64) {
    common_memory_error(str("SLRU validation code ", code, "\n"));
    common_memory_abort();
}
