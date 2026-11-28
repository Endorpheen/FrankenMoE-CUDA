extern fn host_runtime_read(fd: i32, data: u64, bytes: u64) -> i64;
extern fn host_runtime_write(fd: i32, data: u64, bytes: u64) -> i64;
extern fn host_runtime_close(fd: i32) -> i32;

export fn qz_rd(fd: i64, buffer: u64, bytes: i64) -> i64 {
    return host_runtime_read(fd as! i32, buffer, bytes as! u64);
}

export fn qz_wr(fd: i64, buffer: u64, bytes: i64) -> i64 {
    return host_runtime_write(fd as! i32, buffer, bytes as! u64);
}

export fn qz_cl(fd: i64) -> i64 { return host_runtime_close(fd as! i32) as! i64; }

extern fn host_posix_memalign(output: c_ptr<u64>, alignment: u64, bytes: u64) -> i32;
extern fn host_pool_free(address: c_ptr<void>);

export fn ggml_aligned_malloc(bytes: u64) -> u64 {
    if bytes == 0 {
        common_memory_error("Behavior may be unexpected when allocating 0 bytes for ggml_aligned_malloc!\n");
        return 0;
    }
    var address: u64 = 0;
    let status = host_posix_memalign(unsafe_c_address<c_ptr<u64>>(address), 64, bytes);
    if status == 0 { return address; }
    let error = if status == 22 { "invalid alignment value" } else {
        if status == 12 { "insufficient memory" } else { "unknown allocation error" }
    };
    common_memory_error(str("ggml_aligned_malloc: ", error, " (attempted to allocate ", bytes as! i64, " bytes)\n"));
    return 0;
}

export fn ggml_aligned_free(address: u64, bytes: u64) {
    host_pool_free(unsafe_c_ptr<c_ptr<void>>(address));
}
