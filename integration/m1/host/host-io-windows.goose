extern fn host_runtime_recv(fd: u64, data: u64, size: i32, flags: i32) -> i32;
extern fn host_runtime_send(fd: u64, data: u64, size: i32, flags: i32) -> i32;
extern fn host_runtime_close(fd: u64) -> i32;

export fn qz_rd(fd: i64, buffer: u64, bytes: i64) -> i64 {
    if bytes < 0 || bytes > 2147483647 { return -1; }
    return host_runtime_recv(fd as! u64, buffer, bytes as! i32, 0) as! i64;
}

export fn qz_wr(fd: i64, buffer: u64, bytes: i64) -> i64 {
    if bytes < 0 || bytes > 2147483647 { return -1; }
    return host_runtime_send(fd as! u64, buffer, bytes as! i32, 0) as! i64;
}

export fn qz_cl(fd: i64) -> i64 { return host_runtime_close(fd as! u64) as! i64; }

extern fn host_win_aligned_malloc(bytes: u64, alignment: u64) -> u64;
extern fn host_win_aligned_free(address: u64);

export fn ggml_aligned_malloc(bytes: u64) -> u64 { return host_win_aligned_malloc(bytes, 64); }

export fn ggml_aligned_free(address: u64, bytes: u64) { host_win_aligned_free(address); }

export fn ggml_print_backtrace() {}

extern fn ggml_crt_write(fd: i32, address: u64, bytes: u32) -> i32;

fn ggml_stderr_write(address: u64, bytes: u64) {
    var written: u64 = 0;
    while written < bytes {
        let left = bytes - written;
        let chunk: u32 = if left > 2147483647 { 2147483647 } else { left as! u32 };
        let count = ggml_crt_write(2, address + written, chunk);
        if count <= 0 { return; }
        written += count as! u64;
    }
}
